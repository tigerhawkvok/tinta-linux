#include "emacs_keys.h"

#include "app.h"
#include "editor.h"
#include "i18n.h"
#include "input.h"
#include "search.h"
#include "signals.h"
#include "tabs.h"

#include <cstring>
#include <string>

// Most chords stand for a key Tinta already handles: they are re-sent as
// that key with exactly the modifiers it needs, so its existing behaviour
// (every caret, the search bar, the preview, undo) applies unchanged. Kills,
// the mark, C-t, C-o and C-x are Emacs's own and live in editor.cpp/here.

namespace {

bool held(int key) { return (GetKeyState(key) & 0x8000) != 0; }

bool isModifier(WPARAM key) {
    switch (key) {
        case VK_SHIFT: case VK_CONTROL: case VK_MENU:
        case VK_LSHIFT: case VK_RSHIFT: case VK_LCONTROL: case VK_RCONTROL:
        case VK_LMENU: case VK_RMENU: case VK_LWIN: case VK_RWIN: case VK_CAPITAL:
            return true;
        default:
            return false;
    }
}

// Re-dispatch as `vk` with only these modifiers held. Afterwards each
// modifier gets its real state back, unless a modal loop (a file dialog)
// ran meanwhile and already brought the true state in.
bool send(App& app, HWND hwnd, WPARAM vk, bool ctrl = false, bool shift = false) {
    BYTE saved[256], keys[256];
    GetKeyboardState(saved);
    std::memcpy(keys, saved, sizeof keys);
    const int modifiers[] = {VK_CONTROL, VK_LCONTROL, VK_RCONTROL, VK_SHIFT, VK_LSHIFT,
                             VK_RSHIFT, VK_MENU, VK_LMENU, VK_RMENU};
    auto set = [&](int key, bool down) {
        keys[key] = down ? (BYTE)(keys[key] | 0x80) : (BYTE)(keys[key] & ~0x80);
    };
    set(VK_CONTROL, ctrl);
    set(VK_LCONTROL, ctrl);
    set(VK_RCONTROL, false);
    set(VK_SHIFT, shift);
    set(VK_LSHIFT, shift);
    set(VK_RSHIFT, false);
    set(VK_MENU, false);
    set(VK_LMENU, false);
    set(VK_RMENU, false);
    SetKeyboardState(keys);
    app.emacsDispatching = true;
    bool consumed = handleKeyDown(app, hwnd, vk);
    app.emacsDispatching = false;
    BYTE now[256];
    GetKeyboardState(now);
    for (int key : modifiers) {
        if (now[key] == keys[key]) now[key] = saved[key];
    }
    SetKeyboardState(now);
    return consumed;
}

// The keyboard belongs to a dialog, a menu or a text field: only C-g
// (as Esc) reaches it
bool keyboardCaptured(const App& app) {
    return app.showLightbox || app.annotEditorOpen || app.createRefPending || app.pasteImagePending ||
           app.showPrintPreview ||
           app.showContextMenu || app.showShortcutEditor || app.showThemeEditor || app.showSettings ||
           app.showHelp || app.showThemeChooser || app.confirmExitPending || app.appMenuPressed ||
           app.panelResize.panel != SidePanel::None || app.tableEditActive || app.editCtxOpen ||
           app.showTabSwitcher || app.showTabMenu ||
           (app.showFolderBrowser && (app.folderBrowserEditingPath || app.folderBrowserNaming != 0));
}

bool inEditor(const App& app) { return sourceEditorHasFocus(app); }

bool searching(const App& app) { return app.showSearch && app.searchActive; }

void hint(App& app, HWND hwnd, const std::wstring& text) {
    signalHint(app, SIGI_INFO, text);
    InvalidateRect(hwnd, nullptr, FALSE);
}

std::wstring keyName(WPARAM key, bool ctrl) {
    std::wstring name = ctrl ? L"C-" : L"";
    if (key >= 'A' && key <= 'Z') name += (wchar_t)(key - 'A' + 'a');
    else if (key >= '0' && key <= '9') name += (wchar_t)key;
    else if (key == VK_SPACE) name += L"SPC";
    else if (key == VK_RETURN) name += L"RET";
    else if (key == VK_TAB) name += L"TAB";
    else if (key == VK_LEFT) name += L"<left>";
    else if (key == VK_RIGHT) name += L"<right>";
    else name += L"<key>";
    return name;
}

// C-g, keyboard-quit: cancels what is open. Unlike Esc it never closes the
// window or leaves the editor.
bool quit(App& app, HWND hwnd) {
    app.emacsPrefix = false;
    if (keyboardCaptured(app) || searching(app)) {
        send(app, hwnd, VK_ESCAPE);
    } else if (inEditor(app)) {
        if (app.showSearch) closeSearchInput(app);
        editorEmacsQuit(app, hwnd);
    } else if (app.showContextMenu || app.showSearch || app.showFolderBrowser || app.showToc) {
        send(app, hwnd, VK_ESCAPE);
    } else {
        app.hasSelection = false;
    }
    hint(app, hwnd, tr(app, "emacs.quit"));
    return true;
}

bool copyViewerSelection(App& app, HWND hwnd) {
    if (app.hasSelection && app.selAnchor != app.selFocus) send(app, hwnd, 'C', true);
    return true;
}

bool viewerChord(App& app, HWND hwnd, WPARAM key, bool shift) {
    if (shift) return false;
    switch (key) {
        case 'N': send(app, hwnd, VK_DOWN); return true;
        case 'P': send(app, hwnd, VK_UP); return true;
        case 'V': send(app, hwnd, VK_NEXT); return true;
        case 'F': send(app, hwnd, VK_RIGHT); return true;  // wide content scrolls
        case 'B': send(app, hwnd, VK_LEFT); return true;
        case 'S':
        case 'R':
            openSearchInput(app);
            InvalidateRect(hwnd, nullptr, FALSE);
            return true;
        case 'W': return copyViewerSelection(app, hwnd);  // read-only: a kill copies
        default: return false;
    }
}

bool editorChord(App& app, HWND hwnd, WPARAM key, bool shift) {
    bool mark = editorEmacsMarkActive(app);
    // Motions extend the region while the mark is set
    auto move = [&](WPARAM vk, bool word) {
        send(app, hwnd, vk, word, mark);
        return true;
    };
    if (key == VK_OEM_2) return send(app, hwnd, shift ? 'Y' : 'Z', true), true;  // C-/ undo, C-? redo
    if (key == VK_OEM_MINUS && shift) return send(app, hwnd, 'Z', true), true;    // C-_ undo
    if ((key == '2' && shift) || (key == VK_SPACE && !shift)) {                    // C-@, C-SPC
        editorEmacsSetMark(app);
        hint(app, hwnd, tr(app, "emacs.mark_set"));
        return true;
    }
    if (shift) return false;
    switch (key) {
        case 'F': return move(VK_RIGHT, false);
        case 'B': return move(VK_LEFT, false);
        case 'N': return move(VK_DOWN, false);
        case 'P': return move(VK_UP, false);
        case 'A': return move(VK_HOME, false);
        case 'E': return move(VK_END, false);
        case 'V': return move(VK_NEXT, false);
        case 'D':
            app.emacsMark = false;
            send(app, hwnd, VK_DELETE);
            return true;
        case 'K': editorEmacsKill(app, hwnd, EmacsKill::Line); return true;
        case 'Y':
            app.emacsMark = false;
            send(app, hwnd, 'V', true);
            return true;
        case 'W':
            if (app.editorHasSelection || !app.editorExtraCarets.empty()) send(app, hwnd, 'X', true);
            app.emacsMark = false;
            return true;
        case 'S':
        case 'R':
            openSearchInput(app);
            InvalidateRect(hwnd, nullptr, FALSE);
            return true;
        case 'O': editorEmacsOpenLine(app, hwnd); return true;
        case 'J': handleCharInput(app, hwnd, L'\r'); return true;
        case 'T': editorEmacsTranspose(app, hwnd); return true;
        default: return false;
    }
}

// The key after C-x
bool prefixed(App& app, HWND hwnd, WPARAM key, bool ctrl) {
    bool editor = inEditor(app);
    if (ctrl) {
        switch (key) {
            case 'F': openFileDialog(app, hwnd); return true;
            case 'S':
                if (app.editMode) saveEditorFile(app, hwnd);
                return true;
            case 'W':
                if (app.editMode) saveEditorFileAs(app, hwnd);
                else saveFileAs(app, hwnd);
                return true;
            case 'C': PostMessageW(hwnd, WM_CLOSE, 0, 0); return true;  // unsaved tabs still ask
            case 'Q':  // read-only toggle: the editor and the reader
                if (app.editMode) exitEditMode(app);
                else enterEditMode(app);
                InvalidateRect(hwnd, nullptr, FALSE);
                return true;
            case 'X':
                if (editor) editorEmacsExchangePointAndMark(app, hwnd);
                return true;
            case 'G': return quit(app, hwnd);
        }
    } else {
        switch (key) {
            case 'K': tabCloseIndex(app, hwnd, app.activeTab); return true;
            case 'B':
            case VK_RIGHT: tabCycle(app, hwnd, 1); return true;
            case VK_LEFT: tabCycle(app, hwnd, -1); return true;
            case 'H': send(app, hwnd, 'A', true); return true;  // mark-whole-buffer
            case 'U':
                if (editor) send(app, hwnd, 'Z', true);
                return true;
        }
    }
    wchar_t text[96];
    swprintf_s(text, _countof(text), tr(app, "emacs.undefined"),
               (L"C-x " + keyName(key, ctrl)).c_str());
    hint(app, hwnd, text);
    return true;
}

}  // namespace

bool emacsKeyDown(App& app, HWND hwnd, WPARAM key) {
    if (app.keyProfile != "emacs" || app.emacsDispatching || isModifier(key)) return false;
    bool ctrl = held(VK_CONTROL), shift = held(VK_SHIFT);
    if (held(VK_MENU)) return false;  // Meta is WM_SYSKEYDOWN; Ctrl+Alt is AltGr
    if (app.emacsPrefix) {
        app.emacsPrefix = false;
        return prefixed(app, hwnd, key, ctrl);
    }
    if (!ctrl) return false;  // single keys keep the profile's bindings
    if (key == 'G' && !shift) return quit(app, hwnd);
    if (keyboardCaptured(app)) return false;
    if (searching(app)) {  // isearch: C-s and C-r step through the matches
        if (key == 'S' && !shift) return send(app, hwnd, VK_RETURN), true;
        if (key == 'R' && !shift) return send(app, hwnd, VK_RETURN, false, true), true;
        return false;
    }
    if (key == 'X' && !shift) {
        app.emacsPrefix = true;
        hint(app, hwnd, L"C-x-");
        return true;
    }
    if (key == 'H' && !shift) {  // help
        app.showHelp = true;
        app.helpAnimation = 0;
        InvalidateRect(hwnd, nullptr, FALSE);
        return true;
    }
    return inEditor(app) ? editorChord(app, hwnd, key, shift) : viewerChord(app, hwnd, key, shift);
}

bool emacsSysKeyDown(App& app, HWND hwnd, WPARAM key) {
    if (app.keyProfile != "emacs" || app.emacsDispatching || isModifier(key)) return false;
    if (held(VK_CONTROL)) return false;  // Ctrl+Alt: AltGr, or the caret chords
    app.emacsPrefix = false;
    if (keyboardCaptured(app) || searching(app)) return false;
    bool shift = held(VK_SHIFT);
    bool editor = inEditor(app);
    bool mark = editor && editorEmacsMarkActive(app);
    bool lessThan = (key == VK_OEM_COMMA && shift) || (key == VK_OEM_102 && !shift);
    bool greaterThan = (key == VK_OEM_PERIOD && shift) || (key == VK_OEM_102 && shift);
    bool handled = true;
    if (key == 'V' && !shift) {
        send(app, hwnd, VK_PRIOR, false, mark);
    } else if (lessThan) {  // M-<: beginning of buffer
        send(app, hwnd, VK_HOME, editor, mark);
    } else if (greaterThan) {  // M->: end of buffer
        send(app, hwnd, VK_END, editor, mark);
    } else if (key == 'W' && !shift) {  // M-w: copy the region, drop the mark
        if (!editor) {
            copyViewerSelection(app, hwnd);
        } else if (app.editorHasSelection || !app.editorExtraCarets.empty()) {
            send(app, hwnd, 'C', true);
            app.editorHasSelection = false;
            app.emacsMark = false;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    } else if (key == '5' && shift) {  // M-%: query replace
        if (!app.editMode) enterEditMode(app);
        openSearchInput(app, true);
        InvalidateRect(hwnd, nullptr, FALSE);
    } else if (editor && !shift && key == 'F') {
        send(app, hwnd, VK_RIGHT, true, mark);
    } else if (editor && !shift && key == 'B') {
        send(app, hwnd, VK_LEFT, true, mark);
    } else if (editor && !shift && key == 'D') {
        editorEmacsKill(app, hwnd, EmacsKill::WordForward);
    } else if (editor && !shift && key == VK_BACK) {
        editorEmacsKill(app, hwnd, EmacsKill::WordBackward);
    } else {
        handled = false;
    }
    // On Windows the chord's WM_SYSCHAR would otherwise beep
    if (handled) app.emacsSwallowSysChar = true;
    return handled;
}
