// Admonitions in every dialect (admonition.h), on tests/fixtures/admonitions.md:
// the boxes the layout draws in two themes, two scales and two widths,
// folding, print, and the HTML and Word exports
#include "admonition.h"
#include "d2d_init.h"
#include "document.h"
#include "export.h"
#include "render.h"
#include "utils.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>

namespace {
int failures = 0;
void check(bool value, const std::string& message) {
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
std::string read(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    return {(std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()};
}
size_t count(const std::string& text, const std::string& needle) {
    size_t n = 0;
    for (size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) ++n;
    return n;
}

// Every admonition box in the tree, in document order
void boxes(const ElementPtr& e, std::vector<const Element*>& out) {
    for (const auto& c : e->children) {
        if (c->type == ElementType::BlockQuote && c->alertKind != 0) out.push_back(c.get());
        boxes(c, out);
    }
}

constexpr size_t kBoxes = 26;  // the fixture's admonitions, nested ones included

// A box's tint: the rect a little wider than its bar, in its accent at the
// tint's alpha
std::vector<D2D1_RECT_F> tints(const App& app) {
    std::vector<D2D1_RECT_F> out;
    for (const auto& r : app.layoutRects) {
        if (r.color.a > 0.05f && r.color.a < 0.2f && r.rect.right - r.rect.left > 100.0f) out.push_back(r.rect);
    }
    return out;
}

void layoutTests(App& app, const std::string& source) {
    int layouts = 0;
    for (int theme : {0, 5}) {
        applyTheme(app, theme);
        for (float scale : {1.0f, 1.5f}) {
            app.contentScale = scale;
            updateTextFormats(app);
            for (int width : {1050, 650}) {
                app.width = width;
                app.admonitionFoldToggled.clear();
                auto doc = parseDocument(app.parser, source, "admonitions.md");
                app.root = doc.root;
                app.currentFile = "admonitions.md";
                layoutDocument(app);
                ++layouts;
                const auto boxRects = tints(app);
                check(boxRects.size() == kBoxes, "every admonition draws a tinted box (" +
                                                     std::to_string(boxRects.size()) + ")");
                // No text spills out of a box: every run inside a box's
                // vertical span sits inside it horizontally
                bool contained = true;
                for (const auto& run : app.layoutTextRuns) {
                    for (const auto& box : boxRects) {
                        if (run.bounds.top >= box.top && run.bounds.bottom <= box.bottom &&
                            run.bounds.left >= box.left - 0.5f) {
                            contained = contained && run.bounds.right <= box.right + 0.5f;
                        }
                    }
                }
                check(contained, "text stays inside its box at width " + std::to_string(width));
                // Titles and labels read in the document text; folded bodies do not
                const std::wstring& text = app.docText;
                for (const wchar_t* title : {L"Note", L"Caution", L"Why are some of these folded?",
                                             L"Fast path", L"Take care", L"Click to expand", L"See also",
                                             L"Indented content", L"Folded by default", L"Warning",
                                             L"A generic admonition with its own title", L"Docsy", L"Raw HTML"}) {
                    check(text.find(title) != std::wstring::npos, "a title reads in the document text");
                }
                check(text.find(L"A minus sign after the type folds") == std::wstring::npos &&
                          text.find(L"VitePress details folds") == std::wstring::npos &&
                          text.find(L"Three question marks fold") == std::wstring::npos,
                      "folded boxes hide their bodies");
                check(text.find(L"An empty title hides") != std::wstring::npos &&
                          text.find(L"Two levels deep") != std::wstring::npos &&
                          text.find(L"Inside a code fence, markers stay text") != std::wstring::npos,
                      "bodies, nested ones and code stay laid out");
                // Each foldable box has a title bar to click: the faq, the
                // example, details and the bug
                check(app.admonitionFoldRects.size() == 4, "four foldable title bars");
                // Unfolding the faq shows its body and pushes the rest down
                if (!app.admonitionFoldRects.empty()) {
                    const float before = app.contentHeight;
                    app.admonitionFoldToggled.insert(app.admonitionFoldRects.front().key);
                    layoutDocument(app);
                    check(app.docText.find(L"A minus sign after the type folds") != std::wstring::npos &&
                              app.contentHeight > before,
                          "unfolding a box lays out its body");
                    app.admonitionFoldToggled.insert(app.admonitionFoldRects[1].key);  // the open example
                    layoutDocument(app);
                    check(app.docText.find(L"Second step") == std::wstring::npos, "folding an open box hides it");
                    app.admonitionFoldToggled.clear();
                }
                // On paper every box is open
                app.printLayout = true;
                layoutDocument(app);
                check(app.docText.find(L"Three question marks fold") != std::wstring::npos &&
                          app.admonitionFoldRects.empty(),
                      "printing opens every box and leaves nothing to click");
                app.printLayout = false;
            }
        }
    }
    check(layouts == 8, "every theme, scale and width is laid out");
}

void exportTests(App& app, const std::string& source) {
    namespace fs = std::filesystem;
    const fs::path output = fs::current_path() / "admonition-test-output";
    fs::create_directories(output);
    applyTheme(app, 0);
    updateTextFormats(app);
    auto doc = parseDocument(app.parser, source, "admonitions.md");
    app.root = doc.root;
    layoutDocument(app);
    check(exportHtmlFile(app, (output / "admonitions.html").wstring()), "HTML export succeeds");
    const std::string html = read(output / "admonitions.html");
    check(count(html, "class=\"admonition ") == kBoxes, "HTML writes every box");
    check(html.find("<details class=\"admonition question\"><summary") != std::string::npos,
          "a folded callout is a closed <details>");
    check(html.find("<details class=\"admonition example\" open><summary") != std::string::npos,
          "an open foldable callout is an open <details>");
    check(html.find("Fast <strong>path</strong></p>") != std::string::npos, "a title keeps its Markdown");
    check(html.find("<div class=\"admonition info\"><p>An empty title hides") != std::string::npos,
          "an empty MkDocs title writes no title");
    check(html.find(">See also</p>") != std::string::npos, "an untitled box writes its type's label");
    check(html.find(".admonition.bug{border-color:") != std::string::npos &&
              html.find(".admonition.caution>.admonition-title{color:") != std::string::npos,
          "the stylesheet colours every kind");
    check(html.find("<blockquote><p>A plain quote stays a quote.</p>") != std::string::npos,
          "a plain quote stays a blockquote");

    check(exportDocxFile(app, (output / "admonitions.docx").wstring()), "Word export succeeds");
    const std::string zip = read(output / "admonitions.docx");
    check(zip.find("Why are some of these folded?") != std::string::npos, "Word writes a callout's title");
    check(zip.find("A minus sign after the type folds") != std::string::npos, "Word opens a folded box");
    check(zip.find("Fast path") != std::string::npos, "Word writes a title's text");
    check(zip.find(">Caution</w:t>") == std::string::npos && zip.find("  Caution</w:t>") != std::string::npos,
          "Word labels an untitled box after its icon");
}
}  // namespace

int main() {
    auto state = std::make_unique<App>();
    auto& app = *state;
    if (!initD2D(app)) return 2;
    app.height = 900;
    app.readingWidthPct = 100;
    const std::string source = read(TINTA_ADMONITION_FIXTURE);
    {
        auto doc = parseDocument(app.parser, source, "admonitions.md");
        check(doc.success, "the admonition fixture parses");
        std::vector<const Element*> found;
        if (doc.success) boxes(doc.root, found);
        check(found.size() == kBoxes, "the fixture holds " + std::to_string(kBoxes) + " admonitions (" +
                                          std::to_string(found.size()) + ")");
    }
    layoutTests(app, source);
    exportTests(app, source);
    std::cout << "Admonitions: " << failures << " failures\n";
    return failures ? 1 : 0;
}
