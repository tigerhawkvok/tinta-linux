#include "notices.h"

#include "app.h"
#include "editor.h"
#include "fs_path.h"
#include "tabs.h"

#include <cstddef>
#include <filesystem>
#include <fstream>

extern const unsigned char kThirdPartyNotices[];
extern const std::size_t kThirdPartyNoticesSize;

std::string thirdPartyNotices() {
    return std::string(reinterpret_cast<const char*>(kThirdPartyNotices), kThirdPartyNoticesSize);
}

bool openThirdPartyNotices(App& app, HWND hwnd) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path dir = fs::temp_directory_path(ec);
    if (ec) return false;
    wchar_t name[64];
    swprintf_s(name, _countof(name), L"tinta-%d.%d.%d-third-party-notices.md", TINTA_VERSION_MAJOR,
               TINTA_VERSION_MINOR, TINTA_VERSION_PATCH);
    fs::path path = dir / fsPath(std::wstring(name));
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(kThirdPartyNotices), (std::streamsize)kThirdPartyNoticesSize);
        if (!out) return false;
    }
    tabOpenPath(app, hwnd, toUtf8(path.wstring()));
    return true;
}
