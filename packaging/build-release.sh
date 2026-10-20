#!/bin/sh
# Builds both release downloads locally for one version: the portable
# Windows exe (cross-compiled) and the Linux AppImage. Then it checks them.
# It never commits, tags or publishes anything.
#
#   packaging/build-release.sh VERSION [OUT]
#
#   VERSION  X.Y.Z. It is compiled into this build: the title bar, About,
#            the update check and the exe's file version. CMakeLists.txt is
#            not edited, and a later plain configure uses its default again.
#   OUT      where the downloads land (default build/release-VERSION)
#
# OUT receives the files the GitHub release lists:
#   tinta-mono-VERSION-windows-x64.exe   built in build/windows
#   Tinta-Mono-VERSION-x86_64.AppImage   built in build-appimage/
#   THIRD-PARTY-NOTICES.md
#   SHA256SUMS                           of the three above
#   BUILD-INFO.txt                       the commit, uncommitted files, checks
# Anything else in OUT (release notes) is left alone. Full logs are in
# OUT/logs/.
#
# The checks:
#   version  both trees compile VERSION, and the exe's file version says it
#   run      each download exports a Mermaid document to HTML, headless,
#            with the diagram drawn by merman. The exe runs under Wine on
#            Xvfb in a throwaway prefix, and is skipped when wine or
#            xvfb-run is missing. Scratch files are in build/release-smoke.
#
# The first run on a machine downloads toolchains:
# - The Windows one needs TINTA_ACCEPT_MSVC_LICENSE=1
#   (platform/windows/build-windows.sh).
# - The AppImage needs the GTK 4, librsvg and libcurl development packages,
#   plus rustup for merman.
set -eu

usage() { sed -n '2,33p' "$0"; exit 2; }
[ $# -ge 1 ] && [ $# -le 2 ] || usage
VERSION=$1
if ! printf '%s\n' "$VERSION" | grep -Eqx '[0-9]+\.[0-9]+\.[0-9]+'; then
    echo "VERSION must be three numbers, X.Y.Z (not \"$VERSION\")" >&2
    exit 2
fi
SRC=$(realpath "$(dirname "$0")/..")
cd "$SRC"
OUT=$(realpath -m "${2:-build/release-$VERSION}")
SMOKE="$SRC/build/release-smoke"
mkdir -p "$OUT/logs" "$SMOKE"

EXE="tinta-mono-$VERSION-windows-x64.exe"
APPIMAGE="Tinta-Mono-$VERSION-x86_64.AppImage"
began=$(date +%s)
CHECKS=""

# step NAME LOG COMMAND...: run quietly into LOG; on failure show its tail
step() {
    name=$1 log=$2
    shift 2
    printf '%-34s' "$name"
    start=$(date +%s)
    if "$@" > "$log" 2>&1; then
        echo "ok ($(( $(date +%s) - start )) s)"
    else
        echo "FAILED ($(( $(date +%s) - start )) s); the end of $log:"
        tail -n 25 "$log" | sed 's/^/    /'
        exit 1
    fi
}

check() {  # check NAME CONDITION-COMMAND...
    name=$1
    shift
    printf '%-34s' "$name"
    if "$@"; then
        echo ok
        CHECKS="$CHECKS
  ok       $name"
    else
        echo FAILED
        exit 1
    fi
}

skip() {  # skip NAME REASON
    printf '%-34s%s\n' "$1" "skipped: $2"
    CHECKS="$CHECKS
  skipped  $1 ($2)"
}

# The three version defines a tree compiles with, exactly
tree_version() {
    major=${VERSION%%.*} rest=${VERSION#*.}
    minor=${rest%%.*} patch=${rest#*.}
    [ "$(grep -o 'TINTA_VERSION_[A-Z]*=[0-9]*' "$1" | sort -u | tr '\n' ' ')" = \
      "TINTA_VERSION_MAJOR=$major TINTA_VERSION_MINOR=$minor TINTA_VERSION_PATCH=$patch " ]
}

exe_version() {
    [ "$(strings -el build/windows/tinta.exe | grep -cxF "$VERSION")" -ge 2 ]  # File and ProductVersion
}

# A Mermaid document exported to HTML by the AppImage, headless, with a
# private config (no first-run prompt, no update check)
appimage_runs() {
    rm -rf "$SMOKE/appimage"
    mkdir -p "$SMOKE/appimage/config/Tinta"
    printf '[Settings]\nhasAskedFileAssociation=1\ncheckUpdates=0\n' > "$SMOKE/appimage/config/Tinta/settings.ini"
    env -u DISPLAY -u WAYLAND_DISPLAY XDG_CONFIG_HOME="$SMOKE/appimage/config" APPIMAGE_EXTRACT_AND_RUN=1 \
        timeout 120 build/appimage/Tinta-x86_64.AppImage tests/fixtures/gtk-window.md \
        --exporthtml "$SMOKE/appimage/out.html" > "$OUT/logs/run-appimage.log" 2>&1 &&
        grep -q '<svg id="merman"' "$SMOKE/appimage/out.html"
}

# The same under Wine, in portable mode (settings.ini beside the exe). The
# prefix is new for each run and removed after it: setting one up takes
# about 10 s, and keeping it would leave 1.4 GB behind
exe_runs() {
    rm -rf "$SMOKE/wine-app" "$SMOKE/wineprefix"
    mkdir -p "$SMOKE/wine-app"
    cp build/windows/tinta.exe "$SMOKE/wine-app/tinta.exe"
    cp tests/fixtures/gtk-window.md "$SMOKE/wine-app/doc.md"
    printf '[Settings]\nhasAskedFileAssociation=1\ncheckUpdates=0\n' > "$SMOKE/wine-app/settings.ini"
    (cd "$SMOKE/wine-app" &&
        env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$SMOKE/wineprefix" WINEDEBUG=-all \
            WINEDLLOVERRIDES="mscoree,mshtml=" timeout 300 xvfb-run -a wine tinta.exe doc.md --exporthtml out.html \
            > "$OUT/logs/run-exe.log" 2>&1)
    status=$?
    WINEPREFIX="$SMOKE/wineprefix" wineserver -k > /dev/null 2>&1 || true
    rm -rf "$SMOKE/wineprefix"
    [ "$status" -eq 0 ] && grep -q '<svg id="merman"' "$SMOKE/wine-app/out.html"
}

echo "Tinta Mono $VERSION -> $OUT"

# Windows: build-windows.sh configures build/windows/build with TINTA_VERSION
step "Windows exe" "$OUT/logs/windows.log" \
    env TINTA_VERSION="$VERSION" sh platform/windows/build-windows.sh build/windows

# Linux: a Release tree on the system's compiler and libraries (no
# Linuxbrew), so the AppImage runs on other distributions
step "AppImage configure" "$OUT/logs/appimage-configure.log" \
    cmake -S . -B build-appimage -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
        -DPKG_CONFIG_EXECUTABLE=/usr/bin/pkg-config -DTINTA_VERSION="$VERSION"
step "AppImage" "$OUT/logs/appimage.log" \
    env LINUXDEPLOY_OUTPUT_VERSION="$VERSION" sh platform/linux/appimage/build-appimage.sh build-appimage build/appimage

check "version: exe tree" tree_version build/windows/build/build.ninja
check "version: AppImage tree" tree_version build-appimage/build.ninja
check "version: exe file version" exe_version
check "run: AppImage exports a diagram" appimage_runs
if command -v wine > /dev/null && command -v xvfb-run > /dev/null; then
    check "run: exe (Wine) exports a diagram" exe_runs
else
    skip "run: exe (Wine) exports a diagram" "wine or xvfb-run not installed"
fi

cp build/windows/tinta.exe "$OUT/$EXE"
cp build/appimage/Tinta-x86_64.AppImage "$OUT/$APPIMAGE"
cp packaging/THIRD-PARTY-NOTICES.md "$OUT/THIRD-PARTY-NOTICES.md"
(cd "$OUT" && sha256sum "$EXE" "$APPIMAGE" THIRD-PARTY-NOTICES.md > SHA256SUMS)

{
    echo "Tinta Mono $VERSION, built $(date '+%Y-%m-%d %H:%M %Z') on $(uname -n)"
    if git rev-parse --git-dir > /dev/null 2>&1; then
        echo "Commit: $(git rev-parse HEAD) ($(git rev-parse --abbrev-ref HEAD))"
        # New files count too: a source not yet added still builds in
        changed=$(git status --porcelain | wc -l)
        if [ "$changed" -eq 0 ]; then
            echo "Uncommitted changes: none"
        else
            echo "Uncommitted changes in $changed files, built in:"
            git status --porcelain | sed 's/^/  /'
        fi
    else
        echo "Commit: not a git checkout"
    fi
    echo "Checks:$CHECKS"
    echo "SHA-256:"
    sed 's/^/  /' "$OUT/SHA256SUMS"
} > "$OUT/BUILD-INFO.txt"

echo
ls -l "$OUT/$EXE" "$OUT/$APPIMAGE" | sed 's/^/  /'
sed -n '/^Commit/,/^Checks:/p' "$OUT/BUILD-INFO.txt" | grep -v '^Checks:' | sed 's/^/  /'
echo "Done in $(( $(date +%s) - began )) s. SHA256SUMS and BUILD-INFO.txt are in $OUT"
