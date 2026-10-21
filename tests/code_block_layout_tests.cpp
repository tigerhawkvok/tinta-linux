#include "d2d_init.h"
#include "document.h"
#include "input.h"
#include "render.h"
#include "utils.h"

#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>

namespace {
int failures = 0;
void check(bool value, const std::string& message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
bool closeEnough(float a, float b) { return std::abs(a - b) < 0.05f; }

void checkDocument(App& app, const qmd::ParseResult& doc, int rows,
                   const std::wstring& source, const std::wstring& copy,
                   const char* label) {
    check(doc.success, std::string(label) + ": document parses");
    app.root = doc.root;
    layoutDocument(app);
    check(app.codeBlocks.size() == 1, std::string(label) + ": one code block");
    if (app.codeBlocks.size() != 1) return;
    const auto& block = app.codeBlocks[0];
    float scale = app.contentScale * app.zoomFactor;
    float expectedHeight = (rows * 20.0f + 24.0f) * scale;
    check(closeEnough(block.bounds.bottom - block.bounds.top, expectedHeight),
          std::string(label) + ": actual rows plus top/bottom padding");
    check(block.codeText == source, std::string(label) + ": block source retains all newlines");
    // #269: the copy button leaves out only the last line's break
    check(codeCopyText(block.codeText) == copy,
          std::string(label) + ": copied text ends at the last line, blank lines kept");
    check(app.docText.find(source) != std::wstring::npos,
          std::string(label) + ": selectable/searchable text retains all newlines");
    for (const auto& run : app.layoutTextRuns) {
        check(run.pos.y >= block.bounds.top + 12.0f * scale - 0.05f &&
              run.pos.y + 20.0f * scale <= block.bounds.bottom - 12.0f * scale + 0.05f,
              std::string(label) + ": no text layout exists in a phantom bottom row");
    }
}

void testCases(App& app) {
    struct Case { const char* code; int rows; const wchar_t* copy; };
    for (const auto& item : {
            Case{"single line", 1, L"single line"},
            Case{"single line\n", 1, L"single line"},
            Case{"single line\r\n", 1, L"single line"},
            Case{"first\nsecond\n", 2, L"first\nsecond"},
            Case{"first\n\nlast\n", 3, L"first\n\nlast"},
            Case{"line\n\n", 2, L"line\n"},
            Case{"line\n\n\n", 3, L"line\n\n"},
            Case{"line\r\n\r\n", 2, L"line\r\n"},
            Case{"\n", 1, L""}, Case{"\n\n", 2, L"\n"}, Case{"", 1, L""}}) {
        for (const char* path : {"fixture.txt", "fixture.json"}) {
            checkDocument(app, parseDocument(app.parser, item.code, std::string_view(path)),
                          item.rows, toWide(item.code), item.copy, "raw code");
        }
    }
    checkDocument(app, app.parser.parse("```text\nsingle line\n```\n"),
                  1, L"single line\n", L"single line", "reporter LF fence");
    checkDocument(app, app.parser.parse("```text\r\nsingle line\r\n```\r\n"),
                  1, L"single line\n", L"single line", "CRLF fence");
    checkDocument(app, app.parser.parse("```cpp\nint x = 1;\n```"),
                  1, L"int x = 1;\n", L"int x = 1;", "closing fence at EOF");
    checkDocument(app, app.parser.parse("```text\nsingle line"),
                  1, L"single line\n", L"single line", "unclosed fence at EOF");
    checkDocument(app, app.parser.parse("```text\nline\n\n```\n"),
                  2, L"line\n\n", L"line\n", "intentional trailing blank line");
    checkDocument(app, app.parser.parse("```text\n```\n"),
                  1, L"", L"", "empty fence minimum height");
    checkDocument(app, app.parser.parse("```mermaid\nnot a diagram\n```\n"),
                  1, L"not a diagram\n", L"not a diagram", "invalid Mermaid falls back to code");
}

// Line breaks spelled out, for failure messages
std::string escaped(const std::wstring& text) {
    std::string out;
    for (wchar_t c : text) {
        if (c == L'\n') out += "\\n";
        else if (c == L'\r') out += "\\r";
        else if (c < 128) out += static_cast<char>(c);
        else out += '?';
    }
    return out;
}

std::wstring clipboardText(HWND window) {
    std::wstring result;
    // Clipboard-history tools may briefly hold a newly copied value
    bool opened = false;
    for (int i = 0; i < 100 && !(opened = OpenClipboard(window)); ++i) Sleep(5);
    if (!opened) { check(false, "test clipboard opens for inspection"); return {}; }
    if (HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
        if (auto text = static_cast<const wchar_t*>(GlobalLock(data))) {
            result = text;
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
    return result;
}

// #269: clicking each block's Copy button in the mixed fixture puts the
// block's lines on the clipboard without a break after the last one
void testCopyButton() {
    auto state = std::make_unique<App>();
    App& app = *state;
    app.hwnd = CreateWindowExW(0, L"STATIC", L"Tinta code copy test", WS_POPUP,
        100, 100, 1050, 900, nullptr, nullptr, nullptr, nullptr);
    if (!app.hwnd || !initD2D(app) || !createRenderTarget(app)) {
        check(false, "code copy test initializes");
        if (app.hwnd) DestroyWindow(app.hwnd);
        return;
    }
    std::ifstream file(TINTA_COPY_FIXTURE, std::ios::binary);
    check(file.good(), "copy fixture is available");
    app.currentFile = TINTA_COPY_FIXTURE;  // a document, not the start page
    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const wchar_t* expected[] = {
        L"single line",
        L"int main() {\n    return 0;\n}",
        L"line\n",
        L"print(\"tilde\")",
        L"echo \"in a list\"",
        L"{\"quoted\": true}",
        L"```js\nlet x = 1;   \n```",
    };
    for (int theme : {0, 5}) {
        applyTheme(app, theme);
        for (float scale : {1.0f, 1.5f}) {
            app.contentScale = scale;
            updateTextFormats(app);
            for (int width : {1050, 650}) {
                app.width = width;
                app.height = 900;
                app.root = app.parser.parse(source).root;
                layoutDocument(app);
                ensureLayoutComplete(app);
                check(app.codeBlocks.size() == std::size(expected),
                      "the fixture has seven code blocks, not " +
                          std::to_string(app.codeBlocks.size()));
                for (size_t i = 0; i < app.codeBlocks.size() && i < std::size(expected); ++i) {
                    // Bring the block under the pointer, then press its button
                    const auto bounds = app.codeBlocks[i].bounds;
                    app.scrollX = 0;
                    app.scrollY = std::max(0.0f, bounds.top - 200.0f);
                    const float pad = 8.0f * app.contentScale * app.zoomFactor;
                    const float right = std::min(bounds.right, app.scrollX + documentViewportWidth(app));
                    const float x = right - pad - dpi(app, 36.0f) - app.scrollX + documentViewportX(app);
                    const float y = bounds.top + pad + dpi(app, 13.0f) - app.scrollY;
                    const LPARAM point = MAKELPARAM((int)x, (int)y);
                    // Remote Desktop's clipboard sync can hold the clipboard
                    // for a moment after each change; a copy that lost that
                    // race leaves the sentinel, and the click is repeated
                    std::wstring copied = L"sentinel";
                    for (int attempt = 0; attempt < 5 && copied == L"sentinel"; ++attempt) {
                        for (int i2 = 0; i2 < 100 && !copyToClipboard(app.hwnd, L"sentinel"); ++i2) Sleep(5);
                        handleMouseMove(app, app.hwnd, point);
                        check(app.hoveredCodeBlock == (int)i, "the pointer is over the block's copy button");
                        handleMouseDown(app, app.hwnd, MK_LBUTTON, point);
                        handleMouseUp(app, app.hwnd, 0, point);
                        copied = clipboardText(app.hwnd);
                    }
                    check(copied == expected[i],
                          "Copy puts the block's lines on the clipboard, no break after the last: block " +
                              std::to_string(i) + " gave \"" + escaped(copied) + "\" (theme " +
                              std::to_string(theme) + ", scale " + std::to_string(scale) +
                              ", width " + std::to_string(width) + ", chips " +
                              std::to_string(app.signalChips.size()) + ")");
                }
            }
        }
    }
    DestroyWindow(app.hwnd);
}

void testMixedFixture(App& app) {
    std::ifstream file(TINTA_CODE_FIXTURE, std::ios::binary);
    check(file.good(), "mixed fixture is available");
    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    auto doc = app.parser.parse(source);
    check(doc.success, "mixed fixture parses");
    std::map<qmd::ElementType, int> counts;
    std::function<void(const qmd::ElementPtr&)> visit = [&](const qmd::ElementPtr& e) {
        ++counts[e->type];
        for (const auto& child : e->children) visit(child);
    };
    visit(doc.root);
    for (auto type : {qmd::ElementType::Heading, qmd::ElementType::Table,
                     qmd::ElementType::ListItem, qmd::ElementType::Strong,
                     qmd::ElementType::Emphasis, qmd::ElementType::Link,
                     qmd::ElementType::BlockQuote, qmd::ElementType::MathInline,
                     qmd::ElementType::MathDisplay})
        check(counts[type] > 0, "mixed content survives parsing");
    app.root = doc.root;
    layoutDocument(app);
    const int rows[] = {1, 1, 2, 4, 1, 1, 1, 1, 1};
    check(app.codeBlocks.size() == std::size(rows), "all nine mixed code blocks render");
    float scale = app.contentScale * app.zoomFactor;
    for (size_t i = 0; i < app.codeBlocks.size() && i < std::size(rows); ++i) {
        const auto& bounds = app.codeBlocks[i].bounds;
        check(closeEnough(bounds.bottom - bounds.top, (rows[i] * 20.0f + 24.0f) * scale),
              "mixed code block height reflects intentional rows");
        if (i) check(bounds.top > app.codeBlocks[i-1].bounds.bottom, "mixed code blocks never overlap");
    }
    check(app.docText.find(L"Final heading") != std::wstring::npos,
          "content following the final code block renders");
    check(app.contentWidth > app.width, "wide code still enables horizontal scrolling");
}
}

int main() {
    {
        auto state = std::make_unique<App>();
        App& app = *state;
        if (!initD2D(app)) return 2;
        app.height = 900;
        app.readingWidthPct = 100;
        for (int theme : {0, 5}) {
            applyTheme(app, theme);
            for (float scale : {1.0f, 1.5f}) {
                app.contentScale = scale;
                updateTextFormats(app);
                for (int width : {1050, 650}) {
                    app.width = width;
                    testCases(app);
                    testMixedFixture(app);
                }
            }
        }
    }
    testCopyButton();
    CoUninitialize();
    std::cout << "Code block layout: " << failures << " failures\n";
    return failures ? 1 : 0;
}
