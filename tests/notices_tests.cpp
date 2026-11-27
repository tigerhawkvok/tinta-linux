// The third-party notices built into the binary match
// packaging/THIRD-PARTY-NOTICES.md, cover what Tinta ships, and open as a tab
#include "d2d_init.h"
#include "notices.h"
#include "render.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace {

int failures = 0;

void check(bool ok, const std::string& message) {
    if (!ok) {
        std::cerr << "FAIL: " << message << "\n";
        failures++;
    }
}

std::string read(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

}  // namespace

int main() {
    std::string embedded = thirdPartyNotices();
    check(embedded == read(TINTA_NOTICES_FILE), "the built-in notices are packaging/THIRD-PARTY-NOTICES.md");
    for (const char* part : {"# Third-party notices", "## Tinta", "Copyright (c) 2024 oipoistar", "## md4c",
                             "### Selawik", "### Monaspace Argon", "SIL OPEN FONT LICENSE", "| merman-ffi |",
                             "| resvg |", "Mozilla Public License Version 2.0", "KaTeX"}) {
        check(embedded.find(part) != std::string::npos, std::string("the notices include ") + part);
    }

    auto state = std::make_unique<App>();
    App& app = *state;
    app.hwnd = CreateWindowExW(0, L"STATIC", L"Notices tests", WS_POPUP, 0, 0, 900, 600, nullptr, nullptr,
                               nullptr, nullptr);
    if (!app.hwnd || !initD2D(app) || !createRenderTarget(app)) return 2;
    app.width = 900;
    app.height = 600;
    updateTextFormats(app);
    check(openThirdPartyNotices(app, app.hwnd), "Settings > Licenses writes the notices");
    check(app.currentFile.find("third-party-notices.md") != std::string::npos,
          "and opens them as the active tab: " + app.currentFile);
    check(read(app.currentFile) == embedded, "the opened file is the notices, byte for byte");

    std::cout << "notices: " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
