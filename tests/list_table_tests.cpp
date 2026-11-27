// A table indented under a list item lays out as a table, inside the item
// and in document order (tests/fixtures/list-item-table.md). It used to run
// together as text ahead of the item's paragraph.
#include "d2d_init.h"
#include "render.h"
#include "table_format.h"

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

}  // namespace

int main() {
    auto state = std::make_unique<App>();
    App& app = *state;
    app.hwnd = CreateWindowExW(0, L"STATIC", L"List table tests", WS_POPUP, 0, 0, 900, 700, nullptr, nullptr,
                               nullptr, nullptr);
    if (!app.hwnd || !initD2D(app) || !createRenderTarget(app)) return 2;
    app.width = 900;
    app.height = 700;
    app.contentScale = 1.0f;
    updateTextFormats(app);
    std::ifstream in(TINTA_LIST_TABLE_FIXTURE, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    app.currentFile = TINTA_LIST_TABLE_FIXTURE;
    app.root = app.parser.parse(text.str()).root;
    layoutDocument(app);

    check(app.tableRects.size() == 4, "four tables lay out, got " + std::to_string(app.tableRects.size()));
    if (app.tableRects.size() == 4) {
        const auto& item = app.tableRects[0];
        const auto& top = app.tableRects[3];
        check(item.csv.rfind(L"\"Blend\",\"price_per_kg\",\"Survey B\"\r\n", 0) == 0,
              "the first item's table keeps its cells (CSV, every field quoted)");
        check(item.bounds.left > top.bounds.left + 10.0f, "an item's table is indented with the item");
        check(app.tableRects[1].bounds.top > item.bounds.bottom && app.tableRects[2].bounds.top > app.tableRects[1].bounds.bottom,
              "tables follow document order");
    }
    const std::wstring& doc = app.docText;
    size_t lead = doc.find(L"tea, 2025Q3"), table = doc.find(L"Assam 2"), after = doc.find(L"Only the first survey");
    check(lead != std::wstring::npos && table != std::wstring::npos && after != std::wstring::npos && lead < table &&
              table < after,
          "the item's paragraph, its table, then its next paragraph");
    check(doc.find(L"Blendprice_per_kg") == std::wstring::npos, "no table runs together as text");

    // Copy as Markdown: the item's table as written, without the item indent
    if (app.tableRects.size() == 4) {
        std::string md = tableMarkdownAt(text.str(), app.tableRects[0].sourceOffset);
        check(md.rfind("| Blend | price_per_kg | Survey B |\n| --- | --- | --- |\n| Assam 2 |", 0) == 0 &&
                  md.find("  |") == std::string::npos,
              "the Markdown copy is the table as written, de-indented: " + md.substr(0, 60));
    }
    // CSV: every field quoted, quotes doubled, CRLF rows
    check(csvQuoteAll({{L"a", L"say \"hi\""}, {L"1,5", L""}}) == L"\"a\",\"say \"\"hi\"\"\"\r\n\"1,5\",\"\"\r\n",
          "CSV quotes every field");

    // Aligning: a compressed table padded into columns, alignment kept
    const std::wstring compressed = L"|a|bb|ccc|\n|:-|-:|:-:|\n|longer|1|x|\n|y|22|zz|";
    const std::wstring aligned = L"| a      |  bb | ccc |\n"
                                 L"| :----- | --: | :-: |\n"
                                 L"| longer |   1 |  x  |\n"
                                 L"| y      |  22 | zz  |";
    std::wstring got = alignMarkdownTable(compressed);
    check(got == aligned, "a compressed table aligns");
    check(alignMarkdownTable(aligned) == aligned, "aligning is idempotent");
    check(alignMarkdownTable(L"  |x|y|\n  |---|---|\n  |中文|z|") ==
              L"  | x    | y   |\n  | ---- | --- |\n  | 中文 | z   |",
          "the indent stays, and CJK counts double width");
    check(alignMarkdownTable(L"not | a table") == L"not | a table", "text that is no table is left alone");
    // The editor's scan finds tables and skips fenced code
    std::wstring docText = L"intro\n\n|a|b|\n|-|-|\n|1|2|\n\n```\n|x|y|\n|-|-|\n```\n\n|c|\n|-|\n";
    check(tableSpans(docText).size() == 2, "two tables found, the fenced one skipped");

    DestroyWindow(app.hwnd);
    std::cout << "list tables: " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
