#!/bin/sh
# Cross-compiles the portable tinta.exe on Linux: clang-cl and lld-link
# against the MSVC CRT and Windows SDK, as an MSVC build would (static CRT,
# one file, no installer).
#
#   TINTA_ACCEPT_MSVC_LICENSE=1 platform/windows/build-windows.sh OUT [CACHE]
#
#   OUT    output directory (e.g. build/windows): build tree and tinta.exe
#   CACHE  toolchain cache (default ${XDG_CACHE_HOME:-~/.cache}/tinta-cross)
#
# Fetched into CACHE when missing, at pinned versions with SHA-256 checks:
#   - llvm-mingw, for its LLVM (clang in cl mode, lld-link, llvm-lib, llvm-rc)
#   - xwin, which downloads the MSVC CRT and Windows SDK from Microsoft.
#     That download means accepting Microsoft's license terms, hence
#     TINTA_ACCEPT_MSVC_LICENSE=1.
# Portable mode (#147): a settings.ini beside tinta.exe keeps its settings
# there instead of %APPDATA%\Tinta.
set -eu

[ $# -ge 1 ] || { sed -n '2,18p' "$0"; exit 2; }
SRC=$(realpath "$(dirname "$0")/../..")
mkdir -p "$1"
OUT=$(realpath "$1")
CACHE=$(realpath -m "${2:-${XDG_CACHE_HOME:-$HOME/.cache}/tinta-cross}")
mkdir -p "$CACHE"

LLVM_MINGW=llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64
LLVM_MINGW_URL=https://github.com/mstorsjo/llvm-mingw/releases/download/20260922/$LLVM_MINGW.tar.xz
LLVM_MINGW_SHA=bb7bb7654b33d5aa8712acb837c963b2e0c56352560c76105270a3268c665c21
XWIN=xwin-0.10.0-x86_64-unknown-linux-musl
XWIN_URL=https://github.com/Jake-Shadle/xwin/releases/download/0.10.0/$XWIN.tar.gz
XWIN_SHA=d870eb4b2f390878af6da1ccd3cf321d22fcb72720984853b4be732ae597fc88

fetch() {  # url file sha256: download once, verify every time
    if [ ! -f "$2" ]; then
        curl -fsSL -o "$2.part" "$1"
        mv "$2.part" "$2"
    fi
    echo "$3  $2" | sha256sum -c --quiet -
}

if [ ! -x "$CACHE/$LLVM_MINGW/bin/clang" ]; then
    fetch "$LLVM_MINGW_URL" "$CACHE/$LLVM_MINGW.tar.xz" "$LLVM_MINGW_SHA"
    tar -C "$CACHE" -xf "$CACHE/$LLVM_MINGW.tar.xz"
fi
# clang-cl is clang under that name
mkdir -p "$CACHE/bin"
for tool in lld-link llvm-lib llvm-rc; do
    ln -sf "../$LLVM_MINGW/bin/$tool" "$CACHE/bin/$tool"
done
ln -sf "../$LLVM_MINGW/bin/clang" "$CACHE/bin/clang-cl"

if [ ! -d "$CACHE/msvc-sdk/sdk/include/um" ]; then
    if [ "${TINTA_ACCEPT_MSVC_LICENSE:-}" != 1 ]; then
        echo "The MSVC CRT and Windows SDK are not in $CACHE yet. Downloading them" >&2
        echo "accepts Microsoft's license terms: rerun with TINTA_ACCEPT_MSVC_LICENSE=1." >&2
        exit 1
    fi
    if [ ! -x "$CACHE/$XWIN/xwin" ]; then
        fetch "$XWIN_URL" "$CACHE/$XWIN.tar.gz" "$XWIN_SHA"
        tar -C "$CACHE" -xzf "$CACHE/$XWIN.tar.gz"
    fi
    "$CACHE/$XWIN/xwin" --accept-license --arch x86_64 --cache-dir "$CACHE/xwin-cache" \
        splat --output "$CACHE/msvc-sdk"
    rm -rf "$CACHE/xwin-cache"  # the downloaded packages; the splat is what builds use
fi

cmake -S "$SRC" -B "$OUT/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    -DCMAKE_TOOLCHAIN_FILE="$SRC/cmake/toolchains/clang-cl-xwin.cmake" \
    -DTINTA_LLVM_BIN="$CACHE/bin" -DTINTA_XWIN_SDK="$CACHE/msvc-sdk"
cmake --build "$OUT/build" --target tinta
cp "$OUT/build/tinta.exe" "$OUT/tinta.exe"

# Portable means no runtime DLLs: only Windows' own may be imported
"$CACHE/$LLVM_MINGW/bin/llvm-readobj" --coff-imports "$OUT/tinta.exe" | sed -n 's/^ *Name: //p' | sort -u > "$OUT/imports.txt"
if grep -iE '^(vcruntime|msvcp|api-ms-win-crt|ucrtbase|libc\+\+|libwinpthread)' "$OUT/imports.txt"; then
    echo "tinta.exe imports a runtime DLL (above): the CRT must be static" >&2
    exit 1
fi
ls -l "$OUT/tinta.exe"
