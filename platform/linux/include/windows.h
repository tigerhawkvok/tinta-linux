// Linux stand-in for the slice of <windows.h> that Tinta's shared code uses.
// It is only on the include path of the Linux build (platform/linux/linux.cmake);
// the Windows build never sees it. Grow it as shared code needs more, and keep
// Win32 semantics: callers were written against the real API.
//
// wchar_t is UTF-32 here, so "wide" text holds code points, not UTF-16 units.
#ifndef TINTA_LINUX_WINDOWS_H
#define TINTA_LINUX_WINDOWS_H

#include <cstddef>
#include <cstdint>

#include "msvc_crt.h"

// ---- Basic types --------------------------------------------------------
// DWORD is unsigned long (64-bit on LP64) rather than Wine's 32-bit choice:
// shared code prints DWORDs with %lu, and nothing serializes them by size.
typedef int BOOL;
typedef unsigned char BYTE;
typedef short SHORT;
typedef uint16_t WORD;
typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef long LONG;
typedef unsigned long ULONG;
typedef int64_t LONGLONG;
typedef uint64_t ULONGLONG;
typedef wchar_t WCHAR;
typedef char CHAR;
typedef const char* LPCSTR;
typedef char* LPSTR;
typedef const wchar_t* LPCWSTR;
typedef wchar_t* LPWSTR;
typedef void* LPVOID;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef int32_t HRESULT;
typedef int INT;
typedef int8_t INT8;
typedef int16_t INT16;
typedef int32_t INT32;
typedef int64_t INT64;
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef float FLOAT;
typedef intptr_t INT_PTR;
typedef uintptr_t UINT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef size_t SIZE_T;
typedef BOOL* LPBOOL;
typedef DWORD* LPDWORD;
typedef BYTE* LPBYTE;
typedef const void* LPCVOID;
typedef void* HANDLE;
typedef unsigned short USHORT;
typedef unsigned char UCHAR;
typedef DWORD COLORREF;
typedef WORD ATOM;

#define WINAPI
#define CALLBACK
#define APIENTRY
#define CONST const
#define VOID void

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define S_OK ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define E_FAIL ((HRESULT)0x80004005)
#define E_INVALIDARG ((HRESULT)0x80070057)
#define E_OUTOFMEMORY ((HRESULT)0x8007000E)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)

#define LOWORD(l) ((WORD)(((uintptr_t)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((uintptr_t)(l)) >> 16) & 0xffff))

// ---- Code pages and character types ---------------------------------------
#define CP_ACP 0
#define CP_UTF8 65001
#define MB_ERR_INVALID_CHARS 0x00000008
#define WC_ERR_INVALID_CHARS 0x00000080

// Only CP_UTF8 (and CP_ACP, treated as UTF-8) are supported. Invalid input
// decodes to U+FFFD unless MB_ERR_INVALID_CHARS is set, as on Windows.
int MultiByteToWideChar(UINT codePage, DWORD flags, const char* source,
                        int sourceLength, wchar_t* dest, int destLength);
int WideCharToMultiByte(UINT codePage, DWORD flags, const wchar_t* source,
                        int sourceLength, char* dest, int destLength,
                        const char* defaultChar, BOOL* usedDefaultChar);

#define CT_CTYPE1 0x00000001
#define C1_UPPER 0x0001
#define C1_LOWER 0x0002
#define C1_DIGIT 0x0004
#define C1_SPACE 0x0008
#define C1_PUNCT 0x0010
#define C1_CNTRL 0x0020
#define C1_BLANK 0x0040
#define C1_XDIGIT 0x0080
#define C1_ALPHA 0x0100

// CT_CTYPE1 only, classified with Unicode properties (GLib)
BOOL GetStringTypeW(DWORD infoType, const wchar_t* source, int length,
                    WORD* charTypes);

// ---- Time -----------------------------------------------------------------
struct SYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
};
typedef SYSTEMTIME* LPSYSTEMTIME;

// 100 ns ticks since 1601-01-01 UTC, split as on Windows
struct FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
};

BOOL SystemTimeToFileTime(const SYSTEMTIME* systemTime, FILETIME* fileTime);
BOOL FileTimeToSystemTime(const FILETIME* fileTime, SYSTEMTIME* systemTime);
void GetSystemTime(SYSTEMTIME* systemTime);
void GetLocalTime(SYSTEMTIME* systemTime);
void GetSystemTimeAsFileTime(FILETIME* fileTime);
ULONGLONG GetTickCount64();

// ---- Virtual-key codes ------------------------------------------------------
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_CANCEL 0x03
#define VK_MBUTTON 0x04
#define VK_XBUTTON1 0x05
#define VK_XBUTTON2 0x06
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_CLEAR 0x0C
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_PAUSE 0x13
#define VK_CAPITAL 0x14
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_SELECT 0x29
#define VK_PRINT 0x2A
#define VK_SNAPSHOT 0x2C
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_HELP 0x2F
#define VK_LWIN 0x5B
#define VK_RWIN 0x5C
#define VK_APPS 0x5D
#define VK_NUMPAD0 0x60
#define VK_NUMPAD1 0x61
#define VK_NUMPAD2 0x62
#define VK_NUMPAD3 0x63
#define VK_NUMPAD4 0x64
#define VK_NUMPAD5 0x65
#define VK_NUMPAD6 0x66
#define VK_NUMPAD7 0x67
#define VK_NUMPAD8 0x68
#define VK_NUMPAD9 0x69
#define VK_MULTIPLY 0x6A
#define VK_ADD 0x6B
#define VK_SEPARATOR 0x6C
#define VK_SUBTRACT 0x6D
#define VK_DECIMAL 0x6E
#define VK_DIVIDE 0x6F
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B
#define VK_NUMLOCK 0x90
#define VK_SCROLL 0x91
#define VK_LSHIFT 0xA0
#define VK_RSHIFT 0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define VK_LMENU 0xA4
#define VK_RMENU 0xA5
#define VK_BROWSER_BACK 0xA6
#define VK_BROWSER_FORWARD 0xA7
#define VK_OEM_1 0xBA
#define VK_OEM_PLUS 0xBB
#define VK_OEM_COMMA 0xBC
#define VK_OEM_MINUS 0xBD
#define VK_OEM_PERIOD 0xBE
#define VK_OEM_2 0xBF
#define VK_OEM_3 0xC0
#define VK_OEM_4 0xDB
#define VK_OEM_5 0xDC
#define VK_OEM_6 0xDD
#define VK_OEM_7 0xDE
#define VK_OEM_102 0xE2
#define VK_PROCESSKEY 0xE5

// High bit set while the key is down, low bit toggled per press, as on
// Windows. The window layer reports transitions through tintaSetKeyState.
SHORT GetKeyState(int virtualKey);
void tintaSetKeyState(int virtualKey, bool down);

// Linux's registerFileAssociation: a desktop entry, icons, the .mmd MIME
// type and Tinta as the default viewer (desktop_integration.cpp)
bool tintaRegisterDesktopApp();

// Windows, GDI, files, clipboard, dialogs, shell, registry
#include "tinta_win32.h"
// COM, OLE and the shell (no-op while tinta_com.h is including this file)
#include "tinta_com.h"

#endif  // TINTA_LINUX_WINDOWS_H
