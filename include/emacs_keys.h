#ifndef TINTA_EMACS_KEYS_H
#define TINTA_EMACS_KEYS_H

#include <windows.h>

struct App;

// The Emacs shortcut profile ("emacs"): Ctrl chords and the C-x prefix
// (WM_KEYDOWN), Meta chords (WM_SYSKEYDOWN), in the viewer and the source
// editor. Each returns true when it consumed the key; a key it does not
// know keeps its Windows meaning. Inert in every other profile.
bool emacsKeyDown(App& app, HWND hwnd, WPARAM key);
// A menu's shortcut hint, written as the Windows chord, in the active
// profile: the Emacs chord for the same command, or "" where Emacs took
// the chord over and the command has none
const wchar_t* profileChord(const App& app, const wchar_t* windowsChord);
bool emacsSysKeyDown(App& app, HWND hwnd, WPARAM key);

#endif
