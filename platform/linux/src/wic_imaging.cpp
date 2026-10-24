// WIC on gdk-pixbuf (decoding) and Cairo (PNG encoding, bitmap storage).
// Internally every pixel source can produce 32bpp premultiplied BGRA
// (PBGRA), which is Cairo's ARGB32 on little-endian machines.
#include "compat_internal.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <wincodec.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

namespace tinta {
namespace {

void premultiply(unsigned char* bgra, size_t pixels) {
    for (size_t i = 0; i < pixels; i++, bgra += 4) {
        unsigned a = bgra[3];
        bgra[0] = static_cast<unsigned char>((bgra[0] * a + 127) / 255);
        bgra[1] = static_cast<unsigned char>((bgra[1] * a + 127) / 255);
        bgra[2] = static_cast<unsigned char>((bgra[2] * a + 127) / 255);
    }
}

void unpremultiply(unsigned char* bgra, size_t pixels) {
    for (size_t i = 0; i < pixels; i++, bgra += 4) {
        unsigned a = bgra[3];
        if (a == 0 || a == 255) continue;
        bgra[0] = static_cast<unsigned char>(std::min(255u, (bgra[0] * 255u + a / 2) / a));
        bgra[1] = static_cast<unsigned char>(std::min(255u, (bgra[1] * 255u + a / 2) / a));
        bgra[2] = static_cast<unsigned char>(std::min(255u, (bgra[2] * 255u + a / 2) / a));
    }
}

// Copies `rect` of a PBGRA buffer into `buffer` in `format`
HRESULT copyOut(const PixelBuffer& src, REFGUID format, const WICRect* rect, UINT stride, UINT size, BYTE* buffer) {
    WICRect r = rect ? *rect : WICRect{0, 0, src.width, src.height};
    if (r.X < 0 || r.Y < 0 || r.X + r.Width > src.width || r.Y + r.Height > src.height) return E_INVALIDARG;
    bool bgr24 = format == GUID_WICPixelFormat24bppBGR;
    UINT bpp = bgr24 ? 3 : 4;
    if (stride < static_cast<UINT>(r.Width) * bpp || size < stride * static_cast<UINT>(std::max(0, r.Height - 1)) + r.Width * bpp)
        return E_INVALIDARG;
    for (int y = 0; y < r.Height; y++) {
        const unsigned char* in = src.pixels.data() + (r.Y + y) * src.stride + r.X * 4;
        BYTE* out = buffer + y * stride;
        if (bgr24) {
            for (int x = 0; x < r.Width; x++) {
                out[x * 3 + 0] = in[x * 4 + 0];
                out[x * 3 + 1] = in[x * 4 + 1];
                out[x * 3 + 2] = in[x * 4 + 2];
            }
        } else {
            std::memcpy(out, in, r.Width * 4);
            if (format == GUID_WICPixelFormat32bppBGRA) unpremultiply(out, r.Width);
            else if (format == GUID_WICPixelFormat32bppRGBA) {
                unpremultiply(out, r.Width);
                for (int x = 0; x < r.Width; x++) std::swap(out[x * 4 + 0], out[x * 4 + 2]);
            }
        }
    }
    return S_OK;
}

// Base for sources whose pixels are a PBGRA PixelBuffer
template <class Base>
class BufferSource : public TintaComObject<Base> {
public:
    HRESULT GetSize(UINT* width, UINT* height) override {
        *width = static_cast<UINT>(buffer_.width);
        *height = static_cast<UINT>(buffer_.height);
        return S_OK;
    }
    HRESULT GetPixelFormat(WICPixelFormatGUID* format) override {
        *format = GUID_WICPixelFormat32bppPBGRA;
        return S_OK;
    }
    HRESULT GetResolution(double* dpiX, double* dpiY) override {
        *dpiX = *dpiY = 96.0;
        return S_OK;
    }
    HRESULT CopyPixels(const WICRect* rect, UINT stride, UINT size, BYTE* buffer) override {
        return copyOut(buffer_, GUID_WICPixelFormat32bppPBGRA, rect, stride, size, buffer);
    }
    const PixelBuffer& buffer() const { return buffer_; }

protected:
    PixelBuffer buffer_;
};

// Pixels as stored: like WIC, no EXIF orientation is applied, so sizes from
// the header probe and from decoding always agree
bool bufferFromPixbuf(GdkPixbuf* src, PixelBuffer& out) {
    if (!src) return false;
    int w = gdk_pixbuf_get_width(src), h = gdk_pixbuf_get_height(src);
    int channels = gdk_pixbuf_get_n_channels(src), rowstride = gdk_pixbuf_get_rowstride(src);
    bool alpha = gdk_pixbuf_get_has_alpha(src);
    const guchar* pixels = gdk_pixbuf_read_pixels(src);
    out.width = w;
    out.height = h;
    out.stride = w * 4;
    out.pixels.assign(static_cast<size_t>(out.stride) * h, 0);
    for (int y = 0; y < h; y++) {
        const guchar* in = pixels + y * rowstride;
        unsigned char* row = out.pixels.data() + y * out.stride;
        for (int x = 0; x < w; x++) {
            row[x * 4 + 0] = in[x * channels + 2];
            row[x * 4 + 1] = in[x * channels + 1];
            row[x * 4 + 2] = in[x * channels + 0];
            row[x * 4 + 3] = alpha ? in[x * channels + 3] : 255;
        }
    }
    if (alpha) premultiply(out.pixels.data(), static_cast<size_t>(w) * h);
    return true;
}

// Resamples a PBGRA buffer. GOOD is Cairo's box-style downscale, the
// counterpart of WIC's Fant mode (BEST costs 10x: 810 vs 76 ms for
// 6000x4000 -> 1000x666)
void scaleBuffer(const PixelBuffer& in, int width, int height, bool nearest, PixelBuffer& out) {
    // Cairo only reads a source surface; its API just lacks a const variant
    auto* pixels = const_cast<unsigned char*>(in.pixels.data());
    cairo_surface_t* src = cairo_image_surface_create_for_data(pixels, CAIRO_FORMAT_ARGB32, in.width,
                                                               in.height, in.stride);
    cairo_surface_t* dst = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* cr = cairo_create(dst);
    cairo_scale(cr, static_cast<double>(width) / in.width, static_cast<double>(height) / in.height);
    cairo_set_source_surface(cr, src, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), nearest ? CAIRO_FILTER_NEAREST : CAIRO_FILTER_GOOD);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_flush(dst);
    out.width = width;
    out.height = height;
    out.stride = cairo_image_surface_get_stride(dst);
    const unsigned char* data = cairo_image_surface_get_data(dst);
    out.pixels.assign(data, data + static_cast<size_t>(out.stride) * height);
    cairo_surface_destroy(dst);
    cairo_surface_destroy(src);
}

// ---- Decoding --------------------------------------------------------------------
// An encoded file kept until its pixels are needed. As with WIC, the size
// comes from the header, and a scaler on a fresh frame decodes for its
// target size instead of decoding everything first.
struct EncodedImage {
    std::vector<unsigned char> bytes;
    int width = 0, height = 0;
    bool jpeg = false;
};

void sizePrepared(GdkPixbufLoader*, int width, int height, gpointer data) {
    auto* image = static_cast<EncodedImage*>(data);
    image->width = width;
    image->height = height;
}

// Reads the header only: feeds the loader until it reports a size
bool probeEncoded(EncodedImage& image) {
    GdkPixbufLoader* loader = gdk_pixbuf_loader_new();
    g_signal_connect(loader, "size-prepared", G_CALLBACK(sizePrepared), &image);
    constexpr size_t kChunk = 16384;
    for (size_t at = 0; at < image.bytes.size() && image.width == 0; at += kChunk) {
        size_t n = std::min(kChunk, image.bytes.size() - at);
        if (!gdk_pixbuf_loader_write(loader, image.bytes.data() + at, n, nullptr)) break;
    }
    if (GdkPixbufFormat* format = gdk_pixbuf_loader_get_format(loader)) {
        gchar* name = gdk_pixbuf_format_get_name(format);
        image.jpeg = name && std::strcmp(name, "jpeg") == 0;
        g_free(name);
    }
    gdk_pixbuf_loader_close(loader, nullptr);  // usually "truncated": only the header was wanted
    g_object_unref(loader);
    return image.width > 0 && image.height > 0;
}

// The size to decode a JPEG at for a `width` x `height` result: libjpeg
// scales by 1/2, 1/4 and 1/8 almost for free while decoding, so take the
// smallest such size that still covers the target (0: decode in full).
// Other ratios are left to Cairo, which beats gdk-pixbuf's resampler
// (6000x4000 -> 5016x3344: decode + Cairo GOOD 207 ms, gdk-pixbuf 303 ms).
void jpegDecodeSize(const EncodedImage& image, int width, int height, int& decodeW, int& decodeH) {
    decodeW = decodeH = 0;
    for (int k : {8, 4, 2}) {
        int w = (image.width + k - 1) / k, h = (image.height + k - 1) / k;
        if (w >= width && h >= height) {
            decodeW = w;
            decodeH = h;
            return;
        }
    }
}

// TINTA_TRACE_WIC=1 logs every decode with its size and duration
std::atomic<int> decodeCount{0};

bool traceDecodes() {
    static const bool enabled = std::getenv("TINTA_TRACE_WIC") != nullptr;
    return enabled;
}

// Decodes at width x height (0: natural size); DPI from the file when known
bool decodeEncoded(const EncodedImage& image, int width, int height, PixelBuffer& out, double& dpiX,
                   double& dpiY) {
    auto start = std::chrono::steady_clock::now();
    decodeCount.fetch_add(1);
    GdkPixbufLoader* loader = gdk_pixbuf_loader_new();
    int decodeW = 0, decodeH = 0;
    if (width > 0 && height > 0 && image.jpeg) jpegDecodeSize(image, width, height, decodeW, decodeH);
    if (decodeW > 0) gdk_pixbuf_loader_set_size(loader, decodeW, decodeH);
    bool ok = gdk_pixbuf_loader_write(loader, image.bytes.data(), image.bytes.size(), nullptr);
    ok = gdk_pixbuf_loader_close(loader, nullptr) && ok;
    GdkPixbuf* pixbuf = ok ? gdk_pixbuf_loader_get_pixbuf(loader) : nullptr;
    bool decoded = bufferFromPixbuf(pixbuf, out);
    if (decoded) {
        const char* x = gdk_pixbuf_get_option(pixbuf, "x-dpi");
        const char* y = gdk_pixbuf_get_option(pixbuf, "y-dpi");
        if (x && std::atof(x) > 0) dpiX = std::atof(x);
        if (y && std::atof(y) > 0) dpiY = std::atof(y);
    }
    g_object_unref(loader);
    if (decoded && width > 0 && height > 0 && (out.width != width || out.height != height)) {
        PixelBuffer exact;
        scaleBuffer(out, width, height, false, exact);
        out = std::move(exact);
    }
    if (traceDecodes()) {
        std::fprintf(stderr, "wic: decode %dx%d -> %dx%d in %.1f ms (thread %zx)\n", image.width, image.height,
                     out.width, out.height,
                     std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count(),
                     std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xffff);
    }
    return decoded;
}

class FrameDecode final : public BufferSource<IWICBitmapFrameDecode> {
public:
    using Primary = IWICBitmapFrameDecode;
    explicit FrameDecode(std::shared_ptr<const EncodedImage> image) : image_(std::move(image)) {}
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IWICBitmapFrameDecode, IWICBitmapSource>(this, riid, object);
    }
    HRESULT GetSize(UINT* width, UINT* height) override {
        *width = static_cast<UINT>(image_->width);
        *height = static_cast<UINT>(image_->height);
        return S_OK;
    }
    // Known after any decode (every decode reads the same header)
    HRESULT GetResolution(double* dpiX, double* dpiY) override {
        if (!dpiKnown_) natural();
        *dpiX = dpiX_;
        *dpiY = dpiY_;
        return S_OK;
    }
    HRESULT CopyPixels(const WICRect* rect, UINT stride, UINT size, BYTE* buffer) override {
        const PixelBuffer* pixels = natural();
        return pixels ? copyOut(*pixels, GUID_WICPixelFormat32bppPBGRA, rect, stride, size, buffer)
                      : WINCODEC_ERR_UNKNOWNIMAGEFORMAT;
    }

    // Full-resolution pixels, decoded on first use
    const PixelBuffer* natural() {
        if (!decoded_) {
            decoded_ = dpiKnown_ = true;
            ok_ = decodeEncoded(*image_, 0, 0, buffer_, dpiX_, dpiY_);
        }
        return ok_ ? &buffer_ : nullptr;
    }
    // Pixels at a given size, straight from the encoded data
    bool scaled(int width, int height, PixelBuffer& out) {
        if (decoded_) {
            if (!ok_) return false;
            scaleBuffer(buffer_, width, height, false, out);
            return true;
        }
        dpiKnown_ = true;
        return decodeEncoded(*image_, width, height, out, dpiX_, dpiY_);
    }

private:
    std::shared_ptr<const EncodedImage> image_;
    bool decoded_ = false, ok_ = false, dpiKnown_ = false;
    double dpiX_ = 96.0, dpiY_ = 96.0;
};

class Decoder final : public TintaComObject<IWICBitmapDecoder> {
public:
    using Primary = IWICBitmapDecoder;
    explicit Decoder(std::shared_ptr<const EncodedImage> image) : image_(std::move(image)) {}
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IWICBitmapDecoder>(this, riid, object);
    }
    HRESULT GetFrameCount(UINT* count) override {
        *count = 1;
        return S_OK;
    }
    HRESULT GetFrame(UINT index, IWICBitmapFrameDecode** frame) override {
        if (index != 0) return E_INVALIDARG;
        *frame = new FrameDecode(image_);
        return S_OK;
    }

private:
    std::shared_ptr<const EncodedImage> image_;
};

// Encoded bytes -> a decoder, or the WIC error for unreadable data
HRESULT createDecoder(std::vector<unsigned char> bytes, IWICBitmapDecoder** decoder) {
    auto image = std::make_shared<EncodedImage>();
    image->bytes = std::move(bytes);
    if (!probeEncoded(*image)) return WINCODEC_ERR_UNKNOWNIMAGEFORMAT;
    *decoder = new Decoder(std::move(image));
    return S_OK;
}

class Scaler final : public BufferSource<IWICBitmapScaler> {
public:
    using Primary = IWICBitmapScaler;
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IWICBitmapScaler, IWICBitmapSource>(this, riid, object);
    }
    HRESULT Initialize(IWICBitmapSource* source, UINT width, UINT height, WICBitmapInterpolationMode mode) override {
        if (!source || !width || !height) return E_INVALIDARG;
        bool nearest = mode == WICBitmapInterpolationModeNearestNeighbor;
        if (auto* frame = dynamic_cast<FrameDecode*>(source); frame && !nearest) {
            return frame->scaled(static_cast<int>(width), static_cast<int>(height), buffer_)
                       ? S_OK
                       : WINCODEC_ERR_UNKNOWNIMAGEFORMAT;
        }
        PixelBuffer in;
        if (!readWicSource(source, in)) return E_INVALIDARG;
        scaleBuffer(in, static_cast<int>(width), static_cast<int>(height), nearest, buffer_);
        return S_OK;
    }
};

class FormatConverter final : public BufferSource<IWICFormatConverter> {
public:
    using Primary = IWICFormatConverter;
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IWICFormatConverter, IWICBitmapSource>(this, riid, object);
    }
    HRESULT Initialize(IWICBitmapSource* source, REFWICPixelFormatGUID format, WICBitmapDitherType, IWICPalette*,
                       double, WICBitmapPaletteType) override {
        if (!readWicSource(source, buffer_)) return E_INVALIDARG;
        format_ = format;
        return S_OK;
    }
    HRESULT GetPixelFormat(WICPixelFormatGUID* format) override {
        *format = format_;
        return S_OK;
    }
    HRESULT CopyPixels(const WICRect* rect, UINT stride, UINT size, BYTE* buffer) override {
        return copyOut(buffer_, format_, rect, stride, size, buffer);
    }

private:
    GUID format_ = GUID_WICPixelFormat32bppPBGRA;
};

// ---- Bitmaps -------------------------------------------------------------------------
class BitmapLock final : public TintaComObject<IWICBitmapLock> {
public:
    using Primary = IWICBitmapLock;
    BitmapLock(IWICBitmap* owner, cairo_surface_t* surface) : owner_(owner), surface_(surface) {
        owner_->AddRef();
        cairo_surface_flush(surface_);
    }
    ~BitmapLock() override {
        cairo_surface_mark_dirty(surface_);
        owner_->Release();
    }
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IWICBitmapLock>(this, riid, object);
    }
    HRESULT GetSize(UINT* width, UINT* height) override {
        *width = static_cast<UINT>(cairo_image_surface_get_width(surface_));
        *height = static_cast<UINT>(cairo_image_surface_get_height(surface_));
        return S_OK;
    }
    HRESULT GetStride(UINT* stride) override {
        *stride = static_cast<UINT>(cairo_image_surface_get_stride(surface_));
        return S_OK;
    }
    HRESULT GetDataPointer(UINT* size, BYTE** data) override {
        *data = cairo_image_surface_get_data(surface_);
        *size = static_cast<UINT>(cairo_image_surface_get_stride(surface_) * cairo_image_surface_get_height(surface_));
        return S_OK;
    }

private:
    IWICBitmap* owner_;
    cairo_surface_t* surface_;
};

// Storage is a Cairo image surface, so D2D can render into it directly
class WicBitmap final : public TintaComObject<IWICBitmap> {
public:
    using Primary = IWICBitmap;
    WicBitmap(UINT width, UINT height, REFWICPixelFormatGUID format)
        : surface_(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, std::max(1u, width), std::max(1u, height))),
          format_(format) {}
    ~WicBitmap() override { cairo_surface_destroy(surface_); }
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IWICBitmap, IWICBitmapSource>(this, riid, object);
    }
    HRESULT GetSize(UINT* width, UINT* height) override {
        *width = static_cast<UINT>(cairo_image_surface_get_width(surface_));
        *height = static_cast<UINT>(cairo_image_surface_get_height(surface_));
        return S_OK;
    }
    HRESULT GetPixelFormat(WICPixelFormatGUID* format) override {
        *format = format_;
        return S_OK;
    }
    HRESULT GetResolution(double* dpiX, double* dpiY) override {
        *dpiX = *dpiY = 96.0;
        return S_OK;
    }
    HRESULT CopyPixels(const WICRect* rect, UINT stride, UINT size, BYTE* buffer) override {
        PixelBuffer view;
        read(view);
        return copyOut(view, format_, rect, stride, size, buffer);
    }
    HRESULT Lock(const WICRect*, DWORD, IWICBitmapLock** lock) override {
        *lock = new BitmapLock(this, surface_);
        return S_OK;
    }
    HRESULT SetResolution(double, double) override { return S_OK; }

    void read(PixelBuffer& out) {
        cairo_surface_flush(surface_);
        out.width = cairo_image_surface_get_width(surface_);
        out.height = cairo_image_surface_get_height(surface_);
        out.stride = cairo_image_surface_get_stride(surface_);
        const unsigned char* data = cairo_image_surface_get_data(surface_);
        out.pixels.assign(data, data + static_cast<size_t>(out.stride) * out.height);
    }
    void write(const PixelBuffer& in) {
        cairo_surface_flush(surface_);
        unsigned char* data = cairo_image_surface_get_data(surface_);
        int stride = cairo_image_surface_get_stride(surface_);
        int rows = std::min(in.height, cairo_image_surface_get_height(surface_));
        int bytes = std::min(in.width, cairo_image_surface_get_width(surface_)) * 4;
        for (int y = 0; y < rows; y++) std::memcpy(data + y * stride, in.pixels.data() + y * in.stride, bytes);
        cairo_surface_mark_dirty(surface_);
    }
    cairo_surface_t* surface() const { return surface_; }

private:
    cairo_surface_t* surface_;
    GUID format_;
};

// ---- Encoding --------------------------------------------------------------------------
class FrameEncode final : public TintaComObject<IWICBitmapFrameEncode> {
public:
    using Primary = IWICBitmapFrameEncode;
    FrameEncode(IStream* output, bool jpeg) : output_(output), jpeg_(jpeg) { output_->AddRef(); }
    ~FrameEncode() override { output_->Release(); }
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IWICBitmapFrameEncode>(this, riid, object);
    }
    HRESULT Initialize(IPropertyBag2*) override { return S_OK; }
    HRESULT SetSize(UINT width, UINT height) override {
        pixels_.width = static_cast<int>(width);
        pixels_.height = static_cast<int>(height);
        pixels_.stride = static_cast<int>(width) * 4;
        pixels_.pixels.assign(static_cast<size_t>(pixels_.stride) * height, 0);
        return S_OK;
    }
    HRESULT SetResolution(double dpiX, double dpiY) override {
        dpiX_ = dpiX;
        dpiY_ = dpiY;
        return S_OK;
    }
    // 24bpp BGR, 32bpp BGRA and 32bpp PBGRA are written as given; anything
    // else is answered with 32bpp BGRA, as WIC does for formats it lacks
    HRESULT SetPixelFormat(WICPixelFormatGUID* format) override {
        if (*format != GUID_WICPixelFormat24bppBGR && *format != GUID_WICPixelFormat32bppPBGRA &&
            *format != GUID_WICPixelFormat32bppBGRA) {
            *format = GUID_WICPixelFormat32bppBGRA;
        }
        format_ = *format;
        return S_OK;
    }
    HRESULT WritePixels(UINT lines, UINT stride, UINT, BYTE* data) override {
        if (pixels_.pixels.empty()) return E_UNEXPECTED;
        UINT rows = std::min<UINT>(lines, static_cast<UINT>(pixels_.height - written_));
        bool bgr = format_ == GUID_WICPixelFormat24bppBGR;
        for (UINT y = 0; y < rows; y++) {
            unsigned char* row = pixels_.pixels.data() + (written_ + y) * pixels_.stride;
            const BYTE* in = data + y * stride;
            if (bgr) {
                for (int x = 0; x < pixels_.width && static_cast<UINT>(x * 3 + 2) < stride; x++) {
                    row[x * 4 + 0] = in[x * 3 + 0];
                    row[x * 4 + 1] = in[x * 3 + 1];
                    row[x * 4 + 2] = in[x * 3 + 2];
                    row[x * 4 + 3] = 255;
                }
            } else {
                std::memcpy(row, in, std::min<size_t>(stride, pixels_.stride));
                if (format_ == GUID_WICPixelFormat32bppBGRA) premultiply(row, pixels_.width);
            }
        }
        written_ += static_cast<int>(rows);
        return S_OK;
    }
    HRESULT WriteSource(IWICBitmapSource* source, WICRect*) override {
        PixelBuffer in;
        if (!readWicSource(source, in)) return E_INVALIDARG;
        if (pixels_.pixels.empty()) SetSize(static_cast<UINT>(in.width), static_cast<UINT>(in.height));
        int rows = std::min(in.height, pixels_.height), bytes = std::min(in.width, pixels_.width) * 4;
        for (int y = 0; y < rows; y++) {
            std::memcpy(pixels_.pixels.data() + y * pixels_.stride, in.pixels.data() + y * in.stride, bytes);
        }
        double dpiX = 96.0, dpiY = 96.0;
        if (SUCCEEDED(source->GetResolution(&dpiX, &dpiY)) && dpiX_ <= 0) SetResolution(dpiX, dpiY);
        written_ = pixels_.height;
        return S_OK;
    }
    // gdk-pixbuf writes both containers with their resolution (PNG pHYs,
    // JFIF density); alpha survives in PNG, JPEG flattens it onto white
    HRESULT Commit() override {
        if (pixels_.pixels.empty()) return E_UNEXPECTED;
        bool alpha = !jpeg_ && format_ != GUID_WICPixelFormat24bppBGR;
        GdkPixbuf* pixbuf = toPixbuf(alpha);
        std::string x = std::to_string(static_cast<int>(std::lround(dpiX_ > 0 ? dpiX_ : 96.0)));
        std::string y = std::to_string(static_cast<int>(std::lround(dpiY_ > 0 ? dpiY_ : 96.0)));
        std::vector<char*> keys{const_cast<char*>("x-dpi"), const_cast<char*>("y-dpi")};
        std::vector<char*> values{x.data(), y.data()};
        if (jpeg_) {
            keys.push_back(const_cast<char*>("quality"));
            values.push_back(const_cast<char*>("92"));
        }
        keys.push_back(nullptr);
        values.push_back(nullptr);
        gchar* buffer = nullptr;
        gsize size = 0;
        bool ok = gdk_pixbuf_save_to_bufferv(pixbuf, &buffer, &size, jpeg_ ? "jpeg" : "png", keys.data(),
                                             values.data(), nullptr);
        ULONG out = 0;
        ok = ok && SUCCEEDED(output_->Write(buffer, static_cast<ULONG>(size), &out)) && out == size;
        g_free(buffer);
        g_object_unref(pixbuf);
        return ok ? S_OK : E_FAIL;
    }

private:
    // Straight RGB(A) from the premultiplied buffer
    GdkPixbuf* toPixbuf(bool alpha) const {
        int w = pixels_.width, h = pixels_.height, channels = alpha ? 4 : 3;
        GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, alpha, 8, w, h);
        guchar* out = gdk_pixbuf_get_pixels(pixbuf);
        int outStride = gdk_pixbuf_get_rowstride(pixbuf);
        for (int yy = 0; yy < h; yy++) {
            for (int xx = 0; xx < w; xx++) {
                const unsigned char* p = pixels_.pixels.data() + yy * pixels_.stride + xx * 4;
                guchar* o = out + yy * outStride + xx * channels;
                unsigned a = p[3];
                if (alpha) {
                    auto straight = [a](unsigned c) { return a ? static_cast<guchar>(std::min(255u, (c * 255 + a / 2) / a)) : 0; };
                    o[0] = straight(p[2]);
                    o[1] = straight(p[1]);
                    o[2] = straight(p[0]);
                    o[3] = static_cast<guchar>(a);
                } else {
                    auto onWhite = [a](unsigned c) { return static_cast<guchar>(std::min(255u, c + (255 - a))); };
                    o[0] = onWhite(p[2]);
                    o[1] = onWhite(p[1]);
                    o[2] = onWhite(p[0]);
                }
            }
        }
        return pixbuf;
    }

    IStream* output_;
    bool jpeg_;
    PixelBuffer pixels_;
    GUID format_ = GUID_WICPixelFormat32bppPBGRA;
    double dpiX_ = 0.0, dpiY_ = 0.0;
    int written_ = 0;
};

class Encoder final : public TintaComObject<IWICBitmapEncoder> {
public:
    using Primary = IWICBitmapEncoder;
    explicit Encoder(bool jpeg) : jpeg_(jpeg) {}
    ~Encoder() override {
        if (output_) output_->Release();
    }
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IWICBitmapEncoder>(this, riid, object);
    }
    HRESULT Initialize(IStream* stream, WICBitmapEncoderCacheOption) override {
        if (!stream) return E_INVALIDARG;
        stream->AddRef();
        output_ = stream;
        return S_OK;
    }
    HRESULT CreateNewFrame(IWICBitmapFrameEncode** frame, IPropertyBag2** options) override {
        if (!output_) return E_UNEXPECTED;
        *frame = new FrameEncode(output_, jpeg_);
        if (options) *options = nullptr;
        return S_OK;
    }
    HRESULT Commit() override { return output_ ? output_->Commit(0) : E_UNEXPECTED; }

private:
    IStream* output_ = nullptr;
    bool jpeg_;
};

// ---- Streams ---------------------------------------------------------------------------
class WicStream final : public TintaComObject<IWICStream> {
public:
    using Primary = IWICStream;
    ~WicStream() override {
        if (inner_) inner_->Release();
    }
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IWICStream, IStream, ISequentialStream>(this, riid, object);
    }
    HRESULT Read(void* buffer, ULONG size, ULONG* read) override {
        return inner_ ? inner_->Read(buffer, size, read) : E_UNEXPECTED;
    }
    HRESULT Write(const void* buffer, ULONG size, ULONG* written) override {
        return inner_ ? inner_->Write(buffer, size, written) : E_UNEXPECTED;
    }
    HRESULT Seek(LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER* position) override {
        return inner_ ? inner_->Seek(move, origin, position) : E_UNEXPECTED;
    }
    HRESULT SetSize(ULARGE_INTEGER size) override { return inner_ ? inner_->SetSize(size) : E_UNEXPECTED; }
    HRESULT Commit(DWORD flags) override { return inner_ ? inner_->Commit(flags) : E_UNEXPECTED; }
    HRESULT InitializeFromIStream(IStream* stream) override {
        if (!stream) return E_INVALIDARG;
        stream->AddRef();
        inner_ = stream;
        return S_OK;
    }
    HRESULT InitializeFromFilename(LPCWSTR path, DWORD access) override {
        bool write = (access & GENERIC_WRITE) != 0;
        inner_ = createFileStream(nativePath(path), write, write);
        return inner_ ? S_OK : HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
    }
    HRESULT InitializeFromMemory(BYTE* buffer, DWORD size) override {
        inner_ = createMemoryStream(buffer, size);
        return inner_ ? S_OK : E_OUTOFMEMORY;
    }

private:
    IStream* inner_ = nullptr;
};

class ImagingFactory final : public TintaComObject<IWICImagingFactory> {
public:
    using Primary = IWICImagingFactory;
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IWICImagingFactory>(this, riid, object);
    }
    HRESULT CreateDecoderFromFilename(LPCWSTR path, const GUID*, DWORD, WICDecodeOptions,
                                      IWICBitmapDecoder** decoder) override {
        if (!path || !decoder) return E_INVALIDARG;
        FILE* file = std::fopen(nativePath(path).c_str(), "rb");
        if (!file) return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        std::vector<unsigned char> bytes;
        unsigned char chunk[65536];
        size_t n;
        while ((n = std::fread(chunk, 1, sizeof chunk, file)) > 0) bytes.insert(bytes.end(), chunk, chunk + n);
        std::fclose(file);
        return createDecoder(std::move(bytes), decoder);
    }
    HRESULT CreateDecoderFromStream(IStream* stream, const GUID*, WICDecodeOptions,
                                    IWICBitmapDecoder** decoder) override {
        if (!stream || !decoder) return E_INVALIDARG;
        std::vector<unsigned char> bytes;
        unsigned char chunk[65536];
        ULONG read = 0;
        while (SUCCEEDED(stream->Read(chunk, sizeof chunk, &read)) && read > 0) {
            bytes.insert(bytes.end(), chunk, chunk + read);
        }
        return createDecoder(std::move(bytes), decoder);
    }
    HRESULT CreateFormatConverter(IWICFormatConverter** converter) override {
        *converter = new FormatConverter();
        return S_OK;
    }
    HRESULT CreateBitmapScaler(IWICBitmapScaler** scaler) override {
        *scaler = new Scaler();
        return S_OK;
    }
    HRESULT CreateEncoder(REFGUID container, const GUID*, IWICBitmapEncoder** encoder) override {
        if (container != GUID_ContainerFormatPng && container != GUID_ContainerFormatJpeg) {
            return WINCODEC_ERR_COMPONENTNOTFOUND;
        }
        *encoder = new Encoder(container == GUID_ContainerFormatJpeg);
        return S_OK;
    }
    HRESULT CreateStream(IWICStream** stream) override {
        *stream = new WicStream();
        return S_OK;
    }
    HRESULT CreateBitmap(UINT width, UINT height, REFWICPixelFormatGUID format, WICBitmapCreateCacheOption,
                         IWICBitmap** bitmap) override {
        if (!width || !height) return E_INVALIDARG;
        *bitmap = new WicBitmap(width, height, format);
        return S_OK;
    }
    HRESULT CreateBitmapFromMemory(UINT width, UINT height, REFWICPixelFormatGUID format, UINT stride, UINT,
                                   BYTE* data, IWICBitmap** bitmap) override {
        auto* created = new WicBitmap(width, height, format);
        PixelBuffer in;
        in.width = static_cast<int>(width);
        in.height = static_cast<int>(height);
        in.stride = static_cast<int>(stride);
        in.pixels.assign(data, data + static_cast<size_t>(stride) * height);
        if (format == GUID_WICPixelFormat32bppBGRA) {
            for (UINT y = 0; y < height; y++) premultiply(in.pixels.data() + y * stride, width);
        }
        created->write(in);
        *bitmap = created;
        return S_OK;
    }
    // Icons are the app icon's pixels (LoadImageW); GDI bitmaps do not exist here
    HRESULT CreateBitmapFromHICON(HICON icon, IWICBitmap** bitmap) override {
        *bitmap = nullptr;
        if (!icon || icon->pixels.pixels.empty()) return E_INVALIDARG;
        auto* created = new WicBitmap(static_cast<UINT>(icon->pixels.width), static_cast<UINT>(icon->pixels.height),
                                      GUID_WICPixelFormat32bppPBGRA);
        created->write(icon->pixels);
        *bitmap = created;
        return S_OK;
    }
    HRESULT CreateBitmapFromHBITMAP(HBITMAP, HPALETTE, WICBitmapAlphaChannelOption, IWICBitmap** bitmap) override {
        *bitmap = nullptr;
        return E_NOTIMPL;
    }
};

}  // namespace

cairo_surface_t* surfaceOf(IWICBitmap* bitmap) {
    auto* own = dynamic_cast<WicBitmap*>(bitmap);
    return own ? own->surface() : nullptr;
}

bool readWicSource(IWICBitmapSource* source, PixelBuffer& out) {
    if (!source) return false;
    if (auto* frame = dynamic_cast<FrameDecode*>(source)) {
        const PixelBuffer* pixels = frame->natural();
        if (!pixels) return false;
        out = *pixels;
        return true;
    }
    if (auto* own = dynamic_cast<WicBitmap*>(source)) {
        own->read(out);
        return true;
    }
    UINT width = 0, height = 0;
    if (FAILED(source->GetSize(&width, &height)) || !width || !height) return false;
    out.width = static_cast<int>(width);
    out.height = static_cast<int>(height);
    out.stride = static_cast<int>(width) * 4;
    out.pixels.assign(static_cast<size_t>(out.stride) * height, 0);
    WICPixelFormatGUID format{};
    source->GetPixelFormat(&format);
    if (FAILED(source->CopyPixels(nullptr, static_cast<UINT>(out.stride), static_cast<UINT>(out.pixels.size()),
                                  out.pixels.data()))) {
        return false;
    }
    // Every compat source copies out its own format; normalise to PBGRA
    if (format == GUID_WICPixelFormat32bppBGRA) premultiply(out.pixels.data(), static_cast<size_t>(width) * height);
    return true;
}

int wicDecodeCount() { return decodeCount.load(); }

HRESULT createWicFactory(REFIID riid, void** object) {
    auto* factory = new ImagingFactory();
    HRESULT hr = factory->QueryInterface(riid, object);
    factory->Release();
    return hr;
}

}  // namespace tinta
