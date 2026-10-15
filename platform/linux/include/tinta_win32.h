// Win32 windowing, GDI, files, clipboard, dialogs, shell and registry for the
// Linux compat layer; included from windows.h. Values match the Windows SDK.
// Implementations: platform/linux/src/win32_*.cpp (headless in phase 1; the
// GTK window layer backs the window functions from phase 2).
#ifndef TINTA_LINUX_WIN32_H
#define TINTA_LINUX_WIN32_H

// ---- Handles ----------------------------------------------------------------
#define TINTA_DECLARE_HANDLE(name) \
    struct name##__;               \
    typedef name##__* name

TINTA_DECLARE_HANDLE(HWND);
TINTA_DECLARE_HANDLE(HINSTANCE);
TINTA_DECLARE_HANDLE(HICON);
TINTA_DECLARE_HANDLE(HBRUSH);
TINTA_DECLARE_HANDLE(HDC);
TINTA_DECLARE_HANDLE(HBITMAP);
TINTA_DECLARE_HANDLE(HFONT);
TINTA_DECLARE_HANDLE(HRGN);
TINTA_DECLARE_HANDLE(HKEY);
TINTA_DECLARE_HANDLE(HDROP);
TINTA_DECLARE_HANDLE(HIMC);
TINTA_DECLARE_HANDLE(HMONITOR);
TINTA_DECLARE_HANDLE(HMENU);
TINTA_DECLARE_HANDLE(HPEN);
TINTA_DECLARE_HANDLE(HDESK);
TINTA_DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);
typedef HICON HCURSOR;
typedef HINSTANCE HMODULE;
typedef void* HGDIOBJ;
typedef void* HGLOBAL;
typedef void* HLOCAL;

#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define HWND_TOP ((HWND)0)
#define HWND_BOTTOM ((HWND)1)
#define HWND_TOPMOST ((HWND)(intptr_t)-1)
#define HWND_NOTOPMOST ((HWND)(intptr_t)-2)
#define HWND_MESSAGE ((HWND)(intptr_t)-3)
#define HKEY_CURRENT_USER ((HKEY)(uintptr_t)0x80000001)
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)(intptr_t)-4)

// ---- Geometry and messages ------------------------------------------------------
struct POINT { LONG x; LONG y; };
struct POINTS { SHORT x; SHORT y; };
struct RECT { LONG left; LONG top; LONG right; LONG bottom; };
struct SIZE { LONG cx; LONG cy; };
typedef POINT* LPPOINT;
typedef RECT* LPRECT;
typedef const RECT* LPCRECT;

struct MSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
};

typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef void (*TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);

struct WNDCLASSEXW {
    UINT cbSize;
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCWSTR lpszMenuName;
    LPCWSTR lpszClassName;
    HICON hIconSm;
};

struct WINDOWPLACEMENT {
    UINT length;
    UINT flags;
    UINT showCmd;
    POINT ptMinPosition;
    POINT ptMaxPosition;
    RECT rcNormalPosition;
};

struct COPYDATASTRUCT {
    ULONG_PTR dwData;
    DWORD cbData;
    void* lpData;
};

struct TRACKMOUSEEVENT {
    DWORD cbSize;
    DWORD dwFlags;
    HWND hwndTrack;
    DWORD dwHoverTime;
};

struct MONITORINFO {
    DWORD cbSize;
    RECT rcMonitor;
    RECT rcWork;
    DWORD dwFlags;
};

struct NCCALCSIZE_PARAMS {
    RECT rgrc[3];
    void* lppos;
};

struct CREATESTRUCTW {
    void* lpCreateParams;
    HINSTANCE hInstance;
    HMENU hMenu;
    HWND hwndParent;
    int cy;
    int cx;
    int y;
    int x;
    LONG style;
    LPCWSTR lpszName;
    LPCWSTR lpszClass;
    DWORD dwExStyle;
};

#define MAKELPARAM(l, h) ((LPARAM)(DWORD)(((WORD)((DWORD_PTR)(l) & 0xffff)) | ((DWORD)((WORD)((DWORD_PTR)(h) & 0xffff))) << 16))
#define MAKEWPARAM(l, h) ((WPARAM)(DWORD)(((WORD)((DWORD_PTR)(l) & 0xffff)) | ((DWORD)((WORD)((DWORD_PTR)(h) & 0xffff))) << 16))
#define MAKELONG(a, b) ((LONG)(((WORD)((DWORD_PTR)(a) & 0xffff)) | ((DWORD)((WORD)((DWORD_PTR)(b) & 0xffff))) << 16))
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#define GET_WHEEL_DELTA_WPARAM(wParam) ((short)HIWORD(wParam))
#define GET_KEYSTATE_WPARAM(wParam) (LOWORD(wParam))
#define GET_XBUTTON_WPARAM(wParam) (HIWORD(wParam))
#define WHEEL_DELTA 120
#define XBUTTON1 0x0001
#define XBUTTON2 0x0002
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(rgb) ((BYTE)(rgb))
#define GetGValue(rgb) ((BYTE)(((WORD)(rgb)) >> 8))
#define GetBValue(rgb) ((BYTE)((rgb) >> 16))
#define MAKEINTRESOURCEW(i) ((LPWSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCE MAKEINTRESOURCEW
#define MAX_PATH 260
#define CW_USEDEFAULT ((int)0x80000000)
#define INFINITE 0xFFFFFFFF
#define WAIT_OBJECT_0 0x00000000L
#define WAIT_TIMEOUT 258L

// Window messages
#define WM_NULL 0x0000
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_NCDESTROY 0x0082
#define SIZE_RESTORED 0
#define SIZE_MINIMIZED 1
#define SIZE_MAXIMIZED 2
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_ACTIVATE 0x0006
#define WM_SETFOCUS 0x0007
#define WM_KILLFOCUS 0x0008
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ERASEBKGND 0x0014
#define WM_SETTINGCHANGE 0x001A
#define WM_CANCELMODE 0x001F
#define WM_SETCURSOR 0x0020
#define WM_GETMINMAXINFO 0x0024
#define WM_COPYDATA 0x004A
#define WM_CONTEXTMENU 0x007B
#define WM_NCCREATE 0x0081
#define WM_NCCALCSIZE 0x0083
#define WM_NCHITTEST 0x0084
#define WM_NCPAINT 0x0085
#define WM_NCACTIVATE 0x0086
#define WM_NCMOUSEMOVE 0x00A0
#define WM_NCLBUTTONDOWN 0x00A1
#define WM_NCLBUTTONUP 0x00A2
#define WM_NCLBUTTONDBLCLK 0x00A3
#define WM_NCRBUTTONDOWN 0x00A4
#define WM_NCRBUTTONUP 0x00A5
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_SYSCHAR 0x0106
#define WM_IME_STARTCOMPOSITION 0x010D
#define WM_IME_ENDCOMPOSITION 0x010E
#define WM_IME_COMPOSITION 0x010F
#define WM_SYSCOMMAND 0x0112
#define WM_TIMER 0x0113
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_LBUTTONDBLCLK 0x0203
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_MOUSEWHEEL 0x020A
#define WM_XBUTTONDOWN 0x020B
#define WM_XBUTTONUP 0x020C
#define WM_MOUSEHWHEEL 0x020E
#define WM_CAPTURECHANGED 0x0215
#define WM_ENTERSIZEMOVE 0x0231
#define WM_EXITSIZEMOVE 0x0232
#define WM_DROPFILES 0x0233
#define WM_NCMOUSELEAVE 0x02A2
#define WM_MOUSELEAVE 0x02A3
#define WM_DPICHANGED 0x02E0
#define WM_USER 0x0400
#define WM_APP 0x8000

#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002
#define MK_SHIFT 0x0004
#define MK_CONTROL 0x0008
#define MK_MBUTTON 0x0010

// Hit-test results
#define HTERROR (-2)
#define HTTRANSPARENT (-1)
#define HTNOWHERE 0
#define HTCLIENT 1
#define HTCAPTION 2
#define HTSYSMENU 3
#define HTMINBUTTON 8
#define HTMAXBUTTON 9
#define HTLEFT 10
#define HTRIGHT 11
#define HTTOP 12
#define HTTOPLEFT 13
#define HTTOPRIGHT 14
#define HTBOTTOM 15
#define HTBOTTOMLEFT 16
#define HTBOTTOMRIGHT 17
#define HTCLOSE 20

// Show, position, style
#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_NORMAL 1
#define SW_SHOWMINIMIZED 2
#define SW_SHOWMAXIMIZED 3
#define SW_MAXIMIZE 3
#define SW_SHOWNOACTIVATE 4
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_RESTORE 9
#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004
#define SWP_NOACTIVATE 0x0010
#define SWP_FRAMECHANGED 0x0020
#define SWP_SHOWWINDOW 0x0040
#define SWP_NOSENDCHANGING 0x0400
#define WS_OVERLAPPED 0x00000000L
#define WS_POPUP 0x80000000L
#define WS_VISIBLE 0x10000000L
#define WS_CAPTION 0x00C00000L
#define WS_SYSMENU 0x00080000L
#define WS_THICKFRAME 0x00040000L
#define WS_MINIMIZEBOX 0x00020000L
#define WS_MAXIMIZEBOX 0x00010000L
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_EX_TOPMOST 0x00000008L
#define WS_EX_ACCEPTFILES 0x00000010L
#define WS_EX_TRANSPARENT 0x00000020L
#define WS_EX_TOOLWINDOW 0x00000080L
#define WS_EX_LAYERED 0x00080000L
#define WS_EX_NOACTIVATE 0x08000000L
#define CS_VREDRAW 0x0001
#define CS_HREDRAW 0x0002
#define GWL_STYLE (-16)
#define GWL_EXSTYLE (-20)
#define GWLP_USERDATA (-21)
#define GA_ROOT 2

#define SC_SIZE 0xF000
#define SC_MOVE 0xF010
#define SC_MINIMIZE 0xF020
#define SC_MAXIMIZE 0xF030
#define SC_CLOSE 0xF060
#define SC_KEYMENU 0xF100
#define SC_RESTORE 0xF120

#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SM_CXDRAG 68
#define SM_CYDRAG 69
#define SM_XVIRTUALSCREEN 76
#define SM_YVIRTUALSCREEN 77
#define SM_CXVIRTUALSCREEN 78
#define SM_CYVIRTUALSCREEN 79
#define SM_CYSIZEFRAME 33
#define SM_CXSIZEFRAME 32
#define SM_CXPADDEDBORDER 92

#define TME_LEAVE 0x00000002
#define TME_NONCLIENT 0x00000010
#define TPM_RIGHTBUTTON 0x0002
#define TPM_RETURNCMD 0x0100
#define MF_STRING 0x00000000L
#define MF_SEPARATOR 0x00000800L
#define MF_GRAYED 0x00000001L
#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001
#define MONITOR_DEFAULTTONULL 0x00000000
#define MONITOR_DEFAULTTOPRIMARY 0x00000001
#define MONITOR_DEFAULTTONEAREST 0x00000002
#define ULW_ALPHA 0x00000002
#define AC_SRC_OVER 0x00
#define AC_SRC_ALPHA 0x01
#define IMAGE_ICON 1
#define LR_DEFAULTCOLOR 0x00000000
#define LR_SHARED 0x00008000

#define MB_OK 0x00000000L
#define MB_OKCANCEL 0x00000001L
#define MB_YESNOCANCEL 0x00000003L
#define MB_YESNO 0x00000004L
#define MB_ICONERROR 0x00000010L
#define MB_ICONQUESTION 0x00000020L
#define MB_ICONWARNING 0x00000030L
#define MB_ICONINFORMATION 0x00000040L
#define IDOK 1
#define IDCANCEL 2
#define IDYES 6
#define IDNO 7

// Cursors (IDC_*) are resource ids
#define IDC_ARROW MAKEINTRESOURCEW(32512)
#define IDC_IBEAM MAKEINTRESOURCEW(32513)
#define IDC_WAIT MAKEINTRESOURCEW(32514)
#define IDC_SIZENWSE MAKEINTRESOURCEW(32642)
#define IDC_SIZENESW MAKEINTRESOURCEW(32643)
#define IDC_SIZEWE MAKEINTRESOURCEW(32644)
#define IDC_SIZENS MAKEINTRESOURCEW(32645)
#define IDC_SIZEALL MAKEINTRESOURCEW(32646)
#define IDC_HAND MAKEINTRESOURCEW(32649)

// DWM
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#define DWMWA_BORDER_COLOR 34
#define DWMWA_CAPTION_COLOR 35
#define DWMWA_TEXT_COLOR 36

// ---- Window functions ----------------------------------------------------------
LRESULT DefWindowProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
ATOM RegisterClassExW(const WNDCLASSEXW* wc);
HWND CreateWindowExW(DWORD exStyle, LPCWSTR className, LPCWSTR windowName, DWORD style, int x, int y,
                     int width, int height, HWND parent, HMENU menu, HINSTANCE instance, void* param);
BOOL DestroyWindow(HWND hwnd);
BOOL ShowWindow(HWND hwnd, int cmdShow);
BOOL UpdateWindow(HWND hwnd);
BOOL InvalidateRect(HWND hwnd, const RECT* rect, BOOL erase);
BOOL SetWindowPos(HWND hwnd, HWND insertAfter, int x, int y, int cx, int cy, UINT flags);
BOOL GetWindowRect(HWND hwnd, RECT* rect);
BOOL GetClientRect(HWND hwnd, RECT* rect);
BOOL SetWindowTextW(HWND hwnd, LPCWSTR text);
BOOL GetWindowPlacement(HWND hwnd, WINDOWPLACEMENT* placement);
BOOL SetWindowPlacement(HWND hwnd, const WINDOWPLACEMENT* placement);
LONG GetWindowLongW(HWND hwnd, int index);
LONG SetWindowLongW(HWND hwnd, int index, LONG value);
LONG_PTR GetWindowLongPtrW(HWND hwnd, int index);
LONG_PTR SetWindowLongPtrW(HWND hwnd, int index, LONG_PTR value);
BOOL IsZoomed(HWND hwnd);
BOOL IsIconic(HWND hwnd);
BOOL IsWindowVisible(HWND hwnd);
BOOL IsWindow(HWND hwnd);
BOOL SetForegroundWindow(HWND hwnd);
HWND GetForegroundWindow();
HWND SetFocus(HWND hwnd);
HWND GetFocus();
HWND GetAncestor(HWND hwnd, UINT flags);
HWND FindWindowExW(HWND parent, HWND childAfter, LPCWSTR className, LPCWSTR windowName);
inline HWND FindWindowW(LPCWSTR className, LPCWSTR windowName) {
    return FindWindowExW(nullptr, nullptr, className, windowName);
}
struct PAINTSTRUCT {
    HDC hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    BYTE rgbReserved[32];
};
HDC BeginPaint(HWND hwnd, PAINTSTRUCT* paint);
BOOL EndPaint(HWND hwnd, const PAINTSTRUCT* paint);
int GetClassNameW(HWND hwnd, LPWSTR className, int maxCount);
BOOL ScreenToClient(HWND hwnd, POINT* point);
BOOL ClientToScreen(HWND hwnd, POINT* point);
BOOL GetCursorPos(POINT* point);
BOOL PtInRect(const RECT* rect, POINT point);
HWND SetCapture(HWND hwnd);
BOOL ReleaseCapture();
HWND GetCapture();
HCURSOR SetCursor(HCURSOR cursor);
HCURSOR LoadCursorW(HINSTANCE instance, LPCWSTR name);
#define LoadCursor LoadCursorW
HICON LoadIconW(HINSTANCE instance, LPCWSTR name);
HANDLE LoadImageW(HINSTANCE instance, LPCWSTR name, UINT type, int cx, int cy, UINT flags);
BOOL DestroyIcon(HICON icon);
BOOL TrackMouseEvent(TRACKMOUSEEVENT* eventTrack);
UINT_PTR SetTimer(HWND hwnd, UINT_PTR id, UINT elapse, TIMERPROC timerProc);
BOOL KillTimer(HWND hwnd, UINT_PTR id);
BOOL PostMessageW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#define PostMessage PostMessageW
LRESULT SendMessageW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#define SendMessage SendMessageW
BOOL PeekMessageW(MSG* msg, HWND hwnd, UINT filterMin, UINT filterMax, UINT removeMsg);
BOOL GetMessageW(MSG* msg, HWND hwnd, UINT filterMin, UINT filterMax);
BOOL TranslateMessage(const MSG* msg);
LRESULT DispatchMessageW(const MSG* msg);
#define GetMessage GetMessageW
#define PeekMessage PeekMessageW
#define DispatchMessage DispatchMessageW
void PostQuitMessage(int exitCode);
int GetSystemMetrics(int index);
int GetSystemMetricsForDpi(int index, UINT dpi);
UINT GetDpiForWindow(HWND hwnd);
BOOL SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT value);
HMONITOR MonitorFromWindow(HWND hwnd, DWORD flags);
HMONITOR MonitorFromRect(const RECT* rect, DWORD flags);
BOOL GetMonitorInfoW(HMONITOR monitor, MONITORINFO* info);
#define GetMonitorInfo GetMonitorInfoW
HMENU GetSystemMenu(HWND hwnd, BOOL revert);
HMENU CreatePopupMenu();
BOOL AppendMenuW(HMENU menu, UINT flags, UINT_PTR id, LPCWSTR text);
BOOL DestroyMenu(HMENU menu);
BOOL TrackPopupMenu(HMENU menu, UINT flags, int x, int y, int reserved, HWND hwnd, const RECT* rect);
int MessageBoxW(HWND hwnd, LPCWSTR text, LPCWSTR caption, UINT type);
#define MessageBox MessageBoxW
HRESULT DwmSetWindowAttribute(HWND hwnd, DWORD attribute, const void* value, DWORD size);
HRESULT DwmExtendFrameIntoClientArea(HWND hwnd, const void* margins);
BOOL MessageBeep(UINT type);

// ---- GDI (layered windows, print DCs) -------------------------------------------
struct BLENDFUNCTION {
    BYTE BlendOp;
    BYTE BlendFlags;
    BYTE SourceConstantAlpha;
    BYTE AlphaFormat;
};
struct BITMAPINFOHEADER {
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
};
struct RGBQUAD { BYTE rgbBlue; BYTE rgbGreen; BYTE rgbRed; BYTE rgbReserved; };
struct BITMAPINFO {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[1];
};
#define BI_RGB 0L
#define DIB_RGB_COLORS 0
#define TRANSPARENT 1
#define LOGPIXELSX 88
#define LOGPIXELSY 90
#define PHYSICALWIDTH 110
#define PHYSICALHEIGHT 111
#define HORZRES 8
#define VERTRES 10
#define FW_NORMAL 400
#define DT_VCENTER 0x00000004
#define DT_SINGLELINE 0x00000020
#define DT_NOPREFIX 0x00000800
#define DT_END_ELLIPSIS 0x00008000
HDC GetDC(HWND hwnd);
int ReleaseDC(HWND hwnd, HDC dc);
HDC CreateCompatibleDC(HDC dc);
BOOL DeleteDC(HDC dc);
HGDIOBJ SelectObject(HDC dc, HGDIOBJ object);
BOOL DeleteObject(HGDIOBJ object);
HBITMAP CreateDIBSection(HDC dc, const BITMAPINFO* info, UINT usage, void** bits, HANDLE section, DWORD offset);
HBRUSH CreateSolidBrush(COLORREF color);
int SetBkMode(HDC dc, int mode);
COLORREF SetTextColor(HDC dc, COLORREF color);
int DrawTextW(HDC dc, LPCWSTR text, int length, RECT* rect, UINT format);
int GetDeviceCaps(HDC dc, int index);
HRGN CreateRectRgn(int left, int top, int right, int bottom);
BOOL FillRgn(HDC dc, HRGN region, HBRUSH brush);
BOOL FrameRgn(HDC dc, HRGN region, HBRUSH brush, int width, int height);
BOOL UpdateLayeredWindow(HWND hwnd, HDC dst, POINT* dstPoint, SIZE* size, HDC src, POINT* srcPoint,
                         COLORREF key, BLENDFUNCTION* blend, DWORD flags);
HFONT CreateFontW(int height, int width, int escapement, int orientation, int weight, DWORD italic,
                  DWORD underline, DWORD strikeOut, DWORD charSet, DWORD outPrecision, DWORD clipPrecision,
                  DWORD quality, DWORD pitchAndFamily, LPCWSTR face);
#define DEFAULT_CHARSET 1
#define OUT_DEFAULT_PRECIS 0
#define CLIP_DEFAULT_PRECIS 0
#define CLEARTYPE_QUALITY 5
#define DEFAULT_PITCH 0
HRGN CreateRoundRectRgn(int left, int top, int right, int bottom, int widthEllipse, int heightEllipse);
HWND WindowFromPoint(POINT point);
int MulDiv(int number, int numerator, int denominator);

// ---- Memory, clipboard, drag and drop ---------------------------------------------
#define GMEM_MOVEABLE 0x0002
#define GMEM_ZEROINIT 0x0040
#define GHND (GMEM_MOVEABLE | GMEM_ZEROINIT)
HGLOBAL GlobalAlloc(UINT flags, SIZE_T bytes);
void* GlobalLock(HGLOBAL memory);
BOOL GlobalUnlock(HGLOBAL memory);
HGLOBAL GlobalFree(HGLOBAL memory);
SIZE_T GlobalSize(HGLOBAL memory);
HLOCAL LocalFree(HLOCAL memory);

#define CF_TEXT 1
#define CF_BITMAP 2
#define CF_DIB 8
#define CF_UNICODETEXT 13
#define CF_HDROP 15
BOOL OpenClipboard(HWND owner);
BOOL CloseClipboard();
BOOL EmptyClipboard();
HANDLE SetClipboardData(UINT format, HANDLE memory);
HANDLE GetClipboardData(UINT format);
BOOL IsClipboardFormatAvailable(UINT format);
UINT RegisterClipboardFormatW(LPCWSTR name);
UINT DragQueryFileW(HDROP drop, UINT index, LPWSTR file, UINT size);
void DragFinish(HDROP drop);
void DragAcceptFiles(HWND hwnd, BOOL accept);

// ---- IME ------------------------------------------------------------------------------
struct COMPOSITIONFORM { DWORD dwStyle; POINT ptCurrentPos; RECT rcArea; };
struct CANDIDATEFORM { DWORD dwIndex; DWORD dwStyle; POINT ptCurrentPos; RECT rcArea; };
#define CFS_POINT 0x0002
#define CFS_CANDIDATEPOS 0x0040
#define GCS_COMPSTR 0x0008
#define GCS_RESULTSTR 0x0800
HIMC ImmGetContext(HWND hwnd);
BOOL ImmReleaseContext(HWND hwnd, HIMC imc);
BOOL ImmSetCompositionWindow(HIMC imc, COMPOSITIONFORM* form);
BOOL ImmSetCandidateWindow(HIMC imc, CANDIDATEFORM* form);
LONG ImmGetCompositionStringW(HIMC imc, DWORD index, void* buffer, DWORD length);

// ---- Files and processes -------------------------------------------------------------
// Paths are wide strings in Windows syntax; implementations accept '\' or '/'
#define GENERIC_READ 0x80000000L
#define GENERIC_WRITE 0x40000000L
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define FILE_SHARE_DELETE 0x00000004
#define FILE_WRITE_ATTRIBUTES 0x0100
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define FILE_ATTRIBUTE_READONLY 0x00000001
#define FILE_ATTRIBUTE_HIDDEN 0x00000002
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define FILE_FLAG_BACKUP_SEMANTICS 0x02000000
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define VOLUME_NAME_DOS 0x0
#define FILE_NAME_NORMALIZED 0x0
#define ERROR_SUCCESS 0L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_MORE_DATA 234L
#define ERROR_PATH_NOT_FOUND 3L
#define ERROR_ACCESS_DENIED 5L
#define ERROR_ALREADY_EXISTS 183L
#define ERROR_NO_ASSOCIATION 1155L
#define CREATE_NO_WINDOW 0x08000000
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#define STARTF_USESHOWWINDOW 0x00000001
#define STARTF_USESTDHANDLES 0x00000100

struct SECURITY_ATTRIBUTES {
    DWORD nLength;
    void* lpSecurityDescriptor;
    BOOL bInheritHandle;
};
struct WIN32_FIND_DATAW {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    WCHAR cFileName[MAX_PATH];
    WCHAR cAlternateFileName[14];
};
struct STARTUPINFOW {
    DWORD cb;
    LPWSTR lpReserved;
    LPWSTR lpDesktop;
    LPWSTR lpTitle;
    DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
    WORD wShowWindow;
    WORD cbReserved2;
    BYTE* lpReserved2;
    HANDLE hStdInput, hStdOutput, hStdError;
};
struct PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD dwProcessId;
    DWORD dwThreadId;
};

DWORD GetLastError();
void SetLastError(DWORD error);
BOOL CloseHandle(HANDLE handle);
HANDLE CreateFileW(LPCWSTR path, DWORD access, DWORD share, SECURITY_ATTRIBUTES* security,
                   DWORD disposition, DWORD flags, HANDLE templateFile);
BOOL ReadFile(HANDLE file, void* buffer, DWORD toRead, DWORD* read, void* overlapped);
BOOL WriteFile(HANDLE file, const void* buffer, DWORD toWrite, DWORD* written, void* overlapped);
DWORD GetFileAttributesW(LPCWSTR path);
BOOL CreateDirectoryW(LPCWSTR path, SECURITY_ATTRIBUTES* security);
BOOL DeleteFileW(LPCWSTR path);
BOOL CopyFileW(LPCWSTR from, LPCWSTR to, BOOL failIfExists);
BOOL MoveFileExW(LPCWSTR from, LPCWSTR to, DWORD flags);
#define MOVEFILE_REPLACE_EXISTING 0x00000001
HANDLE FindFirstFileW(LPCWSTR pattern, WIN32_FIND_DATAW* data);
BOOL FindNextFileW(HANDLE find, WIN32_FIND_DATAW* data);
BOOL FindClose(HANDLE find);
DWORD GetFullPathNameW(LPCWSTR path, DWORD size, LPWSTR buffer, LPWSTR* filePart);
DWORD GetFinalPathNameByHandleW(HANDLE file, LPWSTR path, DWORD size, DWORD flags);
BOOL GetFileTime(HANDLE file, FILETIME* creation, FILETIME* access, FILETIME* write);
LONG CompareFileTime(const FILETIME* a, const FILETIME* b);
BOOL FileTimeToLocalFileTime(const FILETIME* utc, FILETIME* local);
DWORD GetModuleFileNameW(HMODULE module, LPWSTR path, DWORD size);
HMODULE GetModuleHandleW(LPCWSTR name);
DWORD GetCurrentDirectoryW(DWORD size, LPWSTR buffer);
BOOL SetCurrentDirectoryW(LPCWSTR path);
DWORD GetTempPathW(DWORD size, LPWSTR buffer);
UINT GetSystemDirectoryW(LPWSTR buffer, UINT size);
DWORD GetEnvironmentVariableW(LPCWSTR name, LPWSTR buffer, DWORD size);
DWORD ExpandEnvironmentStringsW(LPCWSTR source, LPWSTR dest, DWORD size);
DWORD SearchPathW(LPCWSTR path, LPCWSTR fileName, LPCWSTR extension, DWORD size, LPWSTR buffer, LPWSTR* filePart);
DWORD GetCurrentProcessId();
HANDLE OpenProcess(DWORD access, BOOL inherit, DWORD processId);
BOOL QueryFullProcessImageNameW(HANDLE process, DWORD flags, LPWSTR name, DWORD* size);
BOOL CreateProcessW(LPCWSTR application, LPWSTR commandLine, SECURITY_ATTRIBUTES* processAttributes,
                    SECURITY_ATTRIBUTES* threadAttributes, BOOL inheritHandles, DWORD flags, void* environment,
                    LPCWSTR currentDirectory, STARTUPINFOW* startup, PROCESS_INFORMATION* info);
BOOL GetExitCodeProcess(HANDLE process, DWORD* exitCode);
BOOL TerminateProcess(HANDLE process, UINT exitCode);
typedef DWORD (*LPTHREAD_START_ROUTINE)(void* parameter);
HANDLE CreateThread(SECURITY_ATTRIBUTES* security, SIZE_T stackSize, LPTHREAD_START_ROUTINE start,
                    void* parameter, DWORD flags, DWORD* threadId);
DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds);
void Sleep(DWORD milliseconds);
void OutputDebugStringW(LPCWSTR text);
LPWSTR GetCommandLineW();
LPWSTR* CommandLineToArgvW(LPCWSTR commandLine, int* count);

// ---- Locale ------------------------------------------------------------------------------
#define LOCALE_NAME_USER_DEFAULT nullptr
#define LOCALE_NAME_MAX_LENGTH 85
#define LOCALE_IMEASURE 0x0000000D
#define LOCALE_RETURN_NUMBER 0x20000000
#define MUI_LANGUAGE_NAME 0x8
#define LANG_JAPANESE 0x11
#define LANG_KOREAN 0x12
#define LANG_CHINESE 0x04
#define PRIMARYLANGID(lgid) ((WORD)(lgid) & 0x3ff)
#define DATE_SHORTDATE 0x00000001
#define DATE_LONGDATE 0x00000002
typedef WORD LANGID;
LANGID GetUserDefaultUILanguage();
BOOL GetUserPreferredUILanguages(DWORD flags, ULONG* count, LPWSTR buffer, ULONG* size);
int GetLocaleInfoEx(LPCWSTR locale, DWORD type, LPWSTR data, int size);
int GetUserDefaultLocaleName(LPWSTR name, int size);
int GetDateFormatEx(LPCWSTR locale, DWORD flags, const SYSTEMTIME* date, LPCWSTR format, LPWSTR buffer,
                    int size, LPCWSTR calendar);

// UTF-16 surrogate tests: never true for wchar_t text on Linux (UTF-32)
#define IS_HIGH_SURROGATE(wch) (((wch) >= 0xD800) && ((wch) <= 0xDBFF))
#define IS_LOW_SURROGATE(wch) (((wch) >= 0xDC00) && ((wch) <= 0xDFFF))
#define IS_SURROGATE_PAIR(hs, ls) (IS_HIGH_SURROGATE(hs) && IS_LOW_SURROGATE(ls))

// ---- Used by upstream's native tests (tests/*.cpp) ------------------------------------
struct WNDCLASSW {
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCWSTR lpszMenuName;
    LPCWSTR lpszClassName;
};
ATOM RegisterClassW(const WNDCLASSW* windowClass);
BOOL UnregisterClassW(LPCWSTR className, HINSTANCE instance);
int GetWindowTextW(HWND hwnd, LPWSTR text, int maxCount);
BOOL GetKeyboardState(BYTE* keyState);
BOOL SetKeyboardState(const BYTE* keyState);
#ifndef VK_F13
#define VK_F13 0x7C
#define VK_F14 0x7D
#define VK_F15 0x7E
#define VK_F16 0x7F
#define VK_F17 0x80
#define VK_F18 0x81
#define VK_F19 0x82
#define VK_F20 0x83
#define VK_F21 0x84
#define VK_F22 0x85
#define VK_F23 0x86
#define VK_F24 0x87
#endif

#define SEM_FAILCRITICALERRORS 0x0001
#define SEM_NOGPFAULTERRORBOX 0x0002
UINT SetErrorMode(UINT mode);

#define QS_KEY 0x0001
#define QS_MOUSEMOVE 0x0002
#define QS_MOUSEBUTTON 0x0004
#define QS_POSTMESSAGE 0x0008
#define QS_TIMER 0x0010
#define QS_PAINT 0x0020
#define QS_SENDMESSAGE 0x0040
#define QS_ALLINPUT 0x04FF
// Waits for one of `handles` or, with a wake mask, for a posted message
// (returns WAIT_OBJECT_0 + count)
DWORD MsgWaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL waitAll, DWORD milliseconds,
                                DWORD wakeMask);

#pragma pack(push, 2)
struct BITMAPFILEHEADER {
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
};
#pragma pack(pop)

BOOL RemoveDirectoryW(LPCWSTR path);
BOOL MoveFileW(LPCWSTR from, LPCWSTR to);
BOOL SetFileTime(HANDLE file, const FILETIME* creation, const FILETIME* access, const FILETIME* write);
// std::filesystem::path::c_str() is a UTF-8 char string here; Windows code
// hands it to CreateFileW as a wide one
HANDLE CreateFileW(const char* path, DWORD access, DWORD share, SECURITY_ATTRIBUTES* security, DWORD disposition,
                   DWORD flags, HANDLE templateFile);

inline LONG InterlockedIncrement(volatile LONG* value) { return __atomic_add_fetch(value, 1, __ATOMIC_SEQ_CST); }
inline LONG InterlockedDecrement(volatile LONG* value) { return __atomic_sub_fetch(value, 1, __ATOMIC_SEQ_CST); }

// Clipboard formats that only exist as GDI/metafile handles: never present here
typedef WORD CLIPFORMAT;
typedef void* HMETAFILE;
typedef void* HENHMETAFILE;
#define CF_METAFILEPICT 3
#define CF_PALETTE 9
#define CF_ENHMETAFILE 14
#define CF_OWNERDISPLAY 0x0080
#define CF_DSPTEXT 0x0081
#define CF_DSPBITMAP 0x0082
#define CF_DSPMETAFILEPICT 0x0083
#define CF_DSPENHMETAFILE 0x008E
struct METAFILEPICT {
    LONG mm;
    LONG xExt;
    LONG yExt;
    HMETAFILE hMF;
};
BOOL DeleteMetaFile(HMETAFILE metafile);
BOOL DeleteEnhMetaFile(HENHMETAFILE metafile);
UINT EnumClipboardFormats(UINT format);
HWND GetClipboardOwner();

#endif  // TINTA_LINUX_WIN32_H
