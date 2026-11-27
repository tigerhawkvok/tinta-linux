#include "d2d_init.h"
#include "export.h"
#include "image_loader.h"
#include "input.h"
#include "overlays.h"
#include "print.h"
#include "render.h"

#include <algorithm>
#include <filesystem>
#include <climits>
#include <fstream>
#include <iostream>
#include <memory>

namespace {
using Microsoft::WRL::ComPtr;
int failures = 0;
void check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

bool writeImage(App& app, const wchar_t* path, UINT width, UINT height,
                bool jpeg = false, double dpiValue = 96.0, bool alpha = false) {
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> frame;
    if (FAILED(app.wicFactory->CreateStream(stream.GetAddressOf())) ||
        FAILED(stream->InitializeFromFilename(path, GENERIC_WRITE)) ||
        FAILED(app.wicFactory->CreateEncoder(jpeg ? GUID_ContainerFormatJpeg : GUID_ContainerFormatPng,
                                             nullptr, encoder.GetAddressOf())) ||
        FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) ||
        FAILED(encoder->CreateNewFrame(frame.GetAddressOf(), nullptr)) ||
        FAILED(frame->Initialize(nullptr)) || FAILED(frame->SetSize(width, height)) ||
        FAILED(frame->SetResolution(dpiValue, dpiValue))) return false;
    WICPixelFormatGUID format = alpha ? GUID_WICPixelFormat32bppBGRA : GUID_WICPixelFormat24bppBGR;
    const auto requested = format;
    if (FAILED(frame->SetPixelFormat(&format)) || format != requested) return false;
    const UINT channels = alpha ? 4 : 3;
    std::vector<BYTE> row(static_cast<size_t>(width) * channels);
    for (UINT x = 0; x < width; ++x) {
        row[x * channels] = 100;
        row[x * channels + 1] = 50;
        row[x * channels + 2] = 20;
        if (alpha) row[x * channels + 3] = 128;
    }
    for (UINT y = 0; y < height; ++y)
        if (FAILED(frame->WritePixels(1, width * channels, static_cast<UINT>(row.size()), row.data())))
            return false;
    return SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
}

void generateAssets(App& app) {
    std::filesystem::create_directories("image-cache-assets");
    check(writeImage(app, L"image-cache-assets/small.png", 256, 128), "write small PNG");
    check(writeImage(app, L"image-cache-assets/large.jpg", 6000, 4000, true), "write large JPEG");
    std::filesystem::copy_file("image-cache-assets/large.jpg", "image-cache-assets/large-second.jpg",
                              std::filesystem::copy_options::overwrite_existing);
    check(writeImage(app, L"image-cache-assets/alpha.png", 128, 64, false, 96, true), "write transparent PNG");
    check(writeImage(app, L"image-cache-assets/dpi.png", 300, 150, false, 300), "write high-DPI PNG");
    check(writeImage(app, L"image-cache-assets/wide.png", 12000, 12), "write panorama");
    std::ofstream("image-cache-assets/broken.jpg", std::ios::binary) << "not an image";
    // A tiny BMP header advertising 400 MP: reject before any pixel allocation.
    BITMAPFILEHEADER fileHeader{};
    fileHeader.bfType = 0x4d42;
    fileHeader.bfOffBits = sizeof(fileHeader) + sizeof(BITMAPINFOHEADER);
    fileHeader.bfSize = fileHeader.bfOffBits;
    BITMAPINFOHEADER info{};
    info.biSize = sizeof(info); info.biWidth = info.biHeight = 20000;
    info.biPlanes = 1; info.biBitCount = 24;
    std::ofstream oversized("image-cache-assets/oversized.bmp", std::ios::binary);
    oversized.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    oversized.write(reinterpret_cast<const char*>(&info), sizeof(info));
    std::filesystem::copy_file(TINTA_IMAGE_FIXTURE, "image-cache-large.md",
                              std::filesystem::copy_options::overwrite_existing);
}

void decoding(App& app) {
    DecodedImage decoded;
    check(SUCCEEDED(decodeImageFile(app.wicFactory, L"image-cache-assets/large.jpg", 8192, decoded)),
          "large JPEG decodes");
    check(decoded.width < 6000 && decoded.height < 4000 &&
          decoded.pixels.size() <= IMAGE_DECODE_MAX_PIXELS * 4,
          "large JPEG uses a bounded display buffer");
    check(std::abs(static_cast<float>(decoded.width) * 96.0f / decoded.dpiX - 6000.0f) < 1.0f &&
          std::abs(static_cast<float>(decoded.height) * 96.0f / decoded.dpiY - 4000.0f) < 1.0f,
          "downsampling preserves the intrinsic size");
    check(SUCCEEDED(decodeImageFile(app.wicFactory, L"image-cache-assets/wide.png", 1024, decoded)) &&
          decoded.width == 1024 && decoded.height == 1, "panorama respects device texture limit");
    check(SUCCEEDED(decodeImageFile(app.wicFactory, L"image-cache-assets/alpha.png", 8192, decoded)) &&
          decoded.pixels[0] == 50 && decoded.pixels[1] == 25 && decoded.pixels[2] == 10 &&
          decoded.pixels[3] == 128, "PNG alpha is premultiplied correctly");
    for (const auto* path : {L"image-cache-assets/broken.jpg", L"image-cache-assets/missing.png",
                              L"image-cache-assets/oversized.bmp"}) {
        check(FAILED(decodeImageFile(app.wicFactory, path, 8192, decoded)) &&
              decoded.pixels.empty() && decoded.width == 0, "bad images leave no partial result");
    }
    check(boundedImageSize(0, 10, 8192).width == 0, "zero dimensions rejected");
    const auto extreme = boundedImageSize(UINT_MAX, UINT_MAX, 8192);
    check(static_cast<uint64_t>(extreme.width) * extreme.height <= IMAGE_DECODE_MAX_PIXELS,
          "size calculation cannot overflow for extreme dimensions");
}

App::ImageEntry bitmapEntry(App& app, UINT width, UINT height, float dpiValue = 96.0f) {
    App::ImageEntry entry;
    const auto props = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpiValue, dpiValue);
    check(SUCCEEDED(app.renderTarget->CreateBitmap(D2D1::SizeU(width, height), nullptr, 0,
                    props, entry.bitmap.GetAddressOf())), "native test bitmap created");
    entry.width = static_cast<int>(static_cast<float>(width) * 96.0f / dpiValue);
    entry.height = static_cast<int>(static_cast<float>(height) * 96.0f / dpiValue);
    return entry;
}

void drawImages(App& app) {
    app.renderTarget->BeginDraw();
    for (const auto& bitmap : app.layoutBitmaps) {
        check(bitmap.bitmap && bitmap.bitmap->GetPixelSize().width > 0, "layout bitmap is alive");
        app.renderTarget->DrawBitmap(bitmap.bitmap.Get(), bitmap.destRect);
    }
    check(SUCCEEDED(app.renderTarget->EndDraw()), "native image drawing succeeds after eviction");
}

void ownership(App& app) {
    app.clearLayoutCache();
    app.releaseImageCache();
    app.storeImageCacheEntry("first", bitmapEntry(app, 300, 150, 300));
    check(app.imageCacheBytes == 300 * 150 * 4, "cache counts pixels rather than DPI-adjusted size");
    app.layoutBitmaps.push_back({app.imageCache.at("first").bitmap, D2D1::RectF(0, 0, 96, 48)});
    // 64 MiB plus the earlier image must evict the earlier cache entry.
    app.storeImageCacheEntry("budget", bitmapEntry(app, 4096, 4096));
    check(!app.imageCache.count("first"), "byte budget eviction actually occurs");
    drawImages(app);
    app.releaseImageCache();
    drawImages(app);

    app.storeImageCacheEntry("first", bitmapEntry(app, 8, 8));
    app.layoutBitmaps.push_back({app.imageCache.at("first").bitmap, D2D1::RectF(0, 50, 8, 58)});
    auto filler = bitmapEntry(app, 1, 1);
    for (size_t i = 0; i < App::IMAGE_CACHE_MAX_ENTRIES; ++i)
        app.storeImageCacheEntry(std::to_string(i), filler);
    check(!app.imageCache.count("first"), "entry-count eviction actually occurs");
    drawImages(app);

    app.storeImageCacheEntry("replacement", bitmapEntry(app, 12, 12));
    app.layoutBitmaps.push_back({app.imageCache.at("replacement").bitmap, D2D1::RectF(0, 60, 12, 72)});
    app.storeImageCacheEntry("replacement", bitmapEntry(app, 16, 16));
    auto savedLayout = app.layoutBitmaps;
    app.layoutBitmaps.resize(1);  // table measurement rollback
    app.releaseImageCache();
    app.layoutBitmaps = std::move(savedLayout);
    drawImages(app);
    openLightbox(app, app.layoutBitmaps.back().bitmap.Get());
    app.clearLayoutCache();
    check(app.lightboxBitmap && app.lightboxBitmap->GetPixelSize().width == 12,
          "lightbox owns its image after cache and layout reset");
    closeLightbox(app);
    check(createRenderTarget(app) && app.layoutBitmaps.empty() && app.imageCache.empty() &&
          !app.showLightbox && app.layoutDirty, "target recreation invalidates old image drawings");
}

void mixedLayout(App& app) {
    std::ifstream file("image-cache-large.md", std::ios::binary);
    const std::string source((std::istreambuf_iterator<char>(file)), {});
    app.currentFile = std::filesystem::absolute("image-cache-large.md").u8string();
    app.root = app.parser.parse(source).root;
    app.readingWidthPct = 100;
    for (int theme : {0, 5}) {
        applyTheme(app, theme);
        for (int width : {1050, 650}) {
            app.width = width;
            app.height = 900;
            for (float scale : {1.0f, 1.5f}) {
                app.contentScale = scale;
                updateTextFormats(app);
                layoutDocumentViewportFirst(app);
                drawImages(app);
                ensureLayoutComplete(app);
                check(app.layoutBitmaps.size() == 7, "all valid mixed images survive measurement and relayout");
                check(app.tableRects.size() == 1 && app.codeBlocks.size() == 1 &&
                      app.docText.find(L"All surrounding content") != std::wstring::npos,
                      "surrounding headings, table, code and final paragraph survive");
                check(app.docText.find(L"Broken image placeholder") != std::wstring::npos &&
                      app.docText.find(L"Oversized image placeholder") != std::wstring::npos,
                      "invalid images render their alt text");
                drawImages(app);
            }
        }
    }
    check(printDebugPages(app, L"pages") > 0, "mixed document prints after cache eviction");
    openLightbox(app, app.layoutBitmaps.front().bitmap.Get());
    check(createRenderTarget(app) && !app.lightboxBitmap && !app.showLightbox &&
          app.layoutBitmaps.empty(), "device reset clears an open lightbox and live layout");
    ensureLayoutComplete(app);
    check(app.layoutBitmaps.size() == 7, "images reload after target recreation");
    drawImages(app);
}

// Images written in HTML (tests/fixtures/html-images.md): an <img> draws at
// its width and height in CSS px, which scale with the text, and a badge
// inside a link is that link under the pointer (links win over the
// lightbox on click). Hover only: a click would open a browser.
void htmlImages(App& app) {
    const auto fixture = std::filesystem::path(TINTA_IMAGE_FIXTURE).parent_path() / "html-images.md";
    std::ifstream file(fixture, std::ios::binary);
    const std::string source((std::istreambuf_iterator<char>(file)), {});
    check(!source.empty(), "the HTML images fixture opens");
    app.currentFile = std::filesystem::absolute(fixture).u8string();
    app.root = app.parser.parse(source).root;
    app.readingWidthPct = 100;
    const std::string badgeUrl =
        "https://example.com/redirect?r=app://add/%7B%22id%22%3A%22org.example.app%22%2C%22name%22%3A%22Example%22%7D";
    auto near = [](float a, float b) { return std::fabs(a - b) < 0.6f; };
    for (int theme : {0, 5}) {
        applyTheme(app, theme);
        for (int width : {1050, 560}) {
            app.width = width;
            app.height = 900;
            for (float scale : {1.0f, 1.5f}) {
                app.contentScale = scale;
                updateTextFormats(app);
                app.layoutDirty = true;
                ensureLayoutComplete(app);
                // Six images draw; the seventh is missing and shows its alt text
                check(app.layoutBitmaps.size() == 6, "every HTML and Markdown image that exists draws");
                check(app.docText.find(L"[image: the project's badge]") != std::wstring::npos,
                      "a missing <img> shows its alt text");
                check(app.docText.find(L"<img") == std::wstring::npos &&
                          app.docText.find(L"<a href") == std::wstring::npos,
                      "no raw <img> or <a href> reaches the page");
                // In document order, from the 400x120 badge.png: width and
                // height; height alone; natural (Markdown); width alone in
                // the HTML block, the list item and the table cell
                const float expected[6][2] = {{200 * scale, 60 * scale}, {80 * scale, 24 * scale}, {400, 120},
                                              {120 * scale, 36 * scale}, {100 * scale, 30 * scale},
                                              {80 * scale, 24 * scale}};
                std::vector<const App::LayoutBitmap*> ordered;
                for (const auto& bmp : app.layoutBitmaps) ordered.push_back(&bmp);
                std::sort(ordered.begin(), ordered.end(), [](const App::LayoutBitmap* a, const App::LayoutBitmap* b) {
                    return a->destRect.top < b->destRect.top;
                });
                bool sizes = ordered.size() == 6;
                std::string drawn;
                for (size_t i = 0; i < ordered.size(); ++i) {
                    const float w = ordered[i]->destRect.right - ordered[i]->destRect.left;
                    const float h = ordered[i]->destRect.bottom - ordered[i]->destRect.top;
                    drawn += std::to_string((int)w) + "x" + std::to_string((int)h) + " ";
                    if (i < 6) sizes = sizes && near(w, expected[i][0]) && near(h, expected[i][1]);
                }
                check(sizes, ("every image takes its HTML size, or its own without one (drew " + drawn + ")").c_str());
                const App::LinkRect* badge = nullptr;
                const App::LinkRect* markdownBadge = nullptr;
                for (const auto& link : app.linkRects) {
                    if (link.url == badgeUrl) badge = &link;
                    if (link.url == "https://example.com/markdown-badge") markdownBadge = &link;
                }
                check(badge && near(badge->bounds.right - badge->bounds.left, 200 * scale) &&
                          near(badge->bounds.bottom - badge->bounds.top, 60 * scale),
                      "the HTML badge is a link the size of the badge");
                check(markdownBadge && markdownBadge->bounds.bottom - markdownBadge->bounds.top > 100,
                      "a Markdown badge inside a link is a link too");
                if (!badge) continue;
                // The pointer over the badge finds its link, not just a picture
                const D2D1_RECT_F r = badge->bounds;
                app.scrollX = app.targetScrollX = 0.0f;
                app.scrollY = app.targetScrollY = std::max(0.0f, r.top - 100.0f);
                const float x = documentViewportX(app) + (r.left + r.right) * 0.5f;
                const float y = (r.top + r.bottom) * 0.5f - app.scrollY;
                handleMouseMove(app, app.hwnd, MAKELPARAM(static_cast<int>(x), static_cast<int>(y)));
                check(app.hoveredLink == badgeUrl, "hovering the badge points at its link");
            }
        }
    }
    app.scrollY = app.targetScrollY = 0.0f;

    // The exports keep the badge's link and size: HTML writes <a><img> with
    // its width and height, Word draws it 200x60 px (9525 EMU a pixel)
    app.contentScale = 1.0f;
    updateTextFormats(app);
    app.layoutDirty = true;
    ensureLayoutComplete(app);
    const std::filesystem::path out = std::filesystem::current_path() / "html-images-export";
    std::filesystem::create_directories(out);
    check(exportHtmlFile(app, (out / "html-images.html").wstring()), "HTML export succeeds");
    std::ifstream htmlFile(out / "html-images.html", std::ios::binary);
    const std::string html((std::istreambuf_iterator<char>(htmlFile)), {});
    const size_t link = html.find("<a href=\"" + badgeUrl + "\"><img src=\"data:image/png;base64,");
    check(link != std::string::npos &&
              html.find("alt=\"Get it on Example\" width=\"200\" height=\"60\"/></a>", link) != std::string::npos,
          "HTML export writes the badge as a linked image with its size");
    check(html.find("&lt;img") == std::string::npos, "HTML export writes no escaped raw <img>");
    check(exportDocxFile(app, (out / "html-images.docx").wstring()), "Word export succeeds");
    std::ifstream docxFile(out / "html-images.docx", std::ios::binary);
    const std::string docx((std::istreambuf_iterator<char>(docxFile)), {});
    check(docx.find("<wp:extent cx=\"1905000\" cy=\"571500\"/>") != std::string::npos,
          "Word export draws the badge at its HTML size");
}

// The baseline of a text run, in document coordinates
float runBaselineY(const App::LayoutTextRun& run) {
    DWRITE_LINE_METRICS metrics[4];
    UINT32 count = 0;
    run.layout->GetLineMetrics(metrics, 4, &count);
    return run.pos.y + (count ? metrics[0].baseline : 0.0f);
}

// Images in the line (tests/fixtures/inline-images.md): a badge row stays a
// row on one baseline, a heading's badge sits beside its words, a line
// grows for a taller picture with its text on the picture's baseline, a
// picture alone in a paragraph leaves no blank line under it, list markers
// follow a grown first line, and text rects stay in the right line buckets.
void inlineImages(App& app) {
    const auto fixture = std::filesystem::path(TINTA_IMAGE_FIXTURE).parent_path() / "inline-images.md";
    std::ifstream file(fixture, std::ios::binary);
    const std::string source((std::istreambuf_iterator<char>(file)), {});
    check(!source.empty(), "the inline images fixture opens");
    app.currentFile = std::filesystem::absolute(fixture).u8string();
    app.root = app.parser.parse(source).root;
    app.readingWidthPct = 100;
    auto near = [](float a, float b, float tolerance = 1.0f) { return std::fabs(a - b) < tolerance; };
    for (int theme : {0, 5}) {
        applyTheme(app, theme);
        for (int width : {1050, 560}) {
            app.width = width;
            app.height = 900;
            for (float scale : {1.0f, 1.5f}) {
                app.contentScale = scale;
                updateTextFormats(app);
                app.layoutDirty = true;
                ensureLayoutComplete(app);
                const std::string where = " (theme " + std::to_string(theme) + ", " + std::to_string(width) +
                                          " px, x" + std::to_string(scale).substr(0, 3) + ")";
                auto c = [&](bool ok, const std::string& what) { check(ok, (what + where).c_str()); };
                // Pictures by their HTML height (all from the 400x120 badge)
                auto byHeight = [&](float h) {
                    std::vector<D2D1_RECT_F> found;
                    for (const auto& bmp : app.layoutBitmaps)
                        if (near(bmp.destRect.bottom - bmp.destRect.top, h * scale, 0.6f)) found.push_back(bmp.destRect);
                    std::sort(found.begin(), found.end(),
                              [](const D2D1_RECT_F& a, const D2D1_RECT_F& b) { return a.left < b.left; });
                    return found;
                };
                auto runStarting = [&](const wchar_t* text) -> const App::LayoutTextRun* {
                    for (const auto& run : app.layoutTextRuns) {
                        if (run.layout && run.docLength &&
                            app.docText.compare(run.docStart, wcslen(text), text) == 0)
                            return &run;
                    }
                    return nullptr;
                };
                c(app.layoutBitmaps.size() == 13,
                  "thirteen pictures draw (" + std::to_string(app.layoutBitmaps.size()) + ")");

                const auto row = byHeight(20);
                c(row.size() == 3 && near(row[0].bottom, row[1].bottom) && near(row[1].bottom, row[2].bottom) &&
                      row[0].right <= row[1].left + 0.5f && row[1].right <= row[2].left + 0.5f,
                  "a badge row is one row on one baseline");
                int rowLinks = 0;
                for (const auto& link : app.linkRects) {
                    for (const auto& badge : row)
                        rowLinks += near(link.bounds.left, badge.left) && near(link.bounds.top, badge.top) &&
                                    (link.url == "https://example.com/build" || link.url == "https://example.com/docs");
                }
                c(rowLinks == 2, "the row's linked badges are their links");

                for (auto [h, words] : {std::pair<float, const wchar_t*>{18, L"Installation"}, {17, L"Usage"}}) {
                    const auto badge = byHeight(h);
                    const auto* heading = runStarting(words);
                    c(badge.size() == 1 && heading && badge[0].left >= heading->bounds.left + 20 &&
                          near(badge[0].bottom, runBaselineY(*heading)),
                      std::string("a heading's badge sits beside its words, on their baseline: ") +
                          (h == 18 ? "Markdown" : "HTML") + " heading");
                }

                const auto tall = byHeight(80);
                const auto* before = runStarting(L"Text before a tall picture");
                // Beside the words when it fits (the line grows to it, the words
                // on its baseline); else wrapped to the next line, like a word
                const bool besideWords = tall.size() == 1 && before && tall[0].left >= before->bounds.right - 1;
                c(tall.size() == 1 && before &&
                      (besideWords ? near(tall[0].bottom, runBaselineY(*before))
                                   : near(tall[0].left, before->bounds.left) && tall[0].top >= before->bounds.bottom - 0.5f),
                  besideWords ? "a tall picture grows its line; the words keep its baseline"
                              : "a tall picture too wide for the rest of the line wraps like a word");
                const auto* usage = runStarting(L"Usage");
                c(tall.size() == 1 && usage && tall[0].top >= usage->bounds.bottom,
                  "the grown line starts below the block above");
                const auto* after = runStarting(L"and the words after it");
                c(after && tall.size() == 1 && (after->bounds.left >= tall[0].right - 1 || after->bounds.top >= tall[0].bottom),
                  "the words after the picture follow it");

                const auto alone = byHeight(36);
                const auto* next = runStarting(L"The paragraph after a picture");
                c(alone.size() == 1 && next && next->bounds.top - alone[0].bottom < 30 * scale &&
                      next->bounds.top > alone[0].bottom,
                  "a picture alone in a paragraph leaves a paragraph's gap, not a blank line, under it");

                for (auto [h, words] : {std::pair<float, const wchar_t*>{22, L"A list item with"},
                                        {24, L"An ordered item with"}}) {
                    const auto* item = runStarting(words);
                    const App::LayoutTextRun* marker = nullptr;
                    for (const auto& run : app.layoutTextRuns) {
                        if (item && run.layout && !run.docLength && run.bounds.right <= item->bounds.left &&
                            run.bounds.bottom > item->bounds.top && run.bounds.top < item->bounds.bottom)
                            marker = &run;
                    }
                    c(byHeight(h).size() == 1 && item && marker && near(runBaselineY(*marker), runBaselineY(*item)),
                      std::string("a list marker follows its grown first line: ") + (h == 22 ? "bullet" : "number"));
                }
                const auto* task = runStarting(L"A task with");
                c(task && app.taskRects.size() == 1 &&
                      near(app.taskRects[0].rect.top, task->bounds.top, 1.5f),
                  "a task's checkbox follows its grown first line");

                const auto cell = byHeight(19);
                c(cell.size() == 2 && near(cell[0].bottom, cell[1].bottom) && app.tableRects.size() == 1 &&
                      cell[1].bottom <= app.tableRects[0].bounds.bottom,
                  "two badges in a table cell share its line inside the table");
                c(byHeight(23).size() == 1 && runStarting(L"A quote with"), "a quote keeps its badge");
                c(app.docText.find(L"A missing picture [image: gone] keeps its words in the line.") != std::wstring::npos,
                  "a missing picture's alt text stays in its sentence");

                // Every text rect sits in exactly one line bucket at its top
                bool buckets = true;
                for (size_t i = 0; i < app.textRects.size(); ++i) {
                    int homes = 0;
                    for (const auto& bucket : app.lineBuckets) {
                        for (size_t idx : bucket.textRectIndices)
                            if (idx == i) {
                                ++homes;
                                buckets = buckets && near(bucket.top, app.textRects[i].rect.top, 5.01f);
                            }
                    }
                    buckets = buckets && homes == 1;
                }
                c(buckets, "every text rect keeps one line bucket at its own top");
            }
        }
    }
}

// Hover, press and release through the real handlers, as a separate click
// rather than the second half of a double-click
void click(App& app, float x, float y) {
    const LPARAM point = MAKELPARAM(static_cast<int>(x), static_cast<int>(y));
    app.lastClickTime = {};
    handleMouseMove(app, app.hwnd, point);
    handleMouseDown(app, app.hwnd, MK_LBUTTON, point);
    handleMouseUp(app, app.hwnd, 0, point);
}

// A click beside the open lightbox closes it; its release must not reach
// the page and re-open the inline image under the pointer (#249)
void lightboxDismissal(App& app) {
    ensureLayoutComplete(app);
    check(!app.layoutBitmaps.empty(), "lightbox fixture lays out inline images");
    if (app.layoutBitmaps.empty()) return;
    const D2D1_RECT_F image = app.layoutBitmaps.front().destRect;
    app.scrollX = app.targetScrollX = 0.0f;
    app.scrollY = app.targetScrollY = std::max(0.0f, image.top - dpi(app, 120.0f));
    const float x = documentViewportX(app) + (image.left + image.right) * 0.5f;
    const float y = std::min((image.top + image.bottom) * 0.5f, image.top + dpi(app, 40.0f)) -
                    app.scrollY;

    click(app, x, y);
    check(app.showLightbox, "clicking an inline image opens the lightbox");
    // Dragged aside, the zoomed image leaves the inline image uncovered
    app.lightboxPanX = static_cast<float>(app.width) * 4.0f;
    click(app, x, y);
    check(!app.showLightbox, "a click beside the zoomed image closes it for good");
    check(!app.swallowNextMouseUp, "the dismissing release is consumed exactly once");

    click(app, x, y);
    check(app.showLightbox, "the next click on the inline image opens it again");
    const D2D1_RECT_F zoomed = lightboxImageRect(app);
    click(app, (zoomed.left + zoomed.right) * 0.5f, (zoomed.top + zoomed.bottom) * 0.5f);
    check(!app.showLightbox, "a plain click on the zoomed image still closes it");
}

void remoteImages(App& app, const char* base) {
    app.clearLayoutCache();
    app.releaseImageCache();
    const std::string url = std::string(base) + "/large.jpg";
    const std::string broken = std::string(base) + "/broken.jpg";
    const std::string source = "# Remote images\n\n![Remote](" + url + ")\n\n![Broken](" + broken + ")\n";
    app.root = app.parser.parse(source).root;
    layoutDocument(app);
    check(app.imageCache.at(url).pending, "remote image starts pending");
    const auto deadline = GetTickCount64() + 15000;
    while (GetTickCount64() < deadline && (app.imageCache.at(url).pending || app.imageCache.at(broken).pending)) {
        MSG message{};
        while (PeekMessageW(&message, app.hwnd, WM_APP_IMAGE_READY, WM_APP_IMAGE_READY, PM_REMOVE))
            completeAsyncImage(app, reinterpret_cast<void*>(message.lParam));
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_POSTMESSAGE);
    }
    check(!app.imageCache.at(url).pending && !app.imageCache.at(url).failed &&
          app.imageCache.at(url).bytes <= IMAGE_DECODE_MAX_PIXELS * 4,
          "downloaded large image completes with bounded pixels");
    check(!app.imageCache.at(broken).pending && app.imageCache.at(broken).failed,
          "downloaded corrupt image completes as a failure");
    layoutDocument(app);
    check(app.layoutBitmaps.size() == 1, "remote result relayout displays image");
    app.releaseImageCache();
    drawImages(app);
}
}

int runImageTests(const char* remoteBase) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    auto state = std::make_unique<App>();
    App& app = *state;
    app.hwnd = CreateWindowExW(0, L"STATIC", L"Image regression tests", WS_POPUP,
        0, 0, 1050, 900, nullptr, nullptr, nullptr, nullptr);
    if (!app.hwnd || !initD2D(app) || !createRenderTarget(app)) return 2;
    generateAssets(app);
    decoding(app);
    ownership(app);
    mixedLayout(app);
    lightboxDismissal(app);
    htmlImages(app);
    inlineImages(app);
    if (remoteBase) remoteImages(app, remoteBase);
    DestroyWindow(app.hwnd);
    app.hwnd = nullptr;
    state.reset();
    CoUninitialize();
    std::cout << "Images: " << failures << " failures\n";
    return failures ? 1 : 0;
}
