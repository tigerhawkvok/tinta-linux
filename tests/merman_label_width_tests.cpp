// MM2: merman sizes diagram shapes from label widths measured in the face
// that draws them (render.cpp labelWidth, through DirectWrite; Pango here),
// not from its own estimates. Each shape below must hold its label as
// DejaVu Sans sets it; merman's estimates, the control, do not.
#include "d2d_init.h"
#include "utils.h"  // toWide
#include "merman_engine.h"
#include "render.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        failures++;
        std::cerr << "FAIL: " << what << "\n";
    }
}

// The advance of `text` in `family`, as the diagram draws it
float faceWidth(App& app, const wchar_t* family, float size, DWRITE_FONT_WEIGHT weight, const std::string& text) {
    std::wstring wide = toWide(text);
    IDWriteTextFormat* format = nullptr;
    IDWriteTextLayout* layout = nullptr;
    float width = -1.0f;
    if (SUCCEEDED(app.dwriteFactory->CreateTextFormat(family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                                      DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &format)) &&
        SUCCEEDED(app.dwriteFactory->CreateTextLayout(wide.c_str(), (UINT32)wide.size(), format, 1.0e6f, 1.0e6f,
                                                      &layout))) {
        layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        DWRITE_TEXT_METRICS metrics{};
        layout->GetMetrics(&metrics);
        width = metrics.widthIncludingTrailingWhitespace;
    }
    if (layout) layout->Release();
    if (format) format->Release();
    return width;
}

struct Shape {
    float x = 0, y = 0, width = -1, height = -1;  // centre and size
    std::vector<std::string> lines;  // the label's rows as drawn
};

float attr(const std::string& svg, size_t from, size_t to, const char* name) {
    size_t at = svg.find(std::string(" ") + name + "=\"", from);
    return at < to ? std::strtof(svg.c_str() + at + std::strlen(name) + 3, nullptr) : -1.0f;
}

// The node whose id starts with `id`: its centre (its transform), its
// outline (a <rect>, or a path from its top-left corner: class boxes), and
// the label rows drawn at its centre line inside it
Shape shapeOf(const std::string& svg, const std::string& id) {
    Shape shape;
    size_t at = svg.find("id=\"" + id);
    if (at == std::string::npos) return shape;
    size_t translate = svg.find("translate(", at);
    size_t rect = svg.find("<rect", at);
    size_t path = svg.find("<path d=\"M", at);
    if (translate == std::string::npos || (rect == std::string::npos && path == std::string::npos)) return shape;
    char* end = nullptr;
    shape.x = std::strtof(svg.c_str() + translate + 10, &end);
    shape.y = std::strtof(end + 1, nullptr);
    if (rect < path) {
        size_t rectEnd = svg.find('>', rect);
        shape.width = attr(svg, rect, rectEnd, "width");
        shape.height = attr(svg, rect, rectEnd, "height");
    } else {
        // M x1 y1 L x2 y2 L x3 y3: the top edge, then the right one
        float p[6];
        const char* c = svg.c_str() + path + 10;
        for (float& v : p) {
            while (*c && (*c == ' ' || *c == 'L' || *c == ',')) c++;
            v = std::strtof(c, &end);
            c = end;
        }
        shape.width = std::fabs(p[2] - p[0]);
        shape.height = std::fabs(p[5] - p[3]);
    }
    for (size_t text = svg.find("<text "); text != std::string::npos; text = svg.find("<text ", text + 1)) {
        size_t open = svg.find('>', text);
        float x = attr(svg, text, open, "x"), y = attr(svg, text, open, "y");
        if (std::fabs(x - shape.x) > 0.5f || std::fabs(y - shape.y) > shape.height / 2) continue;
        size_t close = svg.find("</text>", open);
        std::string row = svg.substr(open + 1, close - open - 1);
        while (!row.empty() && row.back() == ' ') row.pop_back();
        if (!row.empty()) shape.lines.push_back(row);
    }
    return shape;
}

// The widest drawn row of a shape's label
float labelWidth(App& app, const Shape& shape, float size, bool bold) {
    float widest = -1.0f;
    for (const auto& row : shape.lines) {
        widest = std::max(widest, faceWidth(app, L"DejaVu Sans", size,
                                            bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL, row));
    }
    return widest;
}

struct Case {
    const char* what;
    std::string source;
    const char* id;  // the shape's element id prefix
    bool bold;
    bool overflowsEstimated;  // merman's own estimate is too narrow
};

}  // namespace

int main() {
    auto state = std::make_unique<App>();
    App& app = *state;
    app.hwnd = CreateWindowExW(0, L"STATIC", L"Label width tests", WS_POPUP, 0, 0, 900, 600, nullptr, nullptr,
                               nullptr, nullptr);
    if (!app.hwnd || !initD2D(app) || !createRenderTarget(app)) return 2;
    app.width = 900;
    app.height = 600;
    app.contentScale = 1.0f;
    updateTextFormats(app);
    // A face far from merman's built-in metrics
    app.theme.fontFamily = L"DejaVu Sans";
    const float size = app.textFormat->GetFontSize();

    const Case cases[] = {
        {"a state", "stateDiagram-v2\n    [*] --> WaitingForTheHostToAnswer\n", "merman-state-WaitingForTheHostToAnswer",
         false, true},
        {"a class box", "classDiagram\n    class WWWWMMMM\n    class Other\n    Other <|-- WWWWMMMM\n",
         "merman-classId-WWWWMMMM", true, true},
        // (the flowchart's minimum width hides merman's estimates here)
        {"a wrapped flowchart node", "flowchart LR\n    A[WWWW MMMM WWW] --> B[ok]\n", "merman-flowchart-A", false,
         false},
        {"a flowchart node", "flowchart LR\n    A[WWWWMMMM] --> B[ok]\n", "merman-flowchart-A", false, false},
    };

    // The control: merman's estimates leave DejaVu Sans overflowing, so the
    // check below can fail
    setenv("TINTA_MERMAN_ESTIMATED_LABELS", "1", 1);
    for (const Case& c : cases) {
        std::string svg;
        float w = 0, h = 0;
        check(mermanDiagramSvg(app, c.source + "    %% control\n", false, svg, w, h),
              std::string("the control renders ") + c.what);
        Shape shape = shapeOf(svg, c.id);
        float text = labelWidth(app, shape, size, c.bold);
        if (c.overflowsEstimated) {
            check(shape.width > 0 && text > shape.width,
                  std::string("the checker: estimated, ") + c.what + " is narrower than its label (" +
                      std::to_string(shape.width) + " < " + std::to_string(text) + ")");
        }
    }
    unsetenv("TINTA_MERMAN_ESTIMATED_LABELS");

    // Measured: every shape holds its label, without floating in it
    for (const Case& c : cases) {
        std::string svg;
        float w = 0, h = 0;
        check(mermanDiagramSvg(app, c.source, false, svg, w, h), std::string("renders ") + c.what);
        Shape shape = shapeOf(svg, c.id);
        float text = labelWidth(app, shape, size, c.bold);
        bool fits = !shape.lines.empty() && shape.width >= text && shape.width <= text + 4.0f * size;
        check(fits, std::string(c.what) + " holds its label: shape " + std::to_string(shape.width) +
                        ", widest row " + std::to_string(text) + " of " + std::to_string(shape.lines.size()));
        if (!fits) std::ofstream(std::string(c.id) + ".svg", std::ios::binary) << svg;  // to look at
    }

    // The hook itself: merman sizes shapes from whatever the host reports
    merman_engine::Theme wide;
    wide.fontFamily = "DejaVu Sans";
    wide.fontSize = 16.0f;
    merman_engine::setTextWidth([](const merman_engine::TextWidthQuery& q, void*) { return 40.0 * q.text.size(); },
                                nullptr);
    merman_engine::Diagram diagram =
        merman_engine::renderSvg("stateDiagram-v2\n    [*] --> Hooked\n", wide);
    float hooked = shapeOf(diagram.svg, "merman-state-Hooked").width;
    check(diagram.ok && hooked >= 240.0f, "a host's widths size the shape: " + std::to_string(hooked));
    check(merman_engine::cssFontWeight("bold") == 700 && merman_engine::cssFontWeight(" 600 ") == 600 &&
              merman_engine::cssFontWeight("normal") == 400 && merman_engine::cssFontWeight("lighter") == 300,
          "CSS weights");

    DestroyWindow(app.hwnd);
    std::cout << "merman label widths: " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
