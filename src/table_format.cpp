#include "table_format.h"

#include <algorithm>

namespace {

template <typename S>
bool hasPipe(const S& s, size_t from, size_t to) {
    for (size_t i = from; i < to; i++) {
        if (s[i] == '\\') {
            i++;
            continue;
        }
        if (s[i] == '|') return true;
    }
    return false;
}

// A delimiter row: cells of dashes with optional colons, pipe-separated
template <typename S>
bool isDelimiter(const S& s, size_t from, size_t to) {
    bool dash = false, pipe = false;
    for (size_t i = from; i < to; i++) {
        auto c = s[i];
        if (c == '-') dash = true;
        else if (c == '|') pipe = true;
        else if (c != ':' && c != ' ' && c != '\t') return false;
    }
    return dash && pipe;
}

template <typename S>
size_t lineStart(const S& s, size_t at) {
    while (at > 0 && s[at - 1] != '\n') at--;
    return at;
}

template <typename S>
size_t lineEnd(const S& s, size_t at) {
    while (at < s.size() && s[at] != '\n') at++;
    return at;
}

template <typename S>
bool spanAt(const S& s, size_t at, size_t& start, size_t& end) {
    if (at > s.size()) return false;
    size_t ls = lineStart(s, std::min(at, s.size()));
    size_t le = lineEnd(s, ls);
    if (!hasPipe(s, ls, le)) return false;
    start = ls;
    while (start > 0) {
        size_t prev = lineStart(s, start - 1);
        if (!hasPipe(s, prev, start - 1)) break;
        start = prev;
    }
    end = le;
    while (end < s.size()) {
        size_t next = lineEnd(s, end + 1);
        if (!hasPipe(s, end + 1, next)) break;
        end = next;
    }
    // The delimiter row is the second line
    size_t second = lineEnd(s, start);
    if (second >= end) return false;
    return isDelimiter(s, second + 1, lineEnd(s, second + 1));
}

// Display columns of a character: 2 for East Asian wide and astral
// (emoji) characters, 0 for combining marks
int charWidth(unsigned c) {
    if ((c >= 0x0300 && c <= 0x036F) || c == 0x200B || c == 0x200D || (c >= 0xFE00 && c <= 0xFE0F)) return 0;
    if ((c >= 0x1100 && c <= 0x115F) || (c >= 0x2E80 && c <= 0xA4CF) || (c >= 0xAC00 && c <= 0xD7A3) ||
        (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFE30 && c <= 0xFE4F) || (c >= 0xFF00 && c <= 0xFF60) ||
        (c >= 0xFFE0 && c <= 0xFFE6) || c >= 0x1F000) {
        return 2;
    }
    return 1;
}

int displayWidth(const std::wstring& s) {
    int w = 0;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned c = (unsigned)s[i];
        if (sizeof(wchar_t) == 2 && c >= 0xD800 && c <= 0xDBFF && i + 1 < s.size()) {
            c = 0x10000 + ((c - 0xD800) << 10) + ((unsigned)s[i + 1] - 0xDC00);
            i++;
        }
        w += charWidth(c);
    }
    return w;
}

std::wstring trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r");
    if (a == std::wstring::npos) return L"";
    size_t b = s.find_last_not_of(L" \t\r");
    return s.substr(a, b - a + 1);
}

// A row's cells: outer pipes dropped, split on unescaped pipes, trimmed
std::vector<std::wstring> cellsOf(const std::wstring& line) {
    std::wstring row = trim(line);
    if (!row.empty() && row.front() == L'|') row.erase(0, 1);
    if (!row.empty() && row.back() == L'|' && !(row.size() >= 2 && row[row.size() - 2] == L'\\')) row.pop_back();
    std::vector<std::wstring> cells;
    std::wstring cell;
    for (size_t i = 0; i < row.size(); i++) {
        if (row[i] == L'\\' && i + 1 < row.size()) {
            cell += row[i];
            cell += row[++i];
        } else if (row[i] == L'|') {
            cells.push_back(trim(cell));
            cell.clear();
        } else {
            cell += row[i];
        }
    }
    cells.push_back(trim(cell));
    return cells;
}

}  // namespace

bool tableSpanAt(const std::string& src, size_t at, size_t& start, size_t& end) {
    return spanAt(src, at, start, end);
}

bool tableSpanAt(const std::wstring& src, size_t at, size_t& start, size_t& end) {
    return spanAt(src, at, start, end);
}

std::vector<std::pair<size_t, size_t>> tableSpans(const std::wstring& src) {
    std::vector<std::pair<size_t, size_t>> spans;
    bool fenced = false;
    size_t pos = 0;
    while (pos < src.size()) {
        size_t le = lineEnd(src, pos);
        std::wstring line = trim(src.substr(pos, le - pos));
        if (line.rfind(L"```", 0) == 0 || line.rfind(L"~~~", 0) == 0) fenced = !fenced;
        size_t start = 0, end = 0;
        if (!fenced && hasPipe(src, pos, le) && spanAt(src, pos, start, end) && start == pos) {
            spans.push_back({start, end});
            pos = end + 1;
            continue;
        }
        pos = le + 1;
    }
    return spans;
}

std::string tableMarkdownAt(const std::string& src, size_t at) {
    size_t start = 0, end = 0;
    if (!tableSpanAt(src, at, start, end)) return "";
    // The indent every line shares (a table inside a list item)
    std::vector<std::string> lines;
    size_t indent = std::string::npos;
    for (size_t pos = start; pos <= end;) {
        size_t le = lineEnd(src, pos);
        std::string line = src.substr(pos, le - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t lead = line.find_first_not_of(" \t");
        if (lead != std::string::npos) indent = std::min(indent, lead);
        lines.push_back(line);
        pos = le + 1;
    }
    std::string out;
    for (const auto& line : lines) {
        out += line.substr(std::min(indent == std::string::npos ? 0 : indent, line.size()));
        out += '\n';
    }
    return out;
}

std::wstring alignMarkdownTable(const std::wstring& table) {
    std::vector<std::wstring> lines;
    for (size_t pos = 0; pos <= table.size();) {
        size_t le = lineEnd(table, pos);
        lines.push_back(table.substr(pos, le - pos));
        pos = le + 1;
    }
    while (!lines.empty() && trim(lines.back()).empty()) lines.pop_back();
    if (lines.size() < 2 || !isDelimiter(lines[1], 0, lines[1].size())) return table;
    std::wstring indent = lines[0].substr(0, lines[0].find_first_not_of(L" \t"));

    std::vector<std::vector<std::wstring>> rows;
    for (const auto& line : lines) rows.push_back(cellsOf(line));
    size_t cols = 0;
    for (const auto& row : rows) cols = std::max(cols, row.size());
    // Alignment from the delimiter row: 'l', 'r', 'c', or 0 (none)
    std::vector<char> align(cols, 0);
    for (size_t c = 0; c < rows[1].size(); c++) {
        const std::wstring& d = rows[1][c];
        bool left = !d.empty() && d.front() == L':', right = !d.empty() && d.back() == L':';
        align[c] = left && right ? 'c' : right ? 'r' : left ? 'l' : 0;
    }
    std::vector<int> width(cols, 3);
    for (size_t r = 0; r < rows.size(); r++) {
        if (r == 1) continue;
        for (size_t c = 0; c < rows[r].size(); c++) width[c] = std::max(width[c], displayWidth(rows[r][c]));
    }

    std::wstring out;
    for (size_t r = 0; r < rows.size(); r++) {
        out += indent;
        out += L"|";
        for (size_t c = 0; c < cols; c++) {
            out += L" ";
            if (r == 1) {
                std::wstring d(width[c], L'-');
                if (align[c] == 'l' || align[c] == 'c') d[0] = L':';
                if (align[c] == 'r' || align[c] == 'c') d[width[c] - 1] = L':';
                out += d;
            } else {
                std::wstring cell = c < rows[r].size() ? rows[r][c] : L"";
                int pad = width[c] - displayWidth(cell);
                int before = align[c] == 'r' ? pad : align[c] == 'c' ? pad / 2 : 0;
                out += std::wstring(before, L' ') + cell + std::wstring(pad - before, L' ');
            }
            out += L" |";
        }
        if (r + 1 < rows.size()) out += L"\n";
    }
    return out;
}

std::wstring csvQuoteAll(const std::vector<std::vector<std::wstring>>& rows) {
    std::wstring out;
    for (const auto& row : rows) {
        for (size_t c = 0; c < row.size(); c++) {
            if (c) out += L",";
            out += L"\"";
            for (wchar_t ch : row[c]) {
                if (ch == L'"') out += L"\"\"";
                else out += ch;
            }
            out += L"\"";
        }
        out += L"\r\n";
    }
    return out;
}
