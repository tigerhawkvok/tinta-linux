// Inline code sits on the surrounding text's baseline (#262) at 7/8 of its
// size (fork #1, headings included), in every context of the mixed fixtures,
// for several code fonts, themes, scales and widths. The chip (the line box)
// still contains the code glyphs.
#include "d2d_init.h"
#include "export.h"
#include "render.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
int failures = 0, comparisons = 0, sizeComparisons = 0;
void check(bool value, const std::string& message) {
    if (!value) {
        if (failures < 20) std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

std::wstring familyOf(IDWriteTextLayout* layout) {
    UINT32 length = layout->GetFontFamilyNameLength();
    std::wstring name(length + 1, L'\0');
    layout->GetFontFamilyName(name.data(), length + 1);
    name.resize(length);
    return name;
}

struct Line {
    float baseline = 0, height = 0;
};
Line firstLine(IDWriteTextLayout* layout) {
    DWRITE_LINE_METRICS metrics[64];
    UINT32 count = 0;
    layout->GetLineMetrics(metrics, 64, &count);
    return count ? Line{metrics[0].baseline, metrics[0].height} : Line{};
}

std::string utf8(const std::wstring& text) {
    std::string out;
    for (wchar_t c : text) out += c < 0x80 ? static_cast<char>(c) : '?';
    return out;
}

void checkBaselines(App& app, const std::string& where) {
    const std::wstring code = app.theme.codeFontFamily;
    struct Run {
        const App::LayoutTextRun* run;
        bool isCode;
        float size, baseline;
    };
    std::vector<Run> runs;
    for (const auto& run : app.layoutTextRuns) {
        if (!run.layout || !run.docLength) continue;
        // Fenced blocks share the code face but are not inline code
        bool fenced = false;
        for (const auto& block : app.codeBlocks) {
            if (run.pos.y >= block.bounds.top && run.pos.y < block.bounds.bottom) fenced = true;
        }
        if (fenced) continue;
        Line line = firstLine(run.layout);
        runs.push_back({&run, familyOf(run.layout) == code, run.layout->GetFontSize(), run.pos.y + line.baseline});
        if (runs.back().isCode) {
            // The code glyphs' line box stays inside the text line
            check(run.pos.y >= run.bounds.top - 0.5f && run.pos.y + line.height <= run.bounds.bottom + 0.5f,
                  where + ": code stays inside its line box: " +
                      utf8(app.docText.substr(run.docStart, run.docLength)));
            // ...and inside a chip that hugs it: padding above and below
            // within 1 px of each other unless the line box clips the chip
            const D2D1_RECT_F* chip = nullptr;
            for (const auto& rect : app.layoutRects) {
                if (rect.rect.left <= run.pos.x && rect.rect.right >= run.pos.x + 1 &&
                    rect.rect.top <= run.pos.y + line.height && rect.rect.bottom >= run.pos.y &&
                    rect.rect.top >= run.bounds.top - 0.5f && rect.rect.bottom <= run.bounds.bottom + 0.5f) {
                    chip = &rect.rect;
                }
            }
            check(chip != nullptr, where + ": `" + utf8(app.docText.substr(run.docStart, run.docLength)) +
                                       "` has a chip in its line (text at " + std::to_string(run.pos.x) + "," +
                                       std::to_string(run.pos.y) + ", line " + std::to_string(run.bounds.top) +
                                       ".." + std::to_string(run.bounds.bottom) + ")");
            if (chip) {
                float above = run.pos.y - chip->top, below = chip->bottom - (run.pos.y + line.height);
                bool clipped = chip->top <= run.bounds.top + 0.5f || chip->bottom >= run.bounds.bottom - 0.5f;
                check(above >= -0.5f && below >= -0.5f, where + ": the chip covers the code glyphs");
                const float pad = run.layout->GetFontSize() * 0.2f + 1.0f;
                check(above <= pad && below <= pad,
                      where + ": the chip hugs the code, not the whole line (" + std::to_string(above) + " above, " +
                          std::to_string(below) + " below)");
                check(clipped || std::fabs(above - below) < 1.0f,
                      where + ": the chip is centred on the code (" + std::to_string(above) + " above, " +
                          std::to_string(below) + " below)");
            }
        }
    }
    for (const Run& c : runs) {
        if (!c.isCode) continue;
        // Body text of the same line: the largest non-code run there (so
        // superscripts and subscripts are not the reference)
        const Run* text = nullptr;
        for (const Run& t : runs) {
            if (t.isCode || std::fabs(t.run->bounds.top - c.run->bounds.top) > 0.5f) continue;
            if (!text || t.size > text->size) text = &t;
        }
        if (!text) continue;
        ++comparisons;
        check(std::fabs(c.baseline - text->baseline) < 0.75f,
              where + ": `" + utf8(app.docText.substr(c.run->docStart, c.run->docLength)) +
                  "` baseline " + std::to_string(c.baseline) + " vs text " + std::to_string(text->baseline));
        // ...and its size is 7/8 of that text's, as body code (14 px) is to
        // body text (16 px): a heading's code grows with the heading (#1)
        const float expected = app.codeFormat->GetFontSize() * text->size / app.textFormat->GetFontSize();
        ++sizeComparisons;
        check(std::fabs(c.size - expected) < 0.3f,
              where + ": `" + utf8(app.docText.substr(c.run->docStart, c.run->docLength)) + "` is " +
                  std::to_string(c.size) + " px beside " + std::to_string(text->size) + " px text, expected " +
                  std::to_string(expected));
    }
}

// The reported case (#1): a heading that is only code has no text beside
// it, so its size comes from the heading level
void checkCodeOnlyHeading(App& app, const std::string& where) {
    const float expected =
        app.codeFormat->GetFontSize() * app.headingFormats[1]->GetFontSize() / app.textFormat->GetFontSize();
    int found = 0;
    for (const auto& run : app.layoutTextRuns) {
        if (!run.layout || app.docText.substr(run.docStart, run.docLength) != L"git") continue;
        ++found;
        check(std::fabs(run.layout->GetFontSize() - expected) < 0.3f,
              where + ": `## `git`` code is " + std::to_string(run.layout->GetFontSize()) + " px, expected " +
                  std::to_string(expected));
    }
    check(found == 1, where + ": the code-only heading renders once");
}

// The run properties of the run that writes `text` in the Word export
std::string runPropsOf(const std::string& xml, const std::string& text) {
    size_t at = xml.find("<w:t xml:space=\"preserve\">" + text + "</w:t>");
    if (at == std::string::npos) return "";
    size_t props = xml.rfind("<w:rPr>", at);
    size_t run = xml.rfind("<w:r>", at);
    return props == std::string::npos || props < run ? "" : xml.substr(props, at - props);
}

// Word sizes inline code the same way: 7/8 of the paragraph's text, in
// half-points (H2 39 -> 34, body 24 -> 21)
void checkWordExport(App& app, const std::string& source) {
    auto doc = app.parser.parse(source);
    app.root = doc.root;
    layoutDocument(app);
    const std::filesystem::path output = std::filesystem::current_path() / "heading-code-test-output";
    std::filesystem::create_directories(output);
    const std::filesystem::path path = output / "heading-inline-code.docx";
    check(exportDocxFile(app, path.wstring()), "Word export succeeds");
    std::ifstream file(path, std::ios::binary);
    const std::string zip{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    const std::string heading = runPropsOf(zip, "git"), body = runPropsOf(zip, "body code");
    check(heading.find("<w:sz w:val=\"34\"/>") != std::string::npos,
          "Word: `## `git`` code is 17 pt (" + heading + ")");
    check(body.find("<w:sz w:val=\"21\"/>") != std::string::npos, "Word: body code stays 10.5 pt (" + body + ")");
}
}  // namespace

std::string readFile(const char* path) {
    std::ifstream file(path, std::ios::binary);
    std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    check(!source.empty(), std::string("fixture exists: ") + path);
    return source;
}

int main() {
    const std::string fixtures[] = {readFile(TINTA_INLINE_CODE_FIXTURE), readFile(TINTA_HEADING_CODE_FIXTURE)};
    {
        auto state = std::make_unique<App>();
        App& app = *state;
        if (!initD2D(app)) return 2;
        app.height = 900;
        app.readingWidthPct = 100;
        // Body and code faces with different ascents; Noto Sans with MonoLisa
        // is the reporter's own pair
        struct Faces {
            const wchar_t* body;
            const wchar_t* code;
        };
        for (Faces faces : {Faces{nullptr, L"Monaspace Argon"}, Faces{nullptr, L"Consolas"}, Faces{nullptr, L"Cascadia Code"},
                            Faces{nullptr, L"Courier New"}, Faces{nullptr, L"Lucida Console"},
                            Faces{L"Noto Sans", L"MonoLisa"}}) {
            const wchar_t* codeFont = faces.code;
            for (int theme : {0, 5}) {
                applyTheme(app, theme);
                if (faces.body) app.theme.fontFamily = faces.body;
                app.theme.codeFontFamily = codeFont;
                for (float scale : {1.0f, 1.5f}) {
                    app.contentScale = scale;
                    updateTextFormats(app);
                    for (int width : {1050, 650}) {
                        app.width = width;
                        for (size_t f = 0; f < 2; ++f) {
                            auto doc = app.parser.parse(fixtures[f]);
                            check(doc.success, "fixture parses");
                            app.root = doc.root;
                            layoutDocument(app);
                            check(app.docText.find(L"trailing code") != std::wstring::npos,
                                  "final paragraph renders");
                            check(app.tableRects.size() == 1 && app.codeBlocks.size() == 1,
                                  "table and fenced block survive");
                            const std::string where = std::string(f ? "headings " : "#262 ") + utf8(codeFont) +
                                                      " theme " + std::to_string(theme) + " x" +
                                                      std::to_string(scale) + " " + std::to_string(width) + "px";
                            checkBaselines(app, where);
                            if (f == 1) checkCodeOnlyHeading(app, where);
                        }
                    }
                }
            }
        }
        checkWordExport(app, fixtures[1]);
    }
    CoUninitialize();
    check(comparisons > 100, "code runs were compared with their lines: " + std::to_string(comparisons));
    check(sizeComparisons == comparisons, "every compared code run had its size checked");
    std::cout << "Inline code baseline and size: " << comparisons << " comparisons, " << failures << " failures\n";
    return failures ? 1 : 0;
}
