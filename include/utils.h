#ifndef TINTA_UTILS_H
#define TINTA_UTILS_H

#include "app.h"
#include <string>
#include <string_view>

// Simple inline element rendering
struct InlineSpan {
    std::wstring text;
    D2D1_COLOR_F color;
    IDWriteTextFormat* format;
    std::string linkUrl;
    bool underline;
};

std::wstring toWide(const std::string& str);
float measureText(App& app, const std::wstring& text, IDWriteTextFormat* format);
std::wstring toLower(const std::wstring& str);
std::wstring_view textViewForRect(const App& app, const App::TextRect& tr);

// Word/line boundary helpers (offset-based word/line selection lives in
// selection.h)
bool isWordBoundary(wchar_t c);
const App::TextRect* findTextRectAt(const App& app, int x, int y);

std::wstring windowTitle(const App& app);
void updateWindowTitle(App& app);
// Ctrl+N: spawns a second Tinta window on an untitled quick note
void launchQuickNoteWindow();
void openUrl(const std::string& url);
bool copyToClipboard(HWND hwnd, const std::wstring& text);
// What a code block's Copy button puts on the clipboard: its lines without
// the line break that ends the last one (#269)
std::wstring codeCopyText(const std::wstring& code);
// Resolve UTF-8 document paths without a MAX_PATH-sized buffer (#203).
std::wstring absoluteFilePath(const std::string& path);
void copyFilePath(App& app, HWND hwnd, const std::string& path);
void extractText(const ElementPtr& elem, std::wstring& out);

std::string slugifyHeading(const std::wstring& text);
// Where a jump lands its target, below the top of the view: clear of the
// title-bar tab strip that draws over the document's top band
float readingLineOffset(const App& app);
void scrollToHeadingY(App& app, float headingY);
// Document Y of an in-document link target: a footnote, a heading slug, or
// an <a id>/<a name> anchor (#255)
bool documentTargetY(App& app, const std::string& id, float& y);
bool scrollToHeadingId(App& app, const std::string& id);
void handleLinkClick(App& app);

#endif // TINTA_UTILS_H
