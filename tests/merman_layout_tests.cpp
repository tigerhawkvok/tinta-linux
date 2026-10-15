// MM3 (docs/linux-port/PLAN.md §6): Mermaid fences in the viewer draw
// through merman, as images rasterized at the device pixels they cover,
// with the native renderer and then the code block as fallbacks.
// Fixture: tests/fixtures/mermaid-merman.md (four diagrams the native
// parser rejects, then one invalid source).
#include "d2d_init.h"
#include "render.h"

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

// Every diagram bitmap is drawn 1:1: its pixels match its rectangle
bool drawnOneToOne(const App& app, std::string& detail) {
    for (const auto& item : app.layoutBitmaps) {
        D2D1_SIZE_U px = item.bitmap->GetPixelSize();
        float w = item.destRect.right - item.destRect.left;
        float h = item.destRect.bottom - item.destRect.top;
        if (std::fabs(px.width - w) > 1.0f || std::fabs(px.height - h) > 1.0f) {
            detail = std::to_string(px.width) + "x" + std::to_string(px.height) + " bitmap in a " +
                     std::to_string(w) + "x" + std::to_string(h) + " rectangle";
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
        float natural = wide->bounds.right - wide->bounds.left;
        unsigned key = wide->fitKey;
        app.fitBlocks.push_back(key);
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
    }

    DestroyWindow(app.hwnd);
    state.reset();
    std::cout << "merman layout: " << failures << " failures\n";
    return failures ? 1 : 0;
}
