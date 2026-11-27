#include "settings.h"
#include "fs_path.h"

#include <windows.h>
#include <shlobj.h>
#include <string>

// Portable mode (#147): a settings.ini beside the executable makes that
// folder the configuration home - settings, themes.ini, languages.ini,
// and drafts all live there, so the whole setup travels with the exe.
// Without it, everything stays in %APPDATA%\Tinta as before (Store
// installs land here automatically: their folder is read-only, so an
// exe-adjacent settings.ini cannot exist). Resolved once per run.
// Lives in its own translation unit so the test targets that compile
// themes.cpp or i18n.cpp can link it without dragging in settings.cpp.
std::wstring tintaConfigDir() {
    static std::wstring cached;
    static bool resolved = false;
    if (resolved) return cached;
    resolved = true;

    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
        std::wstring dir = exePath;
        size_t slash = dir.find_last_of(L"\\/");
        if (slash != std::wstring::npos) {
            dir = dir.substr(0, slash);
            DWORD attrs =
                GetFileAttributesW(joinPath(dir, L"settings.ini").c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES &&
                !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                cached = dir;
                return cached;
            }
        }
    }

    wchar_t appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0,
                                   appDataPath))) {
        std::wstring path = appDataPath;
#ifdef _WIN32
        // Tinta Mono keeps its own folder beside upstream Tinta's, so the two
        // run side by side without sharing state. The first run copies
        // Tinta's settings, themes and translations over (they are copies:
        // Tinta keeps its own). Linux has no upstream Tinta: ~/.config/Tinta.
        std::wstring upstream = joinPath(path, L"Tinta");
        path = joinPath(path, L"TintaMono");
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES &&
            GetFileAttributesW(upstream.c_str()) != INVALID_FILE_ATTRIBUTES) {
            CreateDirectoryW(path.c_str(), nullptr);
            for (const wchar_t* name : {L"settings.ini", L"themes.ini", L"languages.ini"}) {
                CopyFileW(joinPath(upstream, name).c_str(), joinPath(path, name).c_str(), TRUE);
            }
        }
#else
        path = joinPath(path, L"Tinta");
#endif
        CreateDirectoryW(path.c_str(), nullptr);  // Create if not exists
        cached = path;
    }
    return cached;
}

std::wstring configFilePathForShell(const std::wstring& path) {
    if (path.empty()) return path;
    // Resolve the file, not just its parent: MSIX can merge redirected
    // files with existing, unvirtualized files in the same AppData folder.
    HANDLE file = CreateFileW(path.c_str(), 0,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return path;

    DWORD size = GetFinalPathNameByHandleW(file, nullptr, 0, VOLUME_NAME_DOS);
    std::wstring resolved(size, L'\0');
    DWORD length = size ? GetFinalPathNameByHandleW(file, resolved.data(), size,
                                                  VOLUME_NAME_DOS) : 0;
    CloseHandle(file);
    if (!length || length >= size) return path;
    resolved.resize(length);

    // Shell file associations expect ordinary DOS/UNC paths, not the
    // extended-path prefix returned by GetFinalPathNameByHandleW.
    if (resolved.rfind(L"\\\\?\\UNC\\", 0) == 0) {
        return L"\\\\" + resolved.substr(8);
    }
    if (resolved.rfind(L"\\\\?\\", 0) == 0 && resolved.size() > 6 &&
        resolved[5] == L':') {
        resolved.erase(0, 4);
    }
    return resolved;
}
