// Direct2D-on-Cairo and WIC-on-gdk-pixbuf behaviour the app depends on,
// including regressions found rendering tests/fixtures headless:
//   - image DPI comes from the file (a 300 DPI PNG keeps its intrinsic size)
//   - asking a frame for its DPI after a scaled decode does not decode again
//   - decode-at-size returns exactly the requested size (JPEG DCT path and
//     the Cairo path) with the right colours
//   - bitmaps drawn far below their size (halved levels) match a direct
//     downscale and follow later pixel changes
#include "compat_internal.h"

#include <d2d1_1.h>
#include <documenttarget.h>
#include <shlobj.h>
#include <wincodec.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <unistd.h>
#include <zlib.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    failures++;
}

std::string tempPath(const char* name) {
    const char* dir = std::getenv("TMPDIR");
    return std::string(dir && *dir ? dir : "/tmp") + "/tinta-d2d-wic-" + std::to_string(getpid()) + "-" + name;
}

// Left half red, right half blue
GdkPixbuf* twoColour(int width, int height) {
    GdkPixbuf* p = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    guchar* px = gdk_pixbuf_get_pixels(p);
    int stride = gdk_pixbuf_get_rowstride(p);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            guchar* o = px + y * stride + x * 3;
            bool left = x < width / 2;
            o[0] = left ? 220 : 20;
            o[1] = 20;
            o[2] = left ? 20 : 220;
        }
    }
    return p;
}

// BGRA of pixel (x, y) from a PBGRA source
std::vector<unsigned char> pixelAt(IWICBitmapSource* source, int x, int y) {
    std::vector<unsigned char> px(4);
    WICRect r{x, y, 1, 1};
    source->CopyPixels(&r, 4, 4, px.data());
    return px;
}

bool isRed(const std::vector<unsigned char>& p) { return p[2] > 180 && p[0] < 60; }
bool isBlue(const std::vector<unsigned char>& p) { return p[0] > 180 && p[2] < 60; }

void testDecoding(IWICImagingFactory* wic) {
    // 300 DPI PNG: the frame reports the file's DPI
    std::string png = tempPath("dpi.png");
    GdkPixbuf* small = twoColour(300, 150);
    check(gdk_pixbuf_save(small, png.c_str(), "png", nullptr, "x-dpi", "300", "y-dpi", "300", nullptr),
          "write 300 DPI PNG");
    g_object_unref(small);
    std::wstring wpng = tinta::toWide(png);
    IWICBitmapDecoder* decoder = nullptr;
    check(SUCCEEDED(wic->CreateDecoderFromFilename(wpng.c_str(), nullptr, GENERIC_READ,
                                                   WICDecodeMetadataCacheOnDemand, &decoder)),
          "PNG decoder");
    IWICBitmapFrameDecode* frame = nullptr;
    decoder->GetFrame(0, &frame);
    UINT w = 0, h = 0;
    frame->GetSize(&w, &h);
    check(w == 300 && h == 150, "PNG header size");
    double dpiX = 0, dpiY = 0;
    frame->GetResolution(&dpiX, &dpiY);
    check(std::fabs(dpiX - 300) < 0.5 && std::fabs(dpiY - 300) < 0.5,
          "PNG DPI from pHYs: " + std::to_string(dpiX) + "x" + std::to_string(dpiY));
    frame->Release();
    decoder->Release();
    std::remove(png.c_str());

    // Large JPEG through a scaler, as image_loader.cpp does it
    std::string jpg = tempPath("large.jpg");
    GdkPixbuf* large = twoColour(4000, 3000);
    check(gdk_pixbuf_save(large, jpg.c_str(), "jpeg", nullptr, "quality", "90", nullptr), "write JPEG");
    g_object_unref(large);
    std::wstring wjpg = tinta::toWide(jpg);
    struct Case {
        UINT w, h;
        const char* path;
    } cases[] = {{1000, 750, "exact 1/4 (DCT only)"}, {900, 675, "DCT 1/4 then Cairo"}, {3500, 2625, "full + Cairo"}};
    for (const Case& c : cases) {
        wic->CreateDecoderFromFilename(wjpg.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
        check(decoder != nullptr, "JPEG decoder");
        if (!decoder) return;
        decoder->GetFrame(0, &frame);
        IWICBitmapScaler* scaler = nullptr;
        wic->CreateBitmapScaler(&scaler);
        int before = tinta::wicDecodeCount();
        check(SUCCEEDED(scaler->Initialize(frame, c.w, c.h, WICBitmapInterpolationModeFant)),
              std::string("scaler ") + c.path);
        scaler->GetSize(&w, &h);
        check(w == c.w && h == c.h, std::string("scaled size exact, ") + c.path + ": " + std::to_string(w) + "x" +
                                        std::to_string(h));
        check(isRed(pixelAt(scaler, static_cast<int>(c.w) / 4, static_cast<int>(c.h) / 2)),
              std::string("left stays red, ") + c.path);
        check(isBlue(pixelAt(scaler, static_cast<int>(c.w) * 3 / 4, static_cast<int>(c.h) / 2)),
              std::string("right stays blue, ") + c.path);
        frame->GetResolution(&dpiX, &dpiY);
        check(tinta::wicDecodeCount() - before == 1,
              std::string("one decode per scaled frame, DPI included, ") + c.path + ": " +
                  std::to_string(tinta::wicDecodeCount() - before));
        scaler->Release();
        frame->Release();
        decoder->Release();
    }
    std::remove(jpg.c_str());

    // Unreadable and missing files fail the way WIC does
    std::string broken = tempPath("broken.jpg");
    FILE* f = std::fopen(broken.c_str(), "wb");
    std::fputs("not an image", f);
    std::fclose(f);
    std::wstring wbroken = tinta::toWide(broken);
    decoder = nullptr;
    check(wic->CreateDecoderFromFilename(wbroken.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
                                         &decoder) == WINCODEC_ERR_UNKNOWNIMAGEFORMAT,
          "broken image is an unknown format");
    std::remove(broken.c_str());
    check(wic->CreateDecoderFromFilename(L"/nonexistent/tinta.png", nullptr, GENERIC_READ,
                                         WICDecodeMetadataCacheOnDemand,
                                         &decoder) == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
          "missing image is file-not-found");
}

// The PDF's bytes with every Flate stream inflated in place
std::string pdfText(const std::vector<unsigned char>& bytes) {
    std::string raw(bytes.begin(), bytes.end()), out;
    size_t at = 0;
    while (true) {
        size_t start = raw.find("stream", at);
        if (start == std::string::npos) break;
        if (start >= 3 && raw.compare(start - 3, 3, "end") == 0) {  // "endstream" is not a stream start
            out.append(raw, at, start + 6 - at);
            at = start + 6;
            continue;
        }
        size_t data = start + 6;
        if (data < raw.size() && raw[data] == '\r') data++;
        if (data < raw.size() && raw[data] == '\n') data++;
        size_t end = raw.find("endstream", data);
        if (end == std::string::npos) break;
        out.append(raw, at, data - at);
        std::vector<unsigned char> inflated(1 << 20);
        uLongf length = inflated.size();
        if (uncompress(inflated.data(), &length, reinterpret_cast<const Bytef*>(raw.data() + data), end - data) ==
            Z_OK) {
            out.append(reinterpret_cast<char*>(inflated.data()), length);
        }
        at = end;
    }
    out.append(raw, at, std::string::npos);
    return out;
}

// Average of a w x h PBGRA block's blue channel
double meanBlue(const unsigned char* data, int stride, int x0, int y0, int w, int h) {
    double sum = 0;
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) sum += data[y * stride + x * 4];
    return sum / (w * h);
}

void testDrawing(ID2D1Factory1* factory, IWICImagingFactory* wic) {
    IWICBitmap* canvas = nullptr;
    wic->CreateBitmap(256, 256, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &canvas);
    ID2D1RenderTarget* target = nullptr;
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
    check(SUCCEEDED(factory->CreateWicBitmapRenderTarget(canvas, &props, &target)), "WIC render target");
    if (!target) return;

    // 1-pixel checkerboard, 2048 square: drawn at 256 it averages to grey,
    // through three halved levels
    const int n = 2048;
    std::vector<unsigned char> checker(static_cast<size_t>(n) * n * 4);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            unsigned char v = ((x + y) & 1) ? 255 : 0;
            unsigned char* p = &checker[(static_cast<size_t>(y) * n + x) * 4];
            p[0] = p[1] = p[2] = v;
            p[3] = 255;
        }
    ID2D1Bitmap* bitmap = nullptr;
    D2D1_BITMAP_PROPERTIES bp = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    target->CreateBitmap(D2D1::SizeU(n, n), checker.data(), n * 4, &bp, &bitmap);
    D2D1_RECT_F dest = D2D1::RectF(0, 0, 256, 256);
    auto draw = [&] {
        target->BeginDraw();
        D2D1_COLOR_F red = D2D1::ColorF(1, 0, 0);
        target->Clear(&red);
        target->DrawBitmap(bitmap, &dest, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, nullptr);
        target->EndDraw();
    };
    draw();
    IWICBitmapLock* lock = nullptr;
    canvas->Lock(nullptr, 0, &lock);
    UINT size = 0, stride = 0;
    BYTE* data = nullptr;
    lock->GetDataPointer(&size, &data);
    lock->GetStride(&stride);
    double grey = meanBlue(data, static_cast<int>(stride), 64, 64, 128, 128);
    check(std::fabs(grey - 127.5) < 12, "checkerboard drawn small averages to grey: " + std::to_string(grey));
    lock->Release();

    // New pixels after a draw must show: the halved copies follow the bitmap
    std::vector<unsigned char> white(static_cast<size_t>(n) * n * 4, 255);
    bitmap->CopyFromMemory(nullptr, white.data(), n * 4);
    draw();
    canvas->Lock(nullptr, 0, &lock);
    lock->GetDataPointer(&size, &data);
    double after = meanBlue(data, static_cast<int>(stride), 64, 64, 128, 128);
    check(after > 250, "redraw after CopyFromMemory shows the new pixels: " + std::to_string(after));
    lock->Release();

    bitmap->Release();
    target->Release();
    canvas->Release();
}

void testGeometryAndPdf(ID2D1Factory1* factory) {
    // Clockwise half circle from (100,50) to (0,50) bulges down to y = 100
    ID2D1PathGeometry* path = nullptr;
    factory->CreatePathGeometry(&path);
    ID2D1GeometrySink* sink = nullptr;
    path->Open(&sink);
    sink->BeginFigure(D2D1::Point2F(100, 50), D2D1_FIGURE_BEGIN_FILLED);
    D2D1_ARC_SEGMENT arc{D2D1::Point2F(0, 50), D2D1::SizeF(50, 50), 0, D2D1_SWEEP_DIRECTION_CLOCKWISE,
                         D2D1_ARC_SIZE_SMALL};
    sink->AddArc(&arc);
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    sink->Release();
    D2D1_RECT_F bounds{};
    path->GetBounds(nullptr, &bounds);
    check(std::fabs(bounds.left) < 0.5 && std::fabs(bounds.right - 100) < 0.5 && std::fabs(bounds.top - 50) < 0.5 &&
              std::fabs(bounds.bottom - 100) < 0.5,
          "half-circle arc bounds: " + std::to_string(bounds.left) + "," + std::to_string(bounds.top) + " " +
              std::to_string(bounds.right) + "," + std::to_string(bounds.bottom));
    path->Release();

    // One recorded page through a print control becomes a PDF in the stream
    ID2D1HwndRenderTarget* window = nullptr;
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(nullptr, D2D1::SizeU(64, 64));
    factory->CreateHwndRenderTarget(&props, &hwndProps, &window);
    ID2D1DeviceContext* context = nullptr;
    window->QueryInterface(__uuidof(ID2D1DeviceContext), reinterpret_cast<void**>(&context));
    ID2D1CommandList* page = nullptr;
    context->CreateCommandList(&page);
    context->SetTarget(page);
    context->BeginDraw();
    D2D1_COLOR_F white = D2D1::ColorF(1, 1, 1);
    context->Clear(&white);  // as print.cpp starts every page
    ID2D1SolidColorBrush* brush = nullptr;
    D2D1_COLOR_F black = D2D1::ColorF(0, 0, 0);
    context->CreateSolidColorBrush(&black, nullptr, &brush);
    D2D1_RECT_F box = D2D1::RectF(96, 96, 192, 192);
    context->FillRectangle(&box, brush);
    // A 400 px bitmap in a 100 DIP box: the PDF keeps all 400 pixels (screen
    // drawing would use a halved level; paper needs the full resolution)
    std::vector<unsigned char> pixels(400 * 400 * 4, 0x80);
    ID2D1Bitmap* photo = nullptr;
    D2D1_BITMAP_PROPERTIES photoProps = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    context->CreateBitmap(D2D1::SizeU(400, 400), pixels.data(), 400 * 4, &photoProps, &photo);
    D2D1_RECT_F photoBox = D2D1::RectF(300, 96, 400, 196);
    if (photo) context->DrawBitmap(photo, &photoBox, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, nullptr);
    context->EndDraw();
    if (photo) photo->Release();
    page->Close();
    context->SetTarget(nullptr);

    IStream* pdf = SHCreateMemStream(nullptr, 0);
    IPrintDocumentPackageTargetFactory* targets = nullptr;
    CoCreateInstance(__uuidof(PrintDocumentPackageTargetFactory), nullptr, CLSCTX_INPROC_SERVER,
                     __uuidof(IPrintDocumentPackageTargetFactory), reinterpret_cast<void**>(&targets));
    IPrintDocumentPackageTarget* job = nullptr;
    check(targets && SUCCEEDED(targets->CreateDocumentPackageTargetForPrintJob(L"PDF", L"test", pdf, nullptr, &job)),
          "print job on a stream");
    ID2D1Device* device = nullptr;
    context->GetDevice(&device);
    ID2D1PrintControl* control = nullptr;
    check(job && SUCCEEDED(device->CreatePrintControl(nullptr, job, nullptr, &control)), "print control");
    if (control) {
        check(SUCCEEDED(control->AddPage(page, D2D1::SizeF(816, 1056), nullptr, nullptr, nullptr)), "add page");
        // print.cpp releases each page before closing: the control must keep it
        page->Release();
        page = nullptr;
        check(SUCCEEDED(control->Close()), "close print control");
        std::vector<unsigned char> bytes;
        tinta::memoryStreamBytes(pdf, bytes);
        check(bytes.size() > 200 && std::string(bytes.begin(), bytes.begin() + 5) == "%PDF-",
              "PDF bytes: " + std::to_string(bytes.size()));
        // 816 x 1056 DIPs is US Letter: 612 x 792 points. Cairo keeps page
        // objects in compressed object streams, so look inside those too
        std::string text = pdfText(bytes);
        check(text.find("/MediaBox [ 0 0 612 792 ]") != std::string::npos, "Letter page box in points");
        check(text.find("/Width 400") != std::string::npos && text.find("/Height 400") != std::string::npos,
              "a shrunk bitmap keeps its full 400x400 pixels in the PDF");
        control->Release();
    }
    if (device) device->Release();
    if (job) job->Release();
    if (targets) targets->Release();
    pdf->Release();
    brush->Release();
    if (page) page->Release();
    context->Release();
    window->Release();
}

}  // namespace

int main() {
    IWICImagingFactory* wic = nullptr;
    check(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                     __uuidof(IWICImagingFactory), reinterpret_cast<void**>(&wic))),
          "WIC factory");
    ID2D1Factory1* factory = nullptr;
    check(SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), nullptr,
                                      reinterpret_cast<void**>(&factory))),
          "D2D factory");
    if (!wic || !factory) return 1;

    testDecoding(wic);
    testDrawing(factory, wic);
    testGeometryAndPdf(factory);

    if (failures) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All Direct2D/WIC tests passed\n";
    return 0;
}
