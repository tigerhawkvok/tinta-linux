// Admonitions and callouts: the box several Markdown dialects draw around a
// note, a tip or a warning. One model and one look for all of them:
//   GitHub / GitLab alerts      > [!NOTE]
//   Obsidian callouts           > [!tip]- Title, nested with > > [!note]
//   Docusaurus, VitePress, MyST :::note[Title], ::: tip Title, :::{note},
//                               ```{note}; nested with more colons
//   MkDocs (Material)           !!! note "Title", ??? / ???+ fold, indented
//   reStructuredText (Sphinx)   .. note::, .. admonition:: Title, indented
//   Hugo shortcodes             {{< alert "warning" >}} ... {{< /alert >}}
//   HTML                        <div class="admonition note"> ... </div>
// The parser (markdown.cpp) turns each into a blockquote whose alertKind is
// one of the kinds below; the renderer and the exporters style it from
// admonitionStyle.
#ifndef TINTA_ADMONITION_H
#define TINTA_ADMONITION_H

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

namespace qmd {

// GitHub's five keep the numbers alerts always had
enum AdmonitionKind : int {
    AdmonitionNone = 0,
    AdmonitionNote = 1,
    AdmonitionTip = 2,
    AdmonitionImportant = 3,
    AdmonitionWarning = 4,
    AdmonitionCaution = 5,
    AdmonitionAbstract = 6,
    AdmonitionInfo = 7,
    AdmonitionTodo = 8,
    AdmonitionSuccess = 9,
    AdmonitionQuestion = 10,
    AdmonitionFailure = 11,
    AdmonitionDanger = 12,
    AdmonitionBug = 13,
    AdmonitionExample = 14,
    AdmonitionQuote = 15,
    AdmonitionDetails = 16,
    AdmonitionKindCount = 17
};

struct AdmonitionStyle {
    const char* cls;        // HTML class
    const wchar_t* icon;    // text presentation, drawn in the accent colour
    const char* iconUtf8;
    uint32_t light, dark;   // accent on light and dark themes
};

// Indexed by AdmonitionKind
inline const AdmonitionStyle& admonitionStyle(int kind) {
    static const AdmonitionStyle kStyles[AdmonitionKindCount] = {
        {"note", L"", "", 0x6B7280, 0x9CA3AF},
        {"note", L"ⓘ", "\xE2\x93\x98", 0x0969DA, 0x4493F8},                          // circled i
        {"tip", L"\U0001F4A1︎", "\xF0\x9F\x92\xA1\xEF\xB8\x8E", 0x1A7F37, 0x3FB950},  // bulb
        {"important", L"❗︎", "\xE2\x9D\x97\xEF\xB8\x8E", 0x8250DF, 0xAB7DF8},     // !
        {"warning", L"⚠︎", "\xE2\x9A\xA0\xEF\xB8\x8E", 0x9A6700, 0xD29922},       // triangle
        {"caution", L"⛔︎", "\xE2\x9B\x94\xEF\xB8\x8E", 0xCF222E, 0xF85149},       // no entry
        {"abstract", L"☰", "\xE2\x98\xB0", 0x0D9488, 0x2DD4BF},                        // trigram
        {"info", L"ℹ︎", "\xE2\x84\xB9\xEF\xB8\x8E", 0x0284C7, 0x38BDF8},          // i
        {"todo", L"☑︎", "\xE2\x98\x91\xEF\xB8\x8E", 0x2563EB, 0x60A5FA},          // ballot box
        {"success", L"✔︎", "\xE2\x9C\x94\xEF\xB8\x8E", 0x047857, 0x34D399},       // check
        {"question", L"?", "?", 0x4D7C0F, 0xA3E635},
        {"failure", L"✖︎", "\xE2\x9C\x96\xEF\xB8\x8E", 0xDC2626, 0xF87171},       // cross
        {"danger", L"⚡︎", "\xE2\x9A\xA1\xEF\xB8\x8E", 0xBE123C, 0xFB7185},        // zap
        {"bug", L"✱", "\xE2\x9C\xB1", 0xDB2777, 0xF472B6},                              // heavy asterisk
        {"example", L"≡", "\xE2\x89\xA1", 0x7C3AED, 0xA78BFA},                          // list
        {"quote", L"❝", "\xE2\x9D\x9D", 0x6B7280, 0x9CA3AF},                            // quote mark
        {"details", L"", "", 0x6B7280, 0x9CA3AF},
    };
    return kStyles[kind > 0 && kind < AdmonitionKindCount ? kind : 0];
}

inline std::string admonitionLower(std::string_view text) {
    std::string out(text);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// The kind a type keyword names, aliases included (case-insensitive);
// AdmonitionNone for a keyword none of the dialects defines
inline int admonitionKindOf(std::string_view keyword) {
    static const struct {
        const char* name;
        int kind;
    } kNames[] = {
        {"note", AdmonitionNote},         {"seealso", AdmonitionNote},     {"admonition", AdmonitionNote},
        {"abstract", AdmonitionAbstract}, {"summary", AdmonitionAbstract}, {"tldr", AdmonitionAbstract},
        {"info", AdmonitionInfo},         {"todo", AdmonitionTodo},        {"tip", AdmonitionTip},
        {"hint", AdmonitionTip},          {"important", AdmonitionImportant},
        {"success", AdmonitionSuccess},   {"check", AdmonitionSuccess},    {"done", AdmonitionSuccess},
        {"question", AdmonitionQuestion}, {"help", AdmonitionQuestion},    {"faq", AdmonitionQuestion},
        {"warning", AdmonitionWarning},   {"warn", AdmonitionWarning},     {"attention", AdmonitionWarning},
        {"caution", AdmonitionCaution},   {"failure", AdmonitionFailure},  {"fail", AdmonitionFailure},
        {"missing", AdmonitionFailure},   {"danger", AdmonitionDanger},    {"error", AdmonitionDanger},
        {"bug", AdmonitionBug},           {"example", AdmonitionExample},  {"quote", AdmonitionQuote},
        {"cite", AdmonitionQuote},        {"details", AdmonitionDetails},  {"dropdown", AdmonitionDetails},
    };
    const std::string lower = admonitionLower(keyword);
    for (const auto& n : kNames) {
        if (lower == n.name) return n.kind;
    }
    return AdmonitionNone;
}

// The title a box shows when its source gives none: the keyword as a word
inline std::string admonitionLabel(std::string_view keyword) {
    const std::string lower = admonitionLower(keyword);
    if (lower == "seealso") return "See also";
    if (lower == "tldr") return "TL;DR";
    if (lower == "faq") return "FAQ";
    if (lower == "admonition" || lower.empty()) return "Note";
    std::string label = lower;
    for (char& c : label) {
        if (c == '-' || c == '_') c = ' ';
    }
    label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
    return label;
}

}  // namespace qmd

#endif  // TINTA_ADMONITION_H
