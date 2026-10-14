// APIs only the Windows entry point (main_d2d.cpp) uses: the D3D11 GPU
// warm-up, the WinHTTP update check and minidump crash reports. On Linux
// each reports failure, so the code around it takes its "unavailable" path:
// no warm-up device, no update notice, no dump. d3d11.h, winhttp.h and
// dbghelp.h include this header.
#ifndef TINTA_LINUX_MAIN_H
#define TINTA_LINUX_MAIN_H

#include "tinta_shell.h"

// ---- D3D11 -------------------------------------------------------------------------------
struct ID3D11Device : IUnknown {};
struct ID3D11DeviceContext : IUnknown {};
struct IDXGIAdapter;
enum D3D_FEATURE_LEVEL { D3D_FEATURE_LEVEL_11_0 = 0xb000 };
enum D3D_DRIVER_TYPE { D3D_DRIVER_TYPE_HARDWARE = 1 };
#define D3D11_CREATE_DEVICE_BGRA_SUPPORT 0x20
#define D3D11_SDK_VERSION 7
HRESULT D3D11CreateDevice(IDXGIAdapter* adapter, D3D_DRIVER_TYPE driverType, HMODULE software, UINT flags,
                          const D3D_FEATURE_LEVEL* featureLevels, UINT levelCount, UINT sdkVersion,
                          ID3D11Device** device, D3D_FEATURE_LEVEL* featureLevel, ID3D11DeviceContext** context);

// ---- WinHTTP -------------------------------------------------------------------------------
typedef void* HINTERNET;
typedef WORD INTERNET_PORT;
#define WINHTTP_ACCESS_TYPE_DEFAULT_PROXY 0
#define WINHTTP_NO_PROXY_NAME nullptr
#define WINHTTP_NO_PROXY_BYPASS nullptr
#define WINHTTP_NO_REFERER nullptr
#define WINHTTP_DEFAULT_ACCEPT_TYPES nullptr
#define WINHTTP_NO_ADDITIONAL_HEADERS nullptr
#define WINHTTP_NO_REQUEST_DATA nullptr
#define WINHTTP_FLAG_SECURE 0x00800000
#define INTERNET_DEFAULT_HTTPS_PORT 443
HINTERNET WinHttpOpen(LPCWSTR agent, DWORD accessType, LPCWSTR proxy, LPCWSTR proxyBypass, DWORD flags);
HINTERNET WinHttpConnect(HINTERNET session, LPCWSTR server, INTERNET_PORT port, DWORD reserved);
HINTERNET WinHttpOpenRequest(HINTERNET connect, LPCWSTR verb, LPCWSTR object, LPCWSTR version, LPCWSTR referrer,
                             LPCWSTR* acceptTypes, DWORD flags);
BOOL WinHttpSendRequest(HINTERNET request, LPCWSTR headers, DWORD headersLength, void* optional,
                        DWORD optionalLength, DWORD totalLength, DWORD_PTR context);
BOOL WinHttpReceiveResponse(HINTERNET request, void* reserved);
BOOL WinHttpQueryDataAvailable(HINTERNET request, DWORD* available);
BOOL WinHttpReadData(HINTERNET request, void* buffer, DWORD toRead, DWORD* read);
BOOL WinHttpCloseHandle(HINTERNET handle);

// ---- Crash dumps ---------------------------------------------------------------------------
struct EXCEPTION_RECORD;
struct CONTEXT;
struct EXCEPTION_POINTERS {
    EXCEPTION_RECORD* ExceptionRecord;
    CONTEXT* ContextRecord;
};
typedef LONG (*LPTOP_LEVEL_EXCEPTION_FILTER)(EXCEPTION_POINTERS* info);
#define EXCEPTION_CONTINUE_SEARCH 0
LPTOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER filter);

struct MINIDUMP_EXCEPTION_INFORMATION {
    DWORD ThreadId;
    EXCEPTION_POINTERS* ExceptionPointers;
    BOOL ClientPointers;
};
enum MINIDUMP_TYPE { MiniDumpWithIndirectlyReferencedMemory = 0x40 };
BOOL MiniDumpWriteDump(HANDLE process, DWORD processId, HANDLE file, MINIDUMP_TYPE type,
                       MINIDUMP_EXCEPTION_INFORMATION* exception, void* userStream, void* callback);
HANDLE GetCurrentProcess();
DWORD GetCurrentThreadId();

#endif  // TINTA_LINUX_MAIN_H
