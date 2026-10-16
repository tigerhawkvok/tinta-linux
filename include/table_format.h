#ifndef TINTA_TABLE_FORMAT_H
#define TINTA_TABLE_FORMAT_H

// Pipe tables in Markdown source: copying one as written or as CSV, and
// aligning their columns in the editor.

#include <string>
#include <vector>

// The full lines [start, end) of the pipe table around offset `at`: the
// pipe-bearing lines around it, with a delimiter row second. False when
// `at` is not in a table.
bool tableSpanAt(const std::string& src, size_t at, size_t& start, size_t& end);
bool tableSpanAt(const std::wstring& src, size_t at, size_t& start, size_t& end);

// Every table's span in `src`, in order, skipping fenced code blocks
std::vector<std::pair<size_t, size_t>> tableSpans(const std::wstring& src);

// The table around `at` as written, without the indent its lines share
// (a table in a list item), lines ended by '\n'. Empty when none.
std::string tableMarkdownAt(const std::string& src, size_t at);

// A pipe table with every column padded to one width: cells honour the
// delimiter row's alignment, the delimiter row is redrawn to match, and
// East Asian wide characters count double. Lines keep the first line's
// indent. Text that is no table comes back unchanged.
std::wstring alignMarkdownTable(const std::wstring& table);

// Rows as CSV with every field quoted ('"' doubled), rows ended by CRLF
std::wstring csvQuoteAll(const std::vector<std::vector<std::wstring>>& rows);

#endif  // TINTA_TABLE_FORMAT_H
