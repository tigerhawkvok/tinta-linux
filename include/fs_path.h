#ifndef TINTA_FS_PATH_H
#define TINTA_FS_PATH_H

#include <filesystem>
#include <string>

// Tinta builds paths as wide strings and joins them with '\'. Use fsPath
// wherever one becomes a std::filesystem::path (fstreams included): on
// Windows it is the plain conversion; elsewhere '\' becomes '/'.
inline std::filesystem::path fsPath(std::wstring path) {
#ifndef _WIN32
    for (wchar_t& c : path) {
        if (c == L'\\') c = L'/';
    }
#endif
    return std::filesystem::path(path);
}

#endif  // TINTA_FS_PATH_H
