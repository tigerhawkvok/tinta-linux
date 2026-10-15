// merman's raster loads the font files named in MERMAN_RASTER_FONT_FILES
// (cmake/patches/merman-raster-font-files.patch) instead of scanning every
// system font. Its font database is built once per process, so each case
// renders in a forked child.
#include "merman_engine.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        failures++;
    }
}

const char* kSource = "flowchart LR\n  A[Hello world] --> B[Font check]\n";

// The PNG of kSource in Selawik, rendered in a child with `files` as the
// font list (nullptr: the variable unset)
std::string renderWith(const char* files) {
    int pipeFds[2];
    if (pipe(pipeFds) != 0) return {};
    pid_t child = fork();
    if (child == 0) {
        close(pipeFds[0]);
        if (files) {
            setenv("MERMAN_RASTER_FONT_FILES", files, 1);
        } else {
            unsetenv("MERMAN_RASTER_FONT_FILES");
        }
        merman_engine::Theme theme;
        theme.fontFamily = "Selawik";
        std::string error;
        std::string png = merman_engine::renderPng(kSource, theme, 1.0f, "#ffffff", error);
        size_t at = 0;
        while (at < png.size()) {
            ssize_t wrote = write(pipeFds[1], png.data() + at, png.size() - at);
            if (wrote <= 0) break;
            at += (size_t)wrote;
        }
        _exit(png.empty() ? 1 : 0);
    }
    close(pipeFds[1]);
    std::string png;
    char buffer[65536];
    ssize_t got;
    while ((got = read(pipeFds[0], buffer, sizeof buffer)) > 0) png.append(buffer, (size_t)got);
    close(pipeFds[0]);
    int status = 0;
    waitpid(child, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? png : std::string();
}

}  // namespace

int main() {
    const std::string selawik = TINTA_FONTS_DIR "/selawik/selawk.ttf";
    const std::string argon = TINTA_FONTS_DIR "/monaspace-argon/MonaspaceArgon-Regular.otf";

    std::string selawikOnly = renderWith(selawik.c_str());
    check(selawikOnly.size() > 1000, "renders with Selawik as the only font");
    // A face the diagram does not use changes nothing
    std::string both = renderWith((selawik + ":" + argon).c_str());
    check(!both.empty() && both == selawikOnly, "an unused extra font leaves the PNG byte-identical");
    // Without Selawik in the list, merman cannot use it, installed or not
    std::string argonOnly = renderWith(argon.c_str());
    check(!argonOnly.empty() && argonOnly != selawikOnly, "the list decides the face: Argon only draws differently");
    // Nothing loadable: the system scan, as with the variable unset
    std::string unloadable = renderWith("/nonexistent/font.ttf");
    std::string unset = renderWith(nullptr);
    check(!unset.empty() && unloadable == unset, "an unloadable list falls back to the system fonts");

    std::cout << "merman raster fonts: " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
