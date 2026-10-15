// List items stand apart (render.cpp layoutList): at least half a body line
// of space between items, so one-line items advance 1.5 lines, while an
// item's own wrapped lines keep the paragraph pitch, and loose lists (whose
// paragraphs already leave space) are not spaced twice.
#include "d2d_init.h"
#include "render.h"

#include <algorithm>
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

// Tops of the list markers (bullets and numbers): runs outside the text
struct Marker {
    float top;
    float left;
};

std::vector<Marker> markers(const App& app) {
    std::vector<Marker> out;
    for (const auto& run : app.layoutTextRuns) {
        if (run.docLength == 0 && !run.selectable) out.push_back({run.bounds.top, run.bounds.left});
    }
    std::sort(out.begin(), out.end(), [](const Marker& a, const Marker& b) { return a.top < b.top; });
    return out;
}

// The lowest text bottom above `y` (the end of the item before it)
float textBottomAbove(const App& app, float y) {
    float bottom = 0.0f;
    for (const auto& run : app.layoutTextRuns) {
        if (run.docLength > 0 && run.bounds.top < y - 1.0f) bottom = std::max(bottom, run.bounds.bottom);
    }
    return bottom;
}

void layout(App& app, const std::string& markdown) {
    app.root = app.parser.parse(markdown).root;
    layoutDocument(app);
}

}  // namespace

int main() {
    auto state = std::make_unique<App>();
    App& app = *state;
    app.hwnd = CreateWindowExW(0, L"STATIC", L"list spacing tests", WS_POPUP, 0, 0, 1050, 900, nullptr, nullptr,
                               nullptr, nullptr);
    if (!app.hwnd || !initD2D(app) || !createRenderTarget(app)) return 2;
    app.width = 1050;
    app.height = 900;
    app.readingWidthPct = 100;
    app.contentScale = 1.0f;
    applyTheme(app, 0);
    updateTextFormats(app);
    const float line = app.textFormat->GetFontSize() * 1.7f;  // the body line pitch

    // One-line items advance at least 1.5 lines
    layout(app, "- one\n- two\n- three\n");
    auto m = markers(app);
    check(m.size() == 3, "three markers, got " + std::to_string(m.size()));
    for (size_t i = 1; i < m.size(); i++) {
        float pitch = m[i].top - m[i - 1].top;
        check(pitch >= 1.5f * line - 0.5f, "item pitch " + std::to_string(pitch) + " >= 1.5 x " + std::to_string(line));
    }

    // A wrapped item: half a line of air before the next one
    app.width = 420;
    layout(app, "- a first item long enough that it has to wrap onto more lines at this narrow width, "
                "and then some more words so that it certainly does\n- second\n");
    m = markers(app);
    check(m.size() == 2, "two markers in the wrapped list");
    if (m.size() == 2) {
        float gap = m[1].top - textBottomAbove(app, m[1].top);
        check(gap >= 0.5f * line - 1.0f, "space after a wrapped item " + std::to_string(gap) + " >= half a line");
        check(m[1].top - m[0].top > 2.5f * line, "the first item really wraps");
    }

    // Nested items follow the same rule
    app.width = 1050;
    layout(app, "1. parent\n   - child one\n   - child two\n2. next\n");
    m = markers(app);
    check(m.size() == 4, "four markers in the nested list");
    if (m.size() == 4) {
        float nested = m[2].top - m[1].top;
        check(nested >= 1.5f * line - 0.5f, "nested item pitch " + std::to_string(nested));
    }

    // An item ending in a quote: the quote's own bottom space counts, so the
    // next item comes half a line (not a line) after the quoted text
    layout(app, "1. first\n\n   > quoted text inside the item\n2. second\n");
    m = markers(app);
    check(m.size() == 2, "two markers around the quote");
    if (m.size() == 2) {
        float gap = m[1].top - textBottomAbove(app, m[1].top);
        check(gap >= 0.5f * line - 1.0f && gap <= 0.5f * line + 2.0f,
              "space after an item ending in a quote " + std::to_string(gap) + " is half a line, once");
    }

    // Loose items (paragraphs) are not spaced twice
    layout(app, "- loose one\n\n- loose two\n\n- loose three\n");
    m = markers(app);
    check(m.size() == 3, "three markers in the loose list");
    for (size_t i = 1; i < m.size(); i++) {
        float pitch = m[i].top - m[i - 1].top;
        check(pitch >= 1.5f * line - 0.5f && pitch < 1.75f * line,
              "loose item pitch " + std::to_string(pitch) + " in [1.5, 1.75) lines");
    }

    DestroyWindow(app.hwnd);
    state.reset();
    std::cout << "list spacing: " << failures << " failures\n";
    return failures ? 1 : 0;
}
