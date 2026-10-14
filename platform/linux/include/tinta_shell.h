// Shell, common dialogs, registry, streams, print-document targets and the
// app-model query for the Linux compat layer. Every small Windows header
// (shlobj.h, shellapi.h, commdlg.h, ...) includes this one.
#ifndef TINTA_LINUX_SHELL_H
#define TINTA_LINUX_SHELL_H

#include "tinta_com.h"

// ---- Shell ---------------------------------------------------------------------------
#define CSIDL_APPDATA 0x001a
#define CSIDL_LOCAL_APPDATA 0x001c
#define SHGFP_TYPE_CURRENT 0
HRESULT SHGetFolderPathW(HWND hwnd, int csidl, HANDLE token, DWORD flags, LPWSTR path);

#define SEE_MASK_NOCLOSEPROCESS 0x00000040
#define SEE_MASK_NOASYNC 0x00000100
#define SEE_MASK_FLAG_NO_UI 0x00000400
struct SHELLEXECUTEINFOW {
    DWORD cbSize;
    ULONG fMask;
    HWND hwnd;
    LPCWSTR lpVerb;
    LPCWSTR lpFile;
    LPCWSTR lpParameters;
    LPCWSTR lpDirectory;
    int nShow;
    HINSTANCE hInstApp;
    void* lpIDList;
    LPCWSTR lpClass;
    HKEY hkeyClass;
    DWORD dwHotKey;
    HANDLE hIcon;
    HANDLE hProcess;
};
// Returns a value > 32 on success, as on Windows
HINSTANCE ShellExecuteW(HWND hwnd, LPCWSTR operation, LPCWSTR file, LPCWSTR parameters, LPCWSTR directory,
                        int showCmd);
HINSTANCE ShellExecuteA(HWND hwnd, LPCSTR operation, LPCSTR file, LPCSTR parameters, LPCSTR directory,
                        int showCmd);
BOOL ShellExecuteExW(SHELLEXECUTEINFOW* info);
#define SHCNE_ASSOCCHANGED 0x08000000L
#define SHCNF_IDLIST 0x0000
void SHChangeNotify(LONG eventId, UINT flags, const void* item1, const void* item2);

// ---- Streams over files and memory -----------------------------------------------------
HRESULT SHCreateStreamOnFileEx(LPCWSTR file, DWORD mode, DWORD attributes, BOOL create, IStream* templ,
                               IStream** stream);
IStream* SHCreateMemStream(const BYTE* init, UINT size);
HRESULT CreateStreamOnHGlobal(HGLOBAL global, BOOL deleteOnRelease, IStream** stream);
HRESULT GetHGlobalFromStream(IStream* stream, HGLOBAL* global);

// ---- Common dialogs --------------------------------------------------------------------
#define OFN_OVERWRITEPROMPT 0x00000002
#define OFN_HIDEREADONLY 0x00000004
#define OFN_NOCHANGEDIR 0x00000008
#define OFN_PATHMUSTEXIST 0x00000800
#define OFN_FILEMUSTEXIST 0x00001000
struct OPENFILENAMEW {
    DWORD lStructSize;
    HWND hwndOwner;
    HINSTANCE hInstance;
    LPCWSTR lpstrFilter;
    LPWSTR lpstrCustomFilter;
    DWORD nMaxCustFilter;
    DWORD nFilterIndex;
    LPWSTR lpstrFile;
    DWORD nMaxFile;
    LPWSTR lpstrFileTitle;
    DWORD nMaxFileTitle;
    LPCWSTR lpstrInitialDir;
    LPCWSTR lpstrTitle;
    DWORD Flags;
    WORD nFileOffset;
    WORD nFileExtension;
    LPCWSTR lpstrDefExt;
    LPARAM lCustData;
    void* lpfnHook;
    LPCWSTR lpTemplateName;
    void* pvReserved;
    DWORD dwReserved;
    DWORD FlagsEx;
};
BOOL GetOpenFileNameW(OPENFILENAMEW* ofn);
BOOL GetSaveFileNameW(OPENFILENAMEW* ofn);

#define DM_SPECVERSION 0x0401
#define DM_ORIENTATION 0x00000001L
#define DM_PAPERSIZE 0x00000002L
#define DMORIENT_PORTRAIT 1
#define DMORIENT_LANDSCAPE 2
#define DMPAPER_LETTER 1
#define DMPAPER_LEGAL 5
#define DMPAPER_A3 8
#define DMPAPER_A4 9
#define PD_NOSELECTION 0x00000004
#define PD_NOPAGENUMS 0x00000008
#define PD_RETURNDC 0x00000100
#define PD_USEDEVMODECOPIESANDCOLLATE 0x00040000
struct DEVMODEW {
    WCHAR dmDeviceName[32];
    WORD dmSpecVersion;
    WORD dmDriverVersion;
    WORD dmSize;
    WORD dmDriverExtra;
    DWORD dmFields;
    short dmOrientation;
    short dmPaperSize;
    short dmPaperLength;
    short dmPaperWidth;
    short dmScale;
    short dmCopies;
    short dmDefaultSource;
    short dmPrintQuality;
    short dmColor;
    short dmDuplex;
    short dmYResolution;
    short dmTTOption;
    short dmCollate;
    WCHAR dmFormName[32];
};
struct DEVNAMES {
    WORD wDriverOffset;
    WORD wDeviceOffset;
    WORD wOutputOffset;
    WORD wDefault;
};
struct PRINTDLGW {
    DWORD lStructSize;
    HWND hwndOwner;
    HGLOBAL hDevMode;
    HGLOBAL hDevNames;
    HDC hDC;
    DWORD Flags;
    WORD nFromPage;
    WORD nToPage;
    WORD nMinPage;
    WORD nMaxPage;
    WORD nCopies;
    HINSTANCE hInstance;
    LPARAM lCustData;
    void* lpfnPrintHook;
    void* lpfnSetupHook;
    LPCWSTR lpPrintTemplateName;
    LPCWSTR lpSetupTemplateName;
    HGLOBAL hPrintTemplate;
    HGLOBAL hSetupTemplate;
};
BOOL PrintDlgW(PRINTDLGW* pd);

// ---- Registry: nothing is stored there on Linux ----------------------------------------
#define KEY_READ 0x20019
#define KEY_WRITE 0x20006
#define KEY_ALL_ACCESS 0xF003F
#define REG_NONE 0
#define REG_SZ 1
#define REG_DWORD 4
#define REG_OPTION_NON_VOLATILE 0x00000000L
#define RRF_RT_REG_SZ 0x00000002
#define RRF_RT_REG_DWORD 0x00000010
typedef DWORD REGSAM;
LONG RegOpenKeyExW(HKEY key, LPCWSTR subKey, DWORD options, REGSAM access, HKEY* result);
LONG RegCreateKeyExW(HKEY key, LPCWSTR subKey, DWORD reserved, LPWSTR className, DWORD options, REGSAM access,
                     SECURITY_ATTRIBUTES* security, HKEY* result, DWORD* disposition);
LONG RegCloseKey(HKEY key);
LONG RegSetValueExW(HKEY key, LPCWSTR valueName, DWORD reserved, DWORD type, const BYTE* data, DWORD size);
LONG RegQueryValueExW(HKEY key, LPCWSTR valueName, DWORD* reserved, DWORD* type, BYTE* data, DWORD* size);
LONG RegGetValueW(HKEY key, LPCWSTR subKey, LPCWSTR value, DWORD flags, DWORD* type, void* data, DWORD* size);
LONG RegDeleteTreeW(HKEY key, LPCWSTR subKey);

// ---- App model: never a packaged (Store) app on Linux -----------------------------------
#define APPMODEL_ERROR_NO_PACKAGE 15700L
LONG GetCurrentPackageFullName(UINT32* packageFullNameLength, LPWSTR packageFullName);

// ---- Downloads -------------------------------------------------------------------------
HRESULT URLDownloadToCacheFileW(IUnknown* caller, LPCWSTR url, LPWSTR fileName, DWORD bufferSize,
                                DWORD reserved, void* callback);

// ---- Print document targets (documenttarget.h) -------------------------------------------
TINTA_UUID(IPrintDocumentPackageTarget, 0x0d0c0001)
TINTA_UUID(IPrintDocumentPackageTargetFactory, 0x0d0c0002)
TINTA_UUID(PrintDocumentPackageTargetFactory, 0x0d0c0003)

struct IPrintDocumentPackageTarget : IUnknown {
    virtual HRESULT Cancel() = 0;
};

struct IPrintDocumentPackageTargetFactory : IUnknown {
    virtual HRESULT CreateDocumentPackageTargetForPrintJob(LPCWSTR printerName, LPCWSTR jobName,
                                                           IStream* jobOutputStream, IStream* jobPrintTicketStream,
                                                           IPrintDocumentPackageTarget** docPackageTarget) = 0;
};

#endif  // TINTA_LINUX_SHELL_H
