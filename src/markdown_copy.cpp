#include "markdown_copy.h"

#include "app.h"
#include "editor.h"
#include "selection.h"
#include "utils.h"

#include <algorithm>
#include <vector>

// Layout recorded where each stretch of docText came from (docSourceSpans):
// its block's first source offset, and the inline spans around it. Each
// span's text is found in the source by scanning forward from its block's
// start: rendered text is a literal slice of the source in all but rare
// cases (entities, emoji shortcodes), and the scan steps over the markup
// between spans. Anything that cannot be traced makes the copy fall back to
// rendered text rather than guess.

namespace {

using Span = App::DocSourceSpan;
constexpr size_t kNone = std::string::npos;
// How far past the previous span a span's text may begin (link targets,
// footnote markers and the like sit between)
constexpr size_t kScanWindow = 2048;

const std::string* mappedSource(const App& app) {
    return app.root && app.root->source ? &app.root->source->mapped : nullptr;
}

size_t lineStart(const std::string& src, size_t pos) {
    size_t at = src.rfind('\n', pos == 0 ? 0 : pos - 1);
    return (at == kNone || pos == 0) ? 0 : at + 1;
}

size_t lineEnd(const std::string& src, size_t pos) {
    size_t at = src.find('\n', pos);
    return at == kNone ? src.size() : at;
}

bool isFence(const std::string& src, size_t start) {
    size_t i = start;
    while (i < src.size() && (src[i] == ' ' || src[i] == '\t' || src[i] == '>')) i++;
    return src.compare(i, 3, "```") == 0 || src.compare(i, 3, "~~~") == 0;
}

class SourceMap {
public:
    SourceMap(const App& app, const std::string& src) : app_(app), src_(src) {}

    // Where span i's text starts in the source, or kNone
    size_t locate(size_t i) {
        if (i >= located_.size()) located_.resize(app_.docSourceSpans.size(), kUnknown);
        if (located_[i] != kUnknown) return located_[i];
        const auto& spans = app_.docSourceSpans;
        size_t first = i;
        while (first > 0 && spans[first - 1].block == spans[i].block) first--;
        size_t cursor = spans[first].sourceHint;
        for (size_t j = first; j <= i; j++) {
            size_t pos = kNone;
            if (cursor != kNone && cursor <= src_.size()) {
                std::string text = utf8(j);
                pos = src_.find(text, spans[j].code ? lineStart(src_, cursor) : cursor);
                if (pos != kNone && pos > cursor + kScanWindow) pos = kNone;
                if (pos == kNone && spans[j].code) pos = locateCodeByLines(j, cursor);
                if (pos != kNone) cursor = pos + text.size();
            }
            located_[j] = pos;
        }
        return located_[i];
    }

    // The source offset of docText position pos inside span i
    size_t at(size_t i, size_t pos) {
        size_t base = locate(i);
        if (base == kNone) return kNone;
        const Span& span = app_.docSourceSpans[i];
        size_t inSpan = std::min(pos, span.docStart + span.docLength) - span.docStart;
        return base + toUtf8(app_.docText.substr(span.docStart, inSpan)).size();
    }

    size_t endOf(size_t i) {
        size_t base = locate(i);
        return base == kNone ? kNone : base + utf8(i).size();
    }

    std::string utf8(size_t i) const {
        const Span& span = app_.docSourceSpans[i];
        return toUtf8(app_.docText.substr(span.docStart, span.docLength));
    }

private:
    // Code in a list or quote is indented or prefixed in the source: match
    // its first line instead and trust the line structure
    size_t locateCodeByLines(size_t i, size_t cursor) const {
        std::string text = utf8(i);
        std::string firstLine = text.substr(0, text.find('\n'));
        if (firstLine.empty()) return lineStart(src_, cursor);
        size_t pos = src_.find(firstLine, lineStart(src_, cursor));
        return pos != kNone && pos <= cursor + kScanWindow ? pos : kNone;
    }

    static constexpr size_t kUnknown = kNone - 1;
    const App& app_;
    const std::string& src_;
    std::vector<size_t> located_;
};

// The span holding docText position pos; between spans, the next one (or
// the previous one when looking for an end)
size_t spanAt(const App& app, size_t pos, bool end) {
    const auto& spans = app.docSourceSpans;
    if (spans.empty()) return kNone;
    if (end) {
        // The last span starting before pos: an end belongs to the text it
        // closes, not to a span that starts right there
        auto after = std::lower_bound(spans.begin(), spans.end(), pos,
                                      [](const Span& s, size_t p) { return s.docStart < p; });
        return after == spans.begin() ? kNone : (size_t)(after - 1 - spans.begin());
    }
    auto after = std::upper_bound(spans.begin(), spans.end(), pos,
                                  [](size_t p, const Span& s) { return p < s.docStart; });
    if (after != spans.begin()) {
        const Span& before = *(after - 1);
        if (pos < before.docStart + before.docLength) return (size_t)(after - 1 - spans.begin());
    }
    return after == spans.end() ? kNone : (size_t)(after - spans.begin());
}

bool hasInline(const Span& span, uintptr_t id) {
    for (const auto& in : span.inlines) {
        if (in.second == id) return true;
    }
    return false;
}

bool isOpenDelimiter(char c) {
    return c == '*' || c == '_' || c == '~' || c == '=' || c == '`' || c == '^' || c == '[' || c == '$';
}

bool isCloseDelimiter(char c) {
    return c == '*' || c == '_' || c == '~' || c == '=' || c == '`' || c == '^' || c == '$';
}

// The opening markup right before a span's text ("**", "[", "`", "~~")
size_t openingRun(const std::string& src, size_t textStart) {
    size_t start = textStart, floor = lineStart(src, textStart);
    while (start > floor && isOpenDelimiter(src[start - 1])) start--;
    return start;
}

// The closing markup right after it ("**", "](url)", "][ref]")
size_t closingRun(const std::string& src, size_t textEnd) {
    size_t end = textEnd, ceiling = lineEnd(src, textEnd);
    while (end < ceiling && isCloseDelimiter(src[end])) end++;
    if (end < ceiling && src[end] == ']') {
        end++;
        if (end < ceiling && (src[end] == '(' || src[end] == '[')) {
            char close = src[end] == '(' ? ')' : ']';
            size_t at = src.find(close, end);
            end = at == kNone || at > ceiling ? ceiling : at + 1;
        }
    }
    return end;
}

}  // namespace

std::wstring selectionMarkdown(const App& app, size_t start, size_t end) {
    const std::string* source = mappedSource(app);
    if (!source || start >= end) return std::wstring();
    const std::string& src = *source;
    const auto& spans = app.docSourceSpans;
    size_t a = spanAt(app, start, false), b = spanAt(app, end, true);
    if (a == kNone || b == kNone || a > b) return std::wstring();
    SourceMap map(app, src);
    size_t s0 = map.at(a, std::max(start, spans[a].docStart));
    size_t s1 = map.at(b, std::min(end, spans[b].docStart + spans[b].docLength));
    if (s0 == kNone || s1 == kNone || s1 < s0) return std::wstring();

    const Span& first = spans[a];
    const Span& last = spans[b];
    // The first and last spans of the selection's blocks
    size_t blockFirst = a, blockLast = b;
    while (blockFirst > 0 && spans[blockFirst - 1].block == first.block) blockFirst--;
    while (blockLast + 1 < spans.size() && spans[blockLast + 1].block == last.block) blockLast++;
    bool oneBlock = first.block == last.block;
    // Whole: from its first character to its last, trailing line breaks
    // aside (a code block's text ends in one the reader never selects)
    size_t blockEnd = map.endOf(blockLast);
    size_t blockStart = map.locate(blockFirst);
    while (blockEnd != kNone && blockEnd > blockStart && (src[blockEnd - 1] == '\n' || src[blockEnd - 1] == ' ')) {
        blockEnd--;
    }
    bool wholeBlock = oneBlock && blockStart != kNone && blockEnd != kNone && s0 <= blockStart && s1 >= blockEnd;

    // Part of a code block: exactly that code, no fences
    if (oneBlock && !wholeBlock && first.code) return toWide(src.substr(s0, s1 - s0));

    if (oneBlock && !wholeBlock) {
        // Inside one block: widen to the inline spans the selection cuts
        // into, and wrap it in those it sits wholly inside
        std::string prefix, suffix;
        for (const auto& in : first.inlines) {
            if (hasInline(last, in.second)) continue;
            size_t j = blockFirst;
            while (j < a && !hasInline(spans[j], in.second)) j++;
            size_t textStart = map.locate(j);
            if (textStart != kNone) s0 = std::min(s0, openingRun(src, textStart));
        }
        for (const auto& in : last.inlines) {
            if (hasInline(first, in.second)) continue;
            size_t j = blockLast;
            while (j > b && !hasInline(spans[j], in.second)) j--;
            size_t textEnd = map.endOf(j);
            if (textEnd != kNone) s1 = std::max(s1, closingRun(src, textEnd));
        }
        // Wholly inside: the selection gets that span's delimiters. Nested
        // spans that open (or close) at the same text share one run of them
        size_t wrappedOpen = kNone, wrappedClose = kNone;
        for (const auto& in : first.inlines) {
            if (!hasInline(last, in.second)) continue;
            size_t j = blockFirst, k = blockLast;
            while (j < a && !hasInline(spans[j], in.second)) j++;
            while (k > b && !hasInline(spans[k], in.second)) k--;
            size_t textStart = map.locate(j), textEnd = map.endOf(k);
            if (textStart == kNone || textEnd == kNone) continue;
            if (textStart != wrappedOpen) {
                size_t open = openingRun(src, textStart);
                prefix = src.substr(open, textStart - open) + prefix;
                wrappedOpen = textStart;
            }
            if (textEnd != wrappedClose) {
                size_t close = closingRun(src, textEnd);
                suffix += src.substr(textEnd, close - textEnd);
                wrappedClose = textEnd;
            }
        }
        return toWide(prefix + src.substr(s0, s1 - s0) + suffix);
    }

    // Across blocks, or a block whole: whole source lines
    s0 = lineStart(src, s0);
    s1 = lineEnd(src, s1 > s0 ? s1 - 1 : s1);
    // Code: its fences come along
    if (first.code) {
        size_t codeStart = map.locate(a);
        if (codeStart != kNone && lineStart(src, codeStart) > 0) {
            size_t fence = lineStart(src, lineStart(src, codeStart) - 1);
            if (isFence(src, fence)) s0 = std::min(s0, fence);
        }
    }
    if (last.code) {
        size_t codeEnd = map.endOf(b);
        if (codeEnd != kNone) {
            size_t next = lineEnd(src, codeEnd > 0 ? codeEnd - 1 : codeEnd);
            if (next < src.size()) {
                size_t fence = next + 1;
                if (isFence(src, fence)) s1 = std::max(s1, lineEnd(src, fence));
            }
        }
    }
    // Rows of a table: its header and separator lines come along
    if (first.table) {
        size_t j = a;
        while (j > 0 && spans[j - 1].table == first.table) j--;
        size_t tableStart = map.locate(j);
        if (tableStart != kNone) {
            tableStart = lineStart(src, tableStart);
            if (tableStart < s0) {
                size_t separator = lineEnd(src, tableStart);
                separator = separator < src.size() ? lineEnd(src, separator + 1) : separator;
                std::string head = src.substr(tableStart, separator - tableStart);
                return toWide(head + "\n" + src.substr(s0, s1 - s0));
            }
        }
    }
    return toWide(src.substr(s0, s1 - s0));
}

std::wstring documentMarkdown(const App& app) {
    if (!app.root || !app.root->source) return std::wstring();
    return toWide(app.root->source->original);
}

std::wstring copyTextForRange(const App& app, size_t start, size_t end) {
    std::wstring markdown = selectionMarkdown(app, start, end);
    return markdown.empty() ? selectionTextForRange(app, start, end) : markdown;
}
