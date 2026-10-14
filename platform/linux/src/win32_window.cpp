// Win32 windows, messages, timers, capture, cursors, clipboard, GDI, IME and
// dialogs for the Linux compat layer. Phase 1 is headless: windows are
// records with a size, the message queue and timers are real (worker threads
// post results to it), and everything visual is inert. The GTK window layer
// takes over the window functions in phase 2.
#include "compat_internal.h"

#include <commdlg.h>
#include <ole2.h>
#include <imm.h>
#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

struct HWND__ {
    std::wstring className;
    std::wstring title;
    WNDPROC proc = nullptr;
    RECT rect{0, 0, 1000, 800};  // screen coordinates
    LONG style = 0;
    LONG exStyle = 0;
    LONG_PTR userData = 0;
    bool visible = false;
    bool zoomed = false;
    bool iconic = false;
    bool destroyed = false;
    bool dirty = false;  // needs WM_PAINT (InvalidateRect until BeginPaint)
};

namespace {

using Clock = std::chrono::steady_clock;

struct Timer {
    HWND hwnd;
    UINT_PTR id;
    std::chrono::milliseconds interval;
    Clock::time_point due;
};

struct State {
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<MSG> queue;
    std::vector<Timer> timers;
    std::map<std::wstring, WNDPROC> classes;
    std::vector<HWND> windows;
    HWND capture = nullptr;
    HWND foreground = nullptr;
    HCURSOR cursor = nullptr;
    bool quit = false;
    int paintCount = 0;  // BeginPaint calls, all windows
    int quitCode = 0;
    std::map<UINT, HGLOBAL> clipboard;
    HWND clipboardOpener = nullptr;
    HWND clipboardOwner = nullptr;
    std::map<std::wstring, UINT> clipboardFormats;
    UINT nextClipboardFormat = 0xC000;
};

State& state() {
    static State s;
    return s;
}

// Moves due timers into the queue as WM_TIMER; returns the next deadline
Clock::time_point fireTimers(State& s) {
    Clock::time_point now = Clock::now(), next = Clock::time_point::max();
    for (Timer& t : s.timers) {
        if (t.due <= now) {
            s.queue.push_back(MSG{t.hwnd, WM_TIMER, t.id, 0, 0, {0, 0}});
            t.due = now + t.interval;
        }
        next = std::min(next, t.due);
    }
    return next;
}

bool takeMessage(State& s, MSG* msg, HWND hwnd, UINT min, UINT max, bool remove) {
    for (auto it = s.queue.begin(); it != s.queue.end(); ++it) {
        bool windowMatch = !hwnd || it->hwnd == hwnd;
        bool rangeMatch = (min == 0 && max == 0) || (it->message >= min && it->message <= max);
        if (windowMatch && rangeMatch) {
            *msg = *it;
            if (remove) s.queue.erase(it);
            return true;
        }
    }
    return false;
}

// As on Windows, WM_PAINT is never queued: it is synthesised for a dirty
// window once nothing else is waiting, and repeats until BeginPaint
bool takePaint(State& s, MSG* msg, HWND hwnd, UINT min, UINT max) {
    bool rangeMatch = (min == 0 && max == 0) || (WM_PAINT >= min && WM_PAINT <= max);
    if (!rangeMatch) return false;
    for (HWND w : s.windows) {
        if (w->dirty && !w->destroyed && (!hwnd || w == hwnd)) {
            *msg = MSG{w, WM_PAINT, 0, 0, 0, {0, 0}};
            return true;
        }
    }
    return false;
}

}  // namespace

namespace tinta {

HeadlessOptions& headless() {
    static HeadlessOptions options;
    return options;
}

}  // namespace tinta

namespace {

// Writes `surface` as a PNG; 0 on success
int writeCapture(cairo_surface_t* surface, const std::string& path) {
    cairo_surface_flush(surface);
    cairo_status_t status = cairo_surface_write_to_png(surface, path.c_str());
    if (status != CAIRO_STATUS_SUCCESS) {
        std::fprintf(stderr, "render-png: %s: %s\n", path.c_str(), cairo_status_to_string(status));
        return 1;
    }
    return 0;
}

// --render-png: runs the app's messages (posted results, timers, layout
// chunks, paints) until nothing has happened for settleMs, then saves the
// main window's surface. Returns the process exit code.
int runCapture() {
    State& s = state();
    const tinta::HeadlessOptions& options = tinta::headless();
    auto start = Clock::now(), quietSince = start;
    int paints = 0, dispatched = 0;
    bool timedOut = false;
    while (true) {
        MSG msg{};
        bool got = false, paint = false;
        {
            std::lock_guard<std::mutex> lock(s.mutex);
            fireTimers(s);
            got = takeMessage(s, &msg, nullptr, 0, 0, true);
            if (!got) paint = takePaint(s, &msg, nullptr, 0, 0);
            if (s.quit) break;
        }
        if (got || paint) {
            DispatchMessageW(&msg);
            dispatched++;
            if (paint) paints++;
            // Timers tick forever (file watch, drafts); only their paints count
            if (msg.message != WM_TIMER) quietSince = Clock::now();
            continue;
        }
        auto now = Clock::now();
        if (now - quietSince >= std::chrono::milliseconds(options.settleMs)) break;
        if (now - start >= std::chrono::milliseconds(options.timeoutMs)) {
            timedOut = true;
            break;
        }
        std::unique_lock<std::mutex> lock(s.mutex);
        s.wake.wait_for(lock, std::chrono::milliseconds(5));
    }
    double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    HWND main = nullptr;
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        for (HWND w : s.windows) {
            if (!w->destroyed && tinta::windowSurface(w)) {
                main = w;
                break;
            }
        }
    }
    std::fprintf(stderr, "render-png: settled in %.0f ms (%d messages, %d loop paints, %d total)%s\n", ms, dispatched, paints, s.paintCount,
                 timedOut ? " [timeout]" : "");
    if (!main || s.paintCount == 0) {
        std::fprintf(stderr, "render-png: no painted window\n");
        return 1;
    }
    std::fprintf(stderr, "render-png: window \"%s\"\n", tinta::toUtf8(main->title).c_str());
    if (options.repaints > 0) {
        std::vector<double> frames;
        for (int i = 0; i < options.repaints; i++) {
            auto t0 = Clock::now();
            InvalidateRect(main, nullptr, FALSE);
            SendMessageW(main, WM_PAINT, 0, 0);
            frames.push_back(std::chrono::duration<double, std::milli>(Clock::now() - t0).count());
        }
        std::sort(frames.begin(), frames.end());
        std::fprintf(stderr, "render-png: %d repaints, median %.2f ms, max %.2f ms\n", options.repaints,
                     frames[frames.size() / 2], frames.back());
    }
    return writeCapture(tinta::windowSurface(main), options.renderPng);
}

}  // namespace

// ---- Windows ------------------------------------------------------------------------------
LRESULT DefWindowProcW(HWND, UINT, WPARAM, LPARAM) { return 0; }

ATOM RegisterClassExW(const WNDCLASSEXW* wc) {
    if (!wc || !wc->lpszClassName) return 0;
    std::lock_guard<std::mutex> lock(state().mutex);
    state().classes[wc->lpszClassName] = wc->lpfnWndProc;
    return static_cast<ATOM>(state().classes.size());
}

HWND CreateWindowExW(DWORD exStyle, LPCWSTR className, LPCWSTR windowName, DWORD style, int x, int y, int width,
                     int height, HWND, HMENU, HINSTANCE, void*) {
    auto* window = new HWND__();
    if (className) window->className = className;
    if (windowName) window->title = windowName;
    window->style = static_cast<LONG>(style);
    window->exStyle = static_cast<LONG>(exStyle);
    if (x == CW_USEDEFAULT) x = 100;
    if (y == CW_USEDEFAULT) y = 100;
    if (width == CW_USEDEFAULT || width <= 0) width = 1000;
    if (height == CW_USEDEFAULT || height <= 0) height = 800;
    if (tinta::headless().width > 0) width = tinta::headless().width;
    if (tinta::headless().height > 0) height = tinta::headless().height;
    window->rect = {x, y, x + width, y + height};
    window->visible = (style & WS_VISIBLE) != 0;
    std::lock_guard<std::mutex> lock(state().mutex);
    auto cls = state().classes.find(window->className);
    if (cls != state().classes.end()) window->proc = cls->second;
    state().windows.push_back(window);
    return window;
}

BOOL DestroyWindow(HWND hwnd) {
    if (!hwnd) return FALSE;
    std::lock_guard<std::mutex> lock(state().mutex);
    hwnd->destroyed = true;
    hwnd->visible = false;
    state().timers.erase(std::remove_if(state().timers.begin(), state().timers.end(),
                                        [hwnd](const Timer& t) { return t.hwnd == hwnd; }),
                         state().timers.end());
    return TRUE;
}

// Headless windows keep their size when maximized: there is no screen
BOOL ShowWindow(HWND hwnd, int cmd) {
    if (!hwnd) return FALSE;
    bool was = hwnd->visible;
    hwnd->visible = cmd != SW_HIDE;
    hwnd->zoomed = cmd == SW_SHOWMAXIMIZED;
    hwnd->iconic = cmd == SW_MINIMIZE || cmd == SW_SHOWMINIMIZED;
    if (cmd == SW_RESTORE) hwnd->zoomed = hwnd->iconic = false;
    if (hwnd->visible && !was) InvalidateRect(hwnd, nullptr, FALSE);
    return was;
}

// Paints now if the window is dirty, as Windows does
BOOL UpdateWindow(HWND hwnd) {
    if (hwnd && hwnd->dirty && hwnd->visible) SendMessageW(hwnd, WM_PAINT, 0, 0);
    return TRUE;
}

BOOL InvalidateRect(HWND hwnd, const RECT*, BOOL) {
    std::lock_guard<std::mutex> lock(state().mutex);
    if (hwnd) {
        hwnd->dirty = true;
    } else {
        for (HWND w : state().windows) w->dirty = true;
    }
    state().wake.notify_all();
    return TRUE;
}

HDC BeginPaint(HWND hwnd, PAINTSTRUCT* paint) {
    if (hwnd) {
        std::lock_guard<std::mutex> lock(state().mutex);
        hwnd->dirty = false;
        state().paintCount++;
    }
    if (paint) {
        std::memset(paint, 0, sizeof *paint);
        if (hwnd) GetClientRect(hwnd, &paint->rcPaint);
    }
    return nullptr;
}

BOOL EndPaint(HWND, const PAINTSTRUCT*) { return TRUE; }

BOOL SetWindowPos(HWND hwnd, HWND, int x, int y, int cx, int cy, UINT flags) {
    if (!hwnd) return FALSE;
    RECT& r = hwnd->rect;
    if (!(flags & SWP_NOMOVE)) {
        LONG w = r.right - r.left, h = r.bottom - r.top;
        r = {x, y, x + w, y + h};
    }
    if (!(flags & SWP_NOSIZE)) {
        r.right = r.left + cx;
        r.bottom = r.top + cy;
    }
    if (flags & SWP_SHOWWINDOW) hwnd->visible = true;
    return TRUE;
}

BOOL GetWindowRect(HWND hwnd, RECT* rect) {
    if (!hwnd || !rect) return FALSE;
    *rect = hwnd->rect;
    return TRUE;
}

BOOL GetClientRect(HWND hwnd, RECT* rect) {
    if (!hwnd || !rect) return FALSE;
    *rect = {0, 0, hwnd->rect.right - hwnd->rect.left, hwnd->rect.bottom - hwnd->rect.top};
    return TRUE;
}

BOOL SetWindowTextW(HWND hwnd, LPCWSTR text) {
    if (!hwnd) return FALSE;
    hwnd->title = text ? text : L"";
    return TRUE;
}

BOOL GetWindowPlacement(HWND hwnd, WINDOWPLACEMENT* placement) {
    if (!hwnd || !placement) return FALSE;
    placement->flags = 0;
    placement->showCmd = hwnd->zoomed ? SW_SHOWMAXIMIZED : hwnd->iconic ? SW_SHOWMINIMIZED : SW_SHOWNORMAL;
    placement->rcNormalPosition = hwnd->rect;
    return TRUE;
}

BOOL SetWindowPlacement(HWND hwnd, const WINDOWPLACEMENT* placement) {
    if (!hwnd || !placement) return FALSE;
    hwnd->rect = placement->rcNormalPosition;
    hwnd->zoomed = placement->showCmd == SW_SHOWMAXIMIZED;
    return TRUE;
}

LONG GetWindowLongW(HWND hwnd, int index) {
    if (!hwnd) return 0;
    if (index == GWL_STYLE) return hwnd->style;
    if (index == GWL_EXSTYLE) return hwnd->exStyle;
    return 0;
}

LONG SetWindowLongW(HWND hwnd, int index, LONG value) {
    if (!hwnd) return 0;
    LONG old = GetWindowLongW(hwnd, index);
    if (index == GWL_STYLE) hwnd->style = value;
    else if (index == GWL_EXSTYLE) hwnd->exStyle = value;
    return old;
}

LONG_PTR GetWindowLongPtrW(HWND hwnd, int index) {
    if (!hwnd) return 0;
    return index == GWLP_USERDATA ? hwnd->userData : GetWindowLongW(hwnd, index);
}

LONG_PTR SetWindowLongPtrW(HWND hwnd, int index, LONG_PTR value) {
    if (!hwnd) return 0;
    if (index != GWLP_USERDATA) return SetWindowLongW(hwnd, index, static_cast<LONG>(value));
    LONG_PTR old = hwnd->userData;
    hwnd->userData = value;
    return old;
}

BOOL IsZoomed(HWND hwnd) { return hwnd && hwnd->zoomed; }
BOOL IsIconic(HWND hwnd) { return hwnd && hwnd->iconic; }
BOOL IsWindowVisible(HWND hwnd) { return hwnd && hwnd->visible; }
BOOL IsWindow(HWND hwnd) { return hwnd && !hwnd->destroyed; }

BOOL SetForegroundWindow(HWND hwnd) {
    state().foreground = hwnd;
    return TRUE;
}

HWND GetForegroundWindow() { return state().foreground; }
HWND SetFocus(HWND hwnd) { return hwnd; }
HWND GetFocus() { return state().foreground; }
HWND GetAncestor(HWND hwnd, UINT) { return hwnd; }

// Windows of this process only: other Tinta processes are reached over
// D-Bus once the window layer exists (PLAN.md section 3)
HWND FindWindowExW(HWND, HWND after, LPCWSTR className, LPCWSTR windowName) {
    std::lock_guard<std::mutex> lock(state().mutex);
    bool passed = after == nullptr;
    for (HWND w : state().windows) {
        if (!passed) {
            passed = w == after;
            continue;
        }
        if (w->destroyed) continue;
        if (className && w->className != className) continue;
        if (windowName && w->title != windowName) continue;
        return w;
    }
    return nullptr;
}

ATOM RegisterClassW(const WNDCLASSW* wc) {
    if (!wc) return 0;
    WNDCLASSEXW ex{};
    ex.cbSize = sizeof ex;
    ex.style = wc->style;
    ex.lpfnWndProc = wc->lpfnWndProc;
    ex.hInstance = wc->hInstance;
    ex.hCursor = wc->hCursor;
    ex.lpszClassName = wc->lpszClassName;
    return RegisterClassExW(&ex);
}

BOOL UnregisterClassW(LPCWSTR className, HINSTANCE) {
    if (!className) return FALSE;
    std::lock_guard<std::mutex> lock(state().mutex);
    return state().classes.erase(className) ? TRUE : FALSE;
}

int GetWindowTextW(HWND hwnd, LPWSTR text, int maxCount) {
    if (!hwnd || !text || maxCount <= 0) return 0;
    int n = std::min(static_cast<int>(hwnd->title.size()), maxCount - 1);
    wmemcpy(text, hwnd->title.c_str(), n);
    text[n] = 0;
    return n;
}

DWORD MsgWaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL, DWORD milliseconds, DWORD wakeMask) {
    State& s = state();
    auto deadline = Clock::now() + std::chrono::milliseconds(milliseconds == INFINITE ? 0 : milliseconds);
    while (true) {
        for (DWORD i = 0; i < count; i++) {
            if (WaitForSingleObject(handles[i], 0) == WAIT_OBJECT_0) return WAIT_OBJECT_0 + i;
        }
        {
            std::unique_lock<std::mutex> lock(s.mutex);
            fireTimers(s);
            bool posted = !s.queue.empty();
            bool paint = (wakeMask & QS_PAINT) &&
                         std::any_of(s.windows.begin(), s.windows.end(), [](HWND w) { return w->dirty; });
            if (wakeMask && (posted || paint)) return WAIT_OBJECT_0 + count;
            if (milliseconds != INFINITE && Clock::now() >= deadline) return WAIT_TIMEOUT;
            s.wake.wait_for(lock, std::chrono::milliseconds(2));
        }
    }
}

int GetClassNameW(HWND hwnd, LPWSTR name, int maxCount) {
    if (!hwnd || !name || maxCount <= 0) return 0;
    int n = std::min(static_cast<int>(hwnd->className.size()), maxCount - 1);
    wmemcpy(name, hwnd->className.c_str(), n);
    name[n] = 0;
    return n;
}

BOOL ScreenToClient(HWND hwnd, POINT* p) {
    if (!hwnd || !p) return FALSE;
    p->x -= hwnd->rect.left;
    p->y -= hwnd->rect.top;
    return TRUE;
}

BOOL ClientToScreen(HWND hwnd, POINT* p) {
    if (!hwnd || !p) return FALSE;
    p->x += hwnd->rect.left;
    p->y += hwnd->rect.top;
    return TRUE;
}

BOOL GetCursorPos(POINT* point) {
    if (!point) return FALSE;
    *point = {0, 0};
    return TRUE;
}

HWND WindowFromPoint(POINT) { return nullptr; }

BOOL PtInRect(const RECT* r, POINT p) {
    return r && p.x >= r->left && p.x < r->right && p.y >= r->top && p.y < r->bottom;
}

HWND SetCapture(HWND hwnd) {
    HWND old = state().capture;
    state().capture = hwnd;
    return old;
}

BOOL ReleaseCapture() {
    state().capture = nullptr;
    return TRUE;
}

HWND GetCapture() { return state().capture; }

// Cursors are their resource ids; the window layer maps them to GDK names
HCURSOR LoadCursorW(HINSTANCE, LPCWSTR name) { return reinterpret_cast<HCURSOR>(const_cast<LPWSTR>(name)); }

HCURSOR SetCursor(HCURSOR cursor) {
    HCURSOR old = state().cursor;
    state().cursor = cursor;
    return old;
}

HICON LoadIconW(HINSTANCE, LPCWSTR) { return nullptr; }
HANDLE LoadImageW(HINSTANCE, LPCWSTR, UINT, int, int, UINT) { return nullptr; }
BOOL DestroyIcon(HICON) { return TRUE; }
BOOL TrackMouseEvent(TRACKMOUSEEVENT*) { return TRUE; }

// ---- Messages and timers ---------------------------------------------------------------------
UINT_PTR SetTimer(HWND hwnd, UINT_PTR id, UINT elapse, TIMERPROC) {
    State& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    auto interval = std::chrono::milliseconds(std::max<UINT>(elapse, 1));
    for (Timer& t : s.timers) {
        if (t.hwnd == hwnd && t.id == id) {
            t.interval = interval;
            t.due = Clock::now() + interval;
            s.wake.notify_all();
            return id;
        }
    }
    s.timers.push_back(Timer{hwnd, id, interval, Clock::now() + interval});
    s.wake.notify_all();
    return id;
}

BOOL KillTimer(HWND hwnd, UINT_PTR id) {
    State& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    auto before = s.timers.size();
    s.timers.erase(std::remove_if(s.timers.begin(), s.timers.end(),
                                  [&](const Timer& t) { return t.hwnd == hwnd && t.id == id; }),
                   s.timers.end());
    return s.timers.size() != before;
}

BOOL PostMessageW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    State& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.queue.push_back(MSG{hwnd, msg, wParam, lParam, 0, {0, 0}});
    s.wake.notify_all();
    return TRUE;
}

LRESULT SendMessageW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!hwnd || !hwnd->proc) return 0;
    return hwnd->proc(hwnd, msg, wParam, lParam);
}

void PostQuitMessage(int code) {
    State& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.quit = true;
    s.quitCode = code;
    s.wake.notify_all();
}

BOOL PeekMessageW(MSG* msg, HWND hwnd, UINT min, UINT max, UINT remove) {
    State& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    fireTimers(s);
    return takeMessage(s, msg, hwnd, min, max, (remove & PM_REMOVE) != 0) || takePaint(s, msg, hwnd, min, max);
}

BOOL GetMessageW(MSG* msg, HWND hwnd, UINT min, UINT max) {
    State& s = state();
    // A headless capture runs in place of the first wait for input
    static bool captured = false;
    if (!captured && !tinta::headless().renderPng.empty()) {
        captured = true;
        PostQuitMessage(runCapture());
    }
    std::unique_lock<std::mutex> lock(s.mutex);
    while (true) {
        Clock::time_point next = fireTimers(s);
        if (takeMessage(s, msg, hwnd, min, max, true)) return msg->message != WM_QUIT;
        if (!s.quit && takePaint(s, msg, hwnd, min, max)) return TRUE;
        if (s.quit) {
            *msg = MSG{nullptr, WM_QUIT, static_cast<WPARAM>(s.quitCode), 0, 0, {0, 0}};
            return FALSE;
        }
        if (next == Clock::time_point::max()) s.wake.wait(lock);
        else s.wake.wait_until(lock, next);
    }
}

BOOL TranslateMessage(const MSG*) { return FALSE; }

LRESULT DispatchMessageW(const MSG* msg) {
    if (!msg || !msg->hwnd || !msg->hwnd->proc) return 0;
    return msg->hwnd->proc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
}

// ---- Metrics, monitors, DPI ---------------------------------------------------------------
int GetSystemMetrics(int index) { return GetSystemMetricsForDpi(index, 96); }

int GetSystemMetricsForDpi(int index, UINT) {
    switch (index) {
        case SM_CXSCREEN:
        case SM_CXVIRTUALSCREEN: return 1920;
        case SM_CYSCREEN:
        case SM_CYVIRTUALSCREEN: return 1080;
        case SM_CXDRAG:
        case SM_CYDRAG: return 4;
        case SM_CXSIZEFRAME:
        case SM_CYSIZEFRAME: return 4;
        case SM_CXPADDEDBORDER: return 4;
        default: return 0;
    }
}

UINT GetDpiForWindow(HWND) { return tinta::headless().dpi; }
BOOL SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT) { return TRUE; }

HMONITOR MonitorFromWindow(HWND, DWORD) { return reinterpret_cast<HMONITOR>(1); }
HMONITOR MonitorFromRect(const RECT*, DWORD) { return reinterpret_cast<HMONITOR>(1); }

BOOL GetMonitorInfoW(HMONITOR, MONITORINFO* info) {
    if (!info) return FALSE;
    info->rcMonitor = {0, 0, 1920, 1080};
    info->rcWork = {0, 0, 1920, 1080};
    info->dwFlags = 1;
    return TRUE;
}

// ---- Menus, message boxes, DWM ------------------------------------------------------------
HMENU GetSystemMenu(HWND, BOOL) { return nullptr; }
HMENU CreatePopupMenu() { return nullptr; }
BOOL AppendMenuW(HMENU, UINT, UINT_PTR, LPCWSTR) { return TRUE; }
BOOL DestroyMenu(HMENU) { return TRUE; }
BOOL TrackPopupMenu(HMENU, UINT, int, int, int, HWND, const RECT*) { return FALSE; }

// Headless: report on stderr and take the safe answer (no, cancel)
int MessageBoxW(HWND, LPCWSTR text, LPCWSTR caption, UINT type) {
    std::fprintf(stderr, "[%ls] %ls\n", caption ? caption : L"Tinta", text ? text : L"");
    switch (type & 0xF) {
        case MB_YESNO: return IDNO;
        case MB_YESNOCANCEL:
        case MB_OKCANCEL: return IDCANCEL;
        default: return IDOK;
    }
}

BOOL MessageBeep(UINT) { return TRUE; }
HRESULT DwmSetWindowAttribute(HWND, DWORD, const void*, DWORD) { return S_OK; }
HRESULT DwmExtendFrameIntoClientArea(HWND, const void*) { return S_OK; }

// ---- GDI: only the tab drag ghost and print DCs use it ----------------------------------------
HDC GetDC(HWND) { return nullptr; }
int ReleaseDC(HWND, HDC) { return 1; }
HDC CreateCompatibleDC(HDC) { return nullptr; }
BOOL DeleteDC(HDC) { return TRUE; }
HGDIOBJ SelectObject(HDC, HGDIOBJ) { return nullptr; }
BOOL DeleteObject(HGDIOBJ) { return TRUE; }
HBITMAP CreateDIBSection(HDC, const BITMAPINFO*, UINT, void** bits, HANDLE, DWORD) {
    if (bits) *bits = nullptr;
    return nullptr;
}
HBRUSH CreateSolidBrush(COLORREF) { return nullptr; }
int SetBkMode(HDC, int) { return 0; }
COLORREF SetTextColor(HDC, COLORREF) { return 0; }
int DrawTextW(HDC, LPCWSTR, int, RECT*, UINT) { return 0; }
int GetDeviceCaps(HDC, int index) { return index == LOGPIXELSX || index == LOGPIXELSY ? 96 : 0; }
HRGN CreateRectRgn(int, int, int, int) { return nullptr; }
HRGN CreateRoundRectRgn(int, int, int, int, int, int) { return nullptr; }
BOOL FillRgn(HDC, HRGN, HBRUSH) { return TRUE; }
BOOL FrameRgn(HDC, HRGN, HBRUSH, int, int) { return TRUE; }
BOOL UpdateLayeredWindow(HWND, HDC, POINT*, SIZE*, HDC, POINT*, COLORREF, BLENDFUNCTION*, DWORD) { return FALSE; }
HFONT CreateFontW(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCWSTR) {
    return nullptr;
}

int MulDiv(int number, int numerator, int denominator) {
    if (denominator == 0) return -1;
    long long product = static_cast<long long>(number) * numerator;
    long long half = (product < 0) != (denominator < 0) ? -denominator / 2 : denominator / 2;
    return static_cast<int>((product + half) / denominator);
}

// ---- Clipboard: in-process until the window layer bridges GdkClipboard ----------------------
BOOL OpenClipboard(HWND hwnd) {
    std::lock_guard<std::mutex> lock(state().mutex);
    state().clipboardOpener = hwnd;
    return TRUE;
}

BOOL CloseClipboard() { return TRUE; }

// As on Windows, emptying makes the opener the owner
BOOL EmptyClipboard() {
    std::lock_guard<std::mutex> lock(state().mutex);
    for (auto& entry : state().clipboard) GlobalFree(entry.second);
    state().clipboard.clear();
    state().clipboardOwner = state().clipboardOpener;
    return TRUE;
}

HWND GetClipboardOwner() {
    std::lock_guard<std::mutex> lock(state().mutex);
    return state().clipboardOwner;
}

// Formats in ascending order; 0 ends the enumeration
UINT EnumClipboardFormats(UINT format) {
    std::lock_guard<std::mutex> lock(state().mutex);
    auto it = state().clipboard.upper_bound(format);
    return it == state().clipboard.end() ? 0 : it->first;
}

HANDLE OleDuplicateData(HANDLE source, CLIPFORMAT, UINT) {
    SIZE_T size = GlobalSize(source);
    HGLOBAL copy = GlobalAlloc(GMEM_MOVEABLE, size);
    void* from = GlobalLock(source);
    void* to = GlobalLock(copy);
    if (from && to && size) std::memcpy(to, from, size);
    if (from) GlobalUnlock(source);
    if (to) GlobalUnlock(copy);
    return copy;
}

BOOL DeleteMetaFile(HMETAFILE) { return TRUE; }
BOOL DeleteEnhMetaFile(HENHMETAFILE) { return TRUE; }

namespace {

// OleGetClipboard's snapshot: every format's bytes at the time of the call
class ClipboardSnapshot final : public TintaComObject<IDataObject> {
public:
    using Primary = IDataObject;
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IDataObject>(this, riid, object);
    }
    std::vector<std::pair<UINT, std::vector<unsigned char>>> formats;
};

}  // namespace

HRESULT OleInitialize(void*) { return S_OK; }
void OleUninitialize() {}

HRESULT OleGetClipboard(IDataObject** dataObject) {
    if (!dataObject) return E_POINTER;
    auto* snapshot = new ClipboardSnapshot();
    std::lock_guard<std::mutex> lock(state().mutex);
    for (auto& [format, memory] : state().clipboard) {
        SIZE_T size = GlobalSize(memory);
        auto* bytes = static_cast<unsigned char*>(GlobalLock(memory));
        snapshot->formats.push_back({format, std::vector<unsigned char>(bytes, bytes + (bytes ? size : 0))});
        if (bytes) GlobalUnlock(memory);
    }
    *dataObject = snapshot;
    return S_OK;
}

HRESULT OleSetClipboard(IDataObject* dataObject) {
    auto* snapshot = dynamic_cast<ClipboardSnapshot*>(dataObject);
    if (dataObject && !snapshot) return E_NOTIMPL;  // only snapshots of this clipboard
    OpenClipboard(nullptr);
    EmptyClipboard();
    if (snapshot) {
        for (const auto& [format, bytes] : snapshot->formats) {
            HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
            if (void* to = GlobalLock(memory)) {
                if (!bytes.empty()) std::memcpy(to, bytes.data(), bytes.size());
                GlobalUnlock(memory);
            }
            SetClipboardData(format, memory);
        }
    }
    CloseClipboard();
    return S_OK;
}

HRESULT OleFlushClipboard() { return S_OK; }

HANDLE SetClipboardData(UINT format, HANDLE memory) {
    std::lock_guard<std::mutex> lock(state().mutex);
    auto& slot = state().clipboard[format];
    if (slot && slot != memory) GlobalFree(slot);
    slot = memory;
    return memory;
}

HANDLE GetClipboardData(UINT format) {
    std::lock_guard<std::mutex> lock(state().mutex);
    auto it = state().clipboard.find(format);
    return it == state().clipboard.end() ? nullptr : it->second;
}

BOOL IsClipboardFormatAvailable(UINT format) {
    std::lock_guard<std::mutex> lock(state().mutex);
    return state().clipboard.count(format) ? TRUE : FALSE;
}

UINT RegisterClipboardFormatW(LPCWSTR name) {
    if (!name) return 0;
    std::lock_guard<std::mutex> lock(state().mutex);
    auto it = state().clipboardFormats.find(name);
    if (it != state().clipboardFormats.end()) return it->second;
    UINT id = state().nextClipboardFormat++;
    state().clipboardFormats[name] = id;
    return id;
}

UINT DragQueryFileW(HDROP, UINT, LPWSTR, UINT) { return 0; }
void DragFinish(HDROP) {}
void DragAcceptFiles(HWND, BOOL) {}

// ---- IME --------------------------------------------------------------------------------------
HIMC ImmGetContext(HWND) { return reinterpret_cast<HIMC>(1); }
BOOL ImmReleaseContext(HWND, HIMC) { return TRUE; }
BOOL ImmSetCompositionWindow(HIMC, COMPOSITIONFORM*) { return TRUE; }
BOOL ImmSetCandidateWindow(HIMC, CANDIDATEFORM*) { return TRUE; }
LONG ImmGetCompositionStringW(HIMC, DWORD, void*, DWORD) { return 0; }

// ---- Common dialogs: cancelled until the window layer provides portal dialogs ------------------
BOOL GetOpenFileNameW(OPENFILENAMEW*) { return FALSE; }
BOOL GetSaveFileNameW(OPENFILENAMEW*) { return FALSE; }
BOOL PrintDlgW(PRINTDLGW*) { return FALSE; }
