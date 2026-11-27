// Copying from the reader returns Markdown (markdown_copy.cpp): selections
// made on the rendered text of tests/fixtures/copy-as-markdown.md
#include "d2d_init.h"
#include "document.h"
#include "input.h"
#include "markdown_copy.h"
#include "render.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace {

int failures = 0;

void check(bool ok, const std::string& message) {
    if (!ok) {
        std::cerr << "FAIL: " << message << "\n";
        failures++;
    }
}

std::string read(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

std::string narrow(const std::wstring& text) {
    std::string out;
    for (wchar_t c : text) out += c < 128 ? (char)c : '?';
    return out;
}

// The Markdown copied for rendered text `from` .. `to` (inclusive), each
// the nth occurrence in docText
std::wstring copy(App& app, const std::wstring& from, const std::wstring& to = L"", int nth = 0) {
    size_t start = std::wstring::npos;
    for (int i = 0, at = 0; i <= nth; i++, at = (int)start + 1) start = app.docText.find(from, at);
    if (start == std::wstring::npos) return L"<" + from + L" not rendered>";
    const std::wstring& last = to.empty() ? from : to;
    size_t end = app.docText.find(last, to.empty() ? start : start + from.size());
    if (end == std::wstring::npos) return L"<" + last + L" not rendered>";
    return copyTextForRange(app, start, end + last.size());
}

void expect(App& app, const std::wstring& got, const std::wstring& want, const std::string& what) {
    (void)app;
    check(got == want, what + ": got [" + narrow(got) + "] want [" + narrow(want) + "]");
}

std::wstring clipboard(HWND hwnd) {
    std::wstring text;
    if (OpenClipboard(hwnd)) {
        if (HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
            if (auto* chars = static_cast<const wchar_t*>(GlobalLock(data))) {
                text = chars;
                GlobalUnlock(data);
            }
        }
        CloseClipboard();
    }
    return text;
}

}  // namespace

int main() {
    auto state = std::make_unique<App>();
    App& app = *state;
    app.hwnd = CreateWindowExW(0, L"STATIC", L"Copy tests", WS_POPUP, 0, 0, 1000, 900, nullptr, nullptr,
                               nullptr, nullptr);
    if (!app.hwnd || !initD2D(app) || !createRenderTarget(app)) return 2;
    app.width = 1000;
    app.height = 900;
    updateTextFormats(app);
    std::string source = read(TINTA_COPY_FIXTURE);
    app.root = parseDocument(app.parser, source, std::string_view(TINTA_COPY_FIXTURE)).root;
    layoutDocument(app);
    if (std::getenv("TINTA_COPY_DUMP")) {
        for (const auto& span : app.docSourceSpans) {
            std::cerr << "span block " << span.block << " table " << span.table << " code " << span.code
                      << " hint " << span.sourceHint << " inlines " << span.inlines.size() << " ["
                      << narrow(app.docText.substr(span.docStart, span.docLength)) << "]\n";
        }
    }

    // Inside one block: the formatting comes along
    expect(app, copy(app, L"bold words"), L"**bold words**", "a bold phrase");
    expect(app, copy(app, L"old wor"), L"**old wor**", "part of a bold phrase");
    expect(app, copy(app, L"inline code"), L"`inline code`", "a code span");
    expect(app, copy(app, L"link"), L"[link](https://example.com/page)", "a link");
    expect(app, copy(app, L"struck"), L"~~struck~~", "strikethrough");
    expect(app, copy(app, L"words, ", L"ital", 1), L"**bold words**, *italic*",
           "a selection cutting into two spans takes both whole");
    expect(app, copy(app, L"Plain words"), L"Plain words", "plain text stays plain");

    // Whole blocks and across blocks: whole source lines
    expect(app, copy(app, L"Copy as Markdown"), L"# Copy as Markdown", "a heading, whole");
    expect(app, copy(app, L"first item", L"nested item"),
           L"- first item\n- second item with **heavy** text\n  - nested item", "list items with their markers");
    expect(app, copy(app, L"A quote with ", L"emphasis."), L"> A quote with *emphasis*.", "a quote, whole");
    expect(app, copy(app, L"beta", L"2"), L"| Name | Value |\n|------|-------|\n| beta | 2 |",
           "a table row brings the header");

    // Code: exactly the code, or the fenced block
    expect(app, copy(app, L"return"), L"return", "part of a code block");
    expect(app, copy(app, L"def hello", L"\"world\""), L"```python\ndef hello():\n    return \"world\"\n```",
           "a whole code block, fenced");
    std::wstring across = copy(app, L"\"world\"", L"Last paragraph");
    check(across.find(L"```mermaid\nflowchart LR\n  A --> B\n```") != std::wstring::npos &&
              across.find(L"Last paragraph.") != std::wstring::npos,
          "a selection across a diagram carries its fence: [" + narrow(across) + "]");

    // Everything: the document as written
    check(documentMarkdown(app) == std::wstring(source.begin(), source.end()), "copy all is the file");

    // Ctrl+C in the reader puts the Markdown on the clipboard
    size_t at = app.docText.find(L"bold words");
    app.selAnchor = at;
    app.selFocus = at + 10;
    app.hasSelection = true;
    BYTE keys[256], ctrl[256];
    GetKeyboardState(keys);
    std::memcpy(ctrl, keys, sizeof ctrl);
    ctrl[VK_CONTROL] = 0x80;
    SetKeyboardState(ctrl);
    handleKeyDown(app, app.hwnd, 'C');
    SetKeyboardState(keys);
    expect(app, clipboard(app.hwnd), L"**bold words**", "Ctrl+C copies the Markdown");

    std::cout << "copy markdown: " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
