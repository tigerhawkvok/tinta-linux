// The window record behind HWND, shared by the Win32 message layer
// (win32_window.cpp) and the GTK window layer (gtk_window.cpp).
#ifndef TINTA_LINUX_WINDOW_INTERNAL_H
#define TINTA_LINUX_WINDOW_INTERNAL_H

#include "compat_internal.h"

#include <commdlg.h>

#include <string>
#include <vector>

struct HWND__ {
    std::wstring className;
    std::wstring title;
    WNDPROC proc = nullptr;
    RECT rect{0, 0, 1000, 800};  // screen coordinates; with GTK, (0,0) + client size in device pixels
    LONG style = 0;
    LONG exStyle = 0;
    LONG_PTR userData = 0;
    bool visible = false;
    bool zoomed = false;
    bool iconic = false;
    bool destroyed = false;
    bool dirty = false;    // needs WM_PAINT (InvalidateRect until BeginPaint)
    void* native = nullptr;  // the GTK side (gtk_window.cpp), when interactive
};

// WM_DROPFILES' handle: the paths, in drop order; DragFinish frees it
struct HDROP__ {
    std::vector<std::wstring> files;
};

namespace tinta {

// Posts a message without waking anything (the caller is the GTK main
// thread, which reads the queue next)
void postFromUi(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace gtk {

// True when the app runs interactively (main_linux.cpp started GTK)
bool active();
bool start();  // gtk_init_check; false without a display

void createWindow(HWND hwnd);
void showWindow(HWND hwnd, int cmd);
void destroyWindow(HWND hwnd);
void invalidate(HWND hwnd);
void setTitle(HWND hwnd);
void setCursor(LPCWSTR id);
bool isMaximized(HWND hwnd);
bool isMinimized(HWND hwnd);
double scale(HWND hwnd);
// Window-manager actions DefWindowProc performs on Windows
void beginMove(HWND hwnd);
void beginResize(HWND hwnd, int hitCode);
void minimize(HWND hwnd);
void toggleMaximize(HWND hwnd);
POINT cursorPos();

// Wakes a blocked waitForEvents from any thread
void wake();
// Runs GTK until an event arrives, wake() is called or `timeoutMs` passes
// (-1: no timeout)
void waitForEvents(int timeoutMs);

// Text for the WM_CHAR a WM_KEYDOWN translates to (TranslateMessage)
bool charForKey(const MSG& msg, std::vector<wchar_t>& out);

// Clipboard and dialogs, run in a nested main loop
void publishClipboardText(const std::wstring& text);
bool readClipboardText(std::wstring& text);
bool clipboardHasText();
bool clipboardIsLocal();  // this process set the current clipboard
bool fileDialog(OPENFILENAMEW* ofn, bool save);
int messageBox(HWND owner, LPCWSTR text, LPCWSTR caption, UINT type);

}  // namespace gtk
}  // namespace tinta

#endif  // TINTA_LINUX_WINDOW_INTERNAL_H
