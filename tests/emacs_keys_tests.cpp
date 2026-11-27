// The Emacs shortcut profile (emacs_keys.cpp), driven through the real key
// handlers with the modifier state set as a keyboard would
#include "d2d_init.h"
#include "editor.h"
#include "emacs_keys.h"
#include "input.h"
#include "render.h"
#include "search.h"
#include "settings.h"

#include <cstring>
#include <fstream>
#include <functional>
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

struct Mods { bool ctrl = false, shift = false, alt = false; };
const Mods C{true}, CS{true, true}, M{false, false, true}, MS{false, true, true};

void withKeys(Mods mods, const std::function<void()>& action) {
    BYTE original[256], pressed[256];
    GetKeyboardState(original);
    std::memcpy(pressed, original, sizeof pressed);
    pressed[VK_CONTROL] = mods.ctrl ? 0x80 : 0;
    pressed[VK_SHIFT] = mods.shift ? 0x80 : 0;
    pressed[VK_MENU] = mods.alt ? 0x80 : 0;
    SetKeyboardState(pressed);
    action();
    SetKeyboardState(original);
}

// WM_KEYDOWN; true when consumed (no WM_CHAR follows)
bool key(App& app, unsigned vk, Mods mods = {}) {
    bool consumed = false;
    withKeys(mods, [&] { consumed = handleKeyDown(app, app.hwnd, vk); });
    return consumed;
}

// WM_SYSKEYDOWN, as the window procedure hands it over
bool meta(App& app, unsigned vk, bool shift = false) {
    bool handled = false;
    withKeys(shift ? MS : M, [&] { handled = emacsSysKeyDown(app, app.hwnd, vk); });
    return handled;
}

bool closePosted(App& app) {
    MSG msg;
    bool posted = false;
    while (PeekMessageW(&msg, app.hwnd, WM_CLOSE, WM_CLOSE, PM_REMOVE)) posted = true;
    return posted;
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

void setText(App& app, const std::wstring& text, size_t caret) {
    editorClearExtraCarets(app);
    app.editorText = text;
    rebuildLineStarts(app);
    app.undoStack.clear();
    app.redoStack.clear();
    app.editorCursorPos = app.editorSelStart = app.editorSelEnd = caret;
    app.editorHasSelection = false;
    app.editorDesiredCol = -1;
    app.emacsMark = false;
    app.emacsKillAt = (size_t)-1;
}

std::string read(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

void reader(App& app) {
    app.scrollY = app.targetScrollY = 0;
    // One line, a page, the ends
    key(app, 'N', C);
    check(app.scrollY == 50.0f, "C-n scrolls a line down, got " + std::to_string(app.scrollY));
    key(app, 'P', C);
    check(app.scrollY == 0.0f, "C-p scrolls back up");
    key(app, 'V', C);
    float page = app.scrollY;
    check(page > 100.0f, "C-v scrolls a page, got " + std::to_string(page));
    check(meta(app, 'V') && app.scrollY == 0.0f, "M-v scrolls a page back");
    check(meta(app, VK_OEM_PERIOD, true) && app.scrollY > page, "M-> goes to the end");
    check(meta(app, VK_OEM_COMMA, true) && app.scrollY == 0.0f, "M-< goes to the top");

    // Search, and C-g: closes it, but never the window
    closePosted(app);
    key(app, VK_ESCAPE);
    check(closePosted(app), "the checker: a bare Esc in the reader asks to close the window");
    key(app, 'S', C);
    check(app.showSearch && app.searchActive, "C-s opens the search");
    key(app, 'G', C);
    check(!app.showSearch, "C-g closes the search");
    key(app, 'G', C);
    check(!closePosted(app), "C-g with nothing open leaves the window alone");

    // C-x h selects everything, M-w copies it
    key(app, 'X', C);
    check(app.emacsPrefix, "C-x waits for a key");
    check(key(app, 'H'), "C-x h is consumed");
    check(!app.emacsPrefix && app.hasSelection && app.selAnchor == 0 &&
              app.selFocus == app.docText.size(),
          "C-x h selects the whole document");
    check(meta(app, 'W') && clipboard(app.hwnd).find(L"Emacs keys") != std::wstring::npos,
          "M-w copies the selection");
    // An unknown key after C-x is reported, not typed
    key(app, 'X', C);
    check(key(app, 'Z'), "C-x z is consumed, not typed");
    check(!app.emacsPrefix, "and ends the prefix");
}

void editor(App& app) {
    const std::wstring text = L"alpha beta gamma\nsecond line\nthird";
    setText(app, text, 0);
    key(app, 'F', C);
    check(app.editorCursorPos == 1, "C-f moves forward a character");
    key(app, 'B', C);
    check(app.editorCursorPos == 0, "C-b moves back");
    key(app, 'E', C);
    check(app.editorCursorPos == 16, "C-e goes to the end of the line");
    key(app, 'A', C);
    check(app.editorCursorPos == 0, "C-a goes to its start");
    key(app, 'N', C);
    check(app.editorCursorPos == 17, "C-n goes to the next line");
    key(app, 'P', C);
    check(app.editorCursorPos == 0, "C-p back to the previous");
    meta(app, 'F');
    check(app.editorCursorPos == 5, "M-f goes to the end of the word");
    meta(app, 'F');
    meta(app, 'B');
    check(app.editorCursorPos == 6, "M-b to the start of the word");

    // The mark: motions grow a region from it
    setText(app, text, 0);
    key(app, VK_SPACE, C);
    for (int i = 0; i < 5; i++) key(app, 'F', C);
    check(app.editorHasSelection && app.editorSelStart == 0 && app.editorCursorPos == 5,
          "C-SPC then C-f x5 selects 'alpha'");
    meta(app, 'W');
    check(clipboard(app.hwnd) == L"alpha" && !app.editorHasSelection && !app.emacsMark &&
              app.editorText == text,
          "M-w copies the region and drops the mark");
    setText(app, text, 0);
    key(app, VK_SPACE, C);
    meta(app, 'F');
    key(app, 'W', C);
    check(app.editorText == text.substr(5) && clipboard(app.hwnd) == L"alpha", "C-w kills the region");
    key(app, 'Y', C);
    check(app.editorText == text && app.editorCursorPos == 5, "C-y yanks it back");
    setText(app, text, 0);
    key(app, VK_SPACE, C);
    meta(app, 'F');
    key(app, 'X', C);
    key(app, 'X', C);
    check(app.editorCursorPos == 0 && app.editorHasSelection && app.editorSelStart == 5,
          "C-x C-x swaps the caret and the mark");
    key(app, 'G', C);
    check(!app.editorHasSelection && !app.emacsMark && app.editMode,
          "C-g drops the region and stays in the editor");

    // Kills: consecutive ones gather in the clipboard
    setText(app, text, 6);
    key(app, 'K', C);
    check(app.editorText == L"alpha \nsecond line\nthird" && clipboard(app.hwnd) == L"beta gamma",
          "C-k kills to the end of the line");
    key(app, 'K', C);
    check(app.editorText == L"alpha second line\nthird" && clipboard(app.hwnd) == L"beta gamma\n",
          "a second C-k takes the line break, appended");
    key(app, VK_OEM_2, C);
    key(app, VK_OEM_2, C);
    check(app.editorText == text, "C-/ undoes both kills");
    setText(app, L"one two three", 3);
    meta(app, 'D');
    check(app.editorText == L"one three" && clipboard(app.hwnd) == L" two", "M-d kills a word forward");
    setText(app, L"one two", 7);
    meta(app, VK_BACK);
    check(app.editorText == L"one " && clipboard(app.hwnd) == L"two", "M-DEL kills a word back");
    meta(app, VK_BACK);
    check(app.editorText.empty() && clipboard(app.hwnd) == L"one two",
          "a second M-DEL prepends to the kill");
    key(app, 'X', C);
    key(app, 'U');
    check(app.editorText == L"one ", "C-x u undoes");

    // Small edits
    setText(app, L"abc", 1);
    key(app, 'D', C);
    check(app.editorText == L"ac", "C-d deletes forward");
    setText(app, L"abcd", 2);
    key(app, 'T', C);
    check(app.editorText == L"acbd" && app.editorCursorPos == 3, "C-t transposes and steps on");
    key(app, VK_OEM_2, C);
    check(app.editorText == L"abcd", "C-t undoes in one step");
    setText(app, L"ab", 1);
    key(app, 'O', C);
    check(app.editorText == L"a\nb" && app.editorCursorPos == 1, "C-o opens a line, the caret stays");
    setText(app, text, 3);
    key(app, 'X', C);
    key(app, 'H');
    check(app.editorHasSelection && app.editorSelStart == 0 && app.editorSelEnd == text.size(),
          "C-x h selects the buffer");

    // Search from the editor; C-g closes it and stays
    setText(app, text, 0);
    key(app, 'S', C);
    check(app.showSearch && app.searchActive, "C-s opens the search in the editor");
    key(app, 'G', C);
    check(!app.showSearch && app.editMode && !closePosted(app), "C-g closes it, still editing");

    // AltGr (Ctrl+Alt) is a character, never a chord
    bool taken = true;
    withKeys({true, false, true}, [&] { taken = emacsKeyDown(app, app.hwnd, 'F'); });
    check(!taken, "Ctrl+Alt+F (AltGr) is left alone");
    withKeys({true, false, true}, [&] { taken = emacsSysKeyDown(app, app.hwnd, 'F'); });
    check(!taken, "and not taken as Meta either");
}

}  // namespace

int main() {
    auto state = std::make_unique<App>();
    App& app = *state;
    app.hwnd = CreateWindowExW(0, L"STATIC", L"Emacs key tests", WS_POPUP, 0, 0, 900, 500, nullptr,
                               nullptr, nullptr, nullptr);
    if (!app.hwnd || !initD2D(app) || !createRenderTarget(app)) return 2;
    app.width = 900;
    app.height = 500;
    app.contentScale = 1.0f;
    updateTextFormats(app);
    Settings settings;
    settings.keyProfile = "emacs";
    applyKeymap(app, settings);
    check(app.keyProfile == "emacs", "the emacs profile applies");

    app.currentFile = TINTA_EMACS_FIXTURE;
    app.root = app.parser.parse(read(TINTA_EMACS_FIXTURE)).root;
    layoutDocument(app);
    reader(app);

    key(app, 'X', C);
    key(app, 'Q', C);
    check(app.editMode && sourceEditorHasFocus(app), "C-x C-q opens the editor");
    editor(app);
    setText(app, L"", 0);
    app.editorDirty = false;
    key(app, 'X', C);
    key(app, 'Q', C);
    check(!app.editMode, "C-x C-q goes back to reading");

    // Other profiles are untouched: Ctrl+V does not page in the Windows one
    settings.keyProfile = "windows";
    applyKeymap(app, settings);
    app.scrollY = app.targetScrollY = 0;
    bool taken = true;
    withKeys(C, [&] { taken = emacsKeyDown(app, app.hwnd, 'V'); });
    key(app, 'V', C);
    check(!taken && app.scrollY == 0.0f, "the windows profile ignores Emacs chords");

    // Menu hints follow the profile (the windows one is active again here)
    check(std::wstring(profileChord(app, L"Ctrl+C")) == L"Ctrl+C", "windows menus show Ctrl+C");
    settings.keyProfile = "emacs";
    applyKeymap(app, settings);
    check(std::wstring(profileChord(app, L"Ctrl+C")) == L"M-w" &&
              std::wstring(profileChord(app, L"Ctrl+Shift+S")) == L"C-x C-w" &&
              std::wstring(profileChord(app, L"Ctrl+P")).empty() &&
              std::wstring(profileChord(app, L"Ctrl+,")) == L"Ctrl+,",
          "emacs menus show its chords, none where Emacs took the key, the rest unchanged");

    std::cout << "emacs keys: " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
