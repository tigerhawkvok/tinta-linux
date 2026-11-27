// MM3 (docs/linux-port/PLAN.md §6): Mermaid fences in the viewer draw
// through merman, as images rasterized at the device pixels they cover,
// with the native renderer and then the code block as fallbacks.
// Fixture: tests/fixtures/mermaid-merman.md (four diagrams the native
// parser rejects, then one invalid source).
#include "d2d_init.h"
#include "editor.h"  // toUtf8
#include "export.h"
#include "render.h"
#include "search.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        failures++;
        std::cerr << "FAIL: " << what << "\n";
    }
}

std::string read(const char* path) {
    std::ifstream file(path, std::ios::binary);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

int diagramCount(const App& app) {
    int n = 0;
    for (const auto& block : app.codeBlocks) n += block.isDiagram ? 1 : 0;
    return n;
}

// Every diagram bitmap is drawn 1:1: its pixels exactly fill its rectangle,
// which starts on a whole pixel (anything else resamples and blurs it)
bool drawnOneToOne(const App& app, std::string& detail) {
    for (const auto& item : app.layoutBitmaps) {
        D2D1_SIZE_U px = item.bitmap->GetPixelSize();
        const D2D1_RECT_F& r = item.destRect;
        float w = r.right - r.left;
        float h = r.bottom - r.top;
        if (px.width != w || px.height != h || r.left != std::round(r.left) || r.top != std::round(r.top)) {
            detail = std::to_string(px.width) + "x" + std::to_string(px.height) + " bitmap in a " +
                     std::to_string(w) + "x" + std::to_string(h) + " rectangle at " + std::to_string(r.left) +
                     "," + std::to_string(r.top);
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    auto state = std::make_unique<App>();
    App& app = *state;
    app.hwnd = CreateWindowExW(0, L"STATIC", L"merman layout tests", WS_POPUP, 0, 0, 1050, 900, nullptr,
                               nullptr, nullptr, nullptr);
    if (!app.hwnd || !initD2D(app) || !createRenderTarget(app)) return 2;
    app.width = 1050;
    app.height = 900;
    app.readingWidthPct = 100;
    app.contentScale = 1.0f;
    applyTheme(app, 0);  // creates the text formats
    updateTextFormats(app);
    app.root = app.parser.parse(read(TINTA_MERMAN_FIXTURE)).root;

    // The four diagrams render through merman; the invalid source stays code
    layoutDocument(app);
    int diagrams = diagramCount(app);
    check(diagrams == 4, "4 diagrams drawn, got " + std::to_string(diagrams));
    check(app.layoutBitmaps.size() == 4, "one bitmap per diagram, got " + std::to_string(app.layoutBitmaps.size()));
    int code = 0;
    for (const auto& block : app.codeBlocks) code += block.isDiagram ? 0 : 1;
    check(code >= 1, "the invalid Mermaid source falls back to a code block");
    std::string detail;
    bool oneToOne = drawnOneToOne(app, detail);  // fills detail first
    check(oneToOne, "diagrams rasterized at their device size: " + detail);
    float width1 = app.layoutBitmaps.empty() ? 0.0f
                                             : app.layoutBitmaps[0].destRect.right - app.layoutBitmaps[0].destRect.left;

    // Zoom re-rasterizes at the new size (crisp, not scaled up)
    app.zoomFactor = 2.0f;
    layoutDocument(app);
    float width2 = app.layoutBitmaps.empty() ? 0.0f
                                             : app.layoutBitmaps[0].destRect.right - app.layoutBitmaps[0].destRect.left;
    check(std::fabs(width2 - 2.0f * width1) <= 2.0f,
          "2x zoom doubles the first diagram: " + std::to_string(width1) + " -> " + std::to_string(width2));
    oneToOne = drawnOneToOne(app, detail);
    check(oneToOne, "zoomed diagrams rasterized at their device size: " + detail);
    app.zoomFactor = 1.0f;

    // A theme change renders again in its colours (a different bitmap)
    layoutDocument(app);
    ID2D1Bitmap* before = app.layoutBitmaps.empty() ? nullptr : app.layoutBitmaps[0].bitmap.Get();
    int wombat = -1;
    for (int i = 0; i < themeCount(); i++) {
        if (std::wstring(themeAt(i).name) == L"Wombat") wombat = i;
    }
    applyTheme(app, wombat);
    layoutDocument(app);
    ID2D1Bitmap* after = app.layoutBitmaps.empty() ? nullptr : app.layoutBitmaps[0].bitmap.Get();
    check(before && after && before != after, "a theme change draws the diagram again");

    // Recreating the render target (the switch to the GPU after startup, a
    // DPI change) drops every device bitmap. The diagrams come back; on the
    // PNG path from merman's cached PNG, not a second merman render
    size_t rendersBefore = app.diagramMetrics.rasters;
    check(createRenderTarget(app), "the render target is recreated");
    layoutDocument(app);
    check(app.layoutBitmaps.size() == 4,
          "diagrams drawn again on the new render target, got " + std::to_string(app.layoutBitmaps.size()));
#ifdef TINTA_MERMAN_PNG
    check(app.diagramMetrics.rasters == rendersBefore,
          "no merman render for a new render target, got " +
              std::to_string(app.diagramMetrics.rasters - rendersBefore));
    std::cout << "merman PNG path: render target recreation checked\n";
#else
    (void)rendersBefore;
#endif

    // Narrow window: a wide diagram may overflow, and fit-to-width (the
    // reader's toggle) shrinks it into the column
    app.width = 420;
    layoutDocument(app);
    const App::CodeBlockInfo* wide = nullptr;
    for (const auto& block : app.codeBlocks) {
        if (block.isDiagram && block.fitCandidate && !block.fitActive) {
            wide = &block;
            break;
        }
    }
    check(wide != nullptr, "a diagram wider than a 420 px window offers fit-to-width");
    if (wide) {
        // Unfitted, the page scrolls sideways to its right edge
        check(app.contentWidth >= wide->bounds.right,
              "a wide diagram scrolls sideways: content " + std::to_string(app.contentWidth) +
                  " px, diagram right edge " + std::to_string(wide->bounds.right));
        float natural = wide->bounds.right - wide->bounds.left;
        unsigned key = wide->fitKey;
        for (const auto& block : app.codeBlocks) {
            if (block.isDiagram && block.fitCandidate && !block.fitActive) app.fitBlocks.push_back(block.fitKey);
        }
        layoutDocument(app);
        const App::CodeBlockInfo* fitted = nullptr;
        for (const auto& block : app.codeBlocks) {
            if (block.isDiagram && block.fitKey == key) fitted = &block;
        }
        float width = fitted ? fitted->bounds.right - fitted->bounds.left : 0.0f;
        check(fitted && width < natural && fitted->bounds.right <= app.width,
              "fit-to-width shrinks it into the column: " + std::to_string(natural) + " -> " +
                  std::to_string(width));
        oneToOne = drawnOneToOne(app, detail);
        check(oneToOne, "fitted diagrams rasterized at their device size: " + detail);
        // With every wide diagram fitted, nothing scrolls sideways
        check(app.contentWidth <= app.width,
              "fitted, the page does not scroll sideways: content " + std::to_string(app.contentWidth) + " px");
    }

    // Search reads a diagram's Mermaid source: it is a picture, with no text
    // runs. "finalizeA" is only in the first diagram's source (3 times)
    app.searchQuery = L"finalizeA";
    performSearch(app);
    int firstDiagram = -1;
    for (size_t b = 0; b < app.codeBlocks.size() && firstDiagram < 0; b++) {
        if (app.codeBlocks[b].searchSource) firstDiagram = (int)b;
    }
    bool allInDiagram = app.searchMatches.size() == 3 && firstDiagram >= 0;
    for (const auto& m : app.searchMatches) {
        allInDiagram = allInDiagram && m.codeBlock == firstDiagram && m.codeLength == 9 &&
                       m.highlightRect.top == app.codeBlocks[firstDiagram].bounds.top;
    }
    check(allInDiagram, "search finds a diagram's source text, placed on the diagram: " +
                            std::to_string(app.searchMatches.size()) + " matches");
    // In text and source alike, in reading order: the table and the line
    // before the diagram say "fork", then the diagram's one <<fork>> line
    app.searchQuery = L"fork";
    performSearch(app);
    size_t inText = 0, inSource = 0;
    bool ordered = true;
    for (size_t i = 0; i < app.searchMatches.size(); i++) {
        const auto& m = app.searchMatches[i];
        (m.codeBlock >= 0 ? inSource : inText)++;
        if (i > 0) ordered = ordered && app.searchMatches[i - 1].startPos <= m.startPos;
        if (m.codeBlock >= 0 && i > 0) {
            ordered = ordered && app.searchMatches[i - 1].highlightRect.top <= m.highlightRect.top;
        }
    }
    check(inText >= 2 && inSource == 1 && ordered,
          "text and diagram matches interleave in reading order: " + std::to_string(inText) + " in text, " +
              std::to_string(inSource) + " in source");
    for (size_t i = 0; i < app.searchMatches.size(); i++) {
        if (app.searchMatches[i].codeBlock < 0) continue;
        app.searchCurrentIndex = (int)i;
        app.scrollY = app.targetScrollY = 0;
        scrollToCurrentMatch(app);
        const D2D1_RECT_F& b = app.codeBlocks[app.searchMatches[i].codeBlock].bounds;
        check(b.bottom > app.scrollY && b.top < app.scrollY + app.height,
              "a diagram match scrolls its diagram into view");
        break;
    }
    app.searchQuery.clear();
    performSearch(app);

    // "Copy diagram as image" (the hover button): the first diagram is one
    // only merman draws; the clipboard gets its PNG and a CF_DIB
    const App::CodeBlockInfo* first = nullptr;
    for (const auto& block : app.codeBlocks) {
        if (block.isDiagram) {
            first = &block;
            break;
        }
    }
    check(first && copyDiagramImage(app, app.hwnd, toUtf8(first->codeText)), "copy diagram as image");
    if (OpenClipboard(app.hwnd)) {
        HANDLE png = GetClipboardData(RegisterClipboardFormatW(L"PNG"));
        const auto* bytes = png ? static_cast<const unsigned char*>(GlobalLock(png)) : nullptr;
        check(bytes && GlobalSize(png) > 1000 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G',
              "the clipboard holds the diagram as PNG");
        if (bytes) GlobalUnlock(png);
        check(IsClipboardFormatAvailable(CF_DIB), "and as CF_DIB");
        CloseClipboard();
    }

    DestroyWindow(app.hwnd);
    state.reset();
    std::cout << "merman layout: " << failures << " failures\n";
    return failures ? 1 : 0;
}
