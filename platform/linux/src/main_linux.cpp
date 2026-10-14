// Linux entry point. Tinta's WinMain (src/main_d2d.cpp) runs unchanged: it
// reads its arguments through CommandLineToArgvW(GetCommandLineW()), which
// the compat layer serves from /proc/self/cmdline, and ignores options it
// does not know. The options below are Linux-only and take the
// --name=value form so WinMain never mistakes their values for a file.
//
//   --render-png=PATH   headless: capture the window once startup settles
//   --size=WxH          client size in pixels (default: the saved size)
//   --scale=N           display scale (DPI 96 x N), e.g. 1.5 or 2
//   --settle-ms=N       quiet time before the capture (default 400)
//   --repaint=N         then time N full repaints (median and max frame)
#include "compat_internal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show);

namespace {

bool valueOf(const char* arg, const char* name, std::string& out) {
    size_t n = std::strlen(name);
    if (std::strncmp(arg, name, n) != 0 || arg[n] != '=') return false;
    out = arg + n + 1;
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    tinta::HeadlessOptions& options = tinta::headless();
    for (int i = 1; i < argc; i++) {
        std::string value;
        if (valueOf(argv[i], "--render-png", value)) {
            options.renderPng = value;
        } else if (valueOf(argv[i], "--size", value)) {
            if (std::sscanf(value.c_str(), "%dx%d", &options.width, &options.height) != 2) {
                std::fprintf(stderr, "--size expects WxH, got %s\n", value.c_str());
                return 2;
            }
        } else if (valueOf(argv[i], "--scale", value)) {
            double scale = std::atof(value.c_str());
            if (scale < 0.5 || scale > 4) {
                std::fprintf(stderr, "--scale expects 0.5 to 4, got %s\n", value.c_str());
                return 2;
            }
            options.dpi = static_cast<UINT>(96.0 * scale + 0.5);
        } else if (valueOf(argv[i], "--settle-ms", value)) {
            options.settleMs = std::atoi(value.c_str());
        } else if (valueOf(argv[i], "--repaint", value)) {
            options.repaints = std::atoi(value.c_str());
        }
    }
    return WinMain(nullptr, nullptr, nullptr, SW_SHOWNORMAL);
}
