#ifndef TINTA_MARKDOWN_COPY_H
#define TINTA_MARKDOWN_COPY_H

#include <cstddef>
#include <string>

struct App;

// Copying from the reader returns Markdown: the code that renders what was
// selected, never HTML or plain rendered text.
//  - Within one block, the selected source, with the delimiters of any
//    emphasis, code, link... it cuts into ("**bold**" for "old" in bold).
//  - Across blocks, or a whole block, whole source lines (list markers,
//    "## ", "> " included), widened to complete code fences, and to a
//    table's header when rows of it are taken.
// Empty when the selection cannot be traced to the source; callers then
// copy the rendered text.
std::wstring selectionMarkdown(const App& app, size_t start, size_t end);

// The whole document as written; empty without a source
std::wstring documentMarkdown(const App& app);

// What a copy of docText [start, end) puts on the clipboard: its Markdown,
// else its rendered text
std::wstring copyTextForRange(const App& app, size_t start, size_t end);

#endif
