#!/bin/sh
# Builds Tinta-x86_64.AppImage from a build tree that uses the system's
# libraries (configured with -DPKG_CONFIG_EXECUTABLE=/usr/bin/pkg-config).
#
#   platform/linux/appimage/build-appimage.sh BUILD OUT [TOOLS]
#
#   BUILD  the build tree; tinta is built, then installed into OUT/AppDir
#   OUT    output directory (e.g. build/appimage): AppDir/ and the AppImage
#   TOOLS  linuxdeploy and its GTK plugin (default OUT/tools), downloaded at
#          pinned versions and checked against their SHA-256 when missing
#
# linuxdeploy bundles every library except the base system (glibc, libGL,
# fontconfig, ...: its excludelist), so the AppImage runs where this host's
# glibc or a newer one is installed.
set -eu

[ $# -ge 2 ] || { sed -n '2,14p' "$0"; exit 2; }
BUILD=$(realpath "$1")
mkdir -p "$2"
OUT=$(realpath "$2")
TOOLS=$(realpath -m "${3:-$OUT/tools}")
mkdir -p "$TOOLS"

LINUXDEPLOY_URL=https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage
LINUXDEPLOY_SHA=c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
GTK_PLUGIN_URL=https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/7a3fbc31a9e5075073ff8790f26effbac5f84453/linuxdeploy-plugin-gtk.sh
GTK_PLUGIN_SHA=b0f4cbc684a0103a9651f0955b635eaea0096b3a66c0f5a2c2aa337960375171

fetch() {  # url file sha256
    if [ ! -f "$2" ]; then
        curl -fsSL -o "$2.part" "$1"
        mv "$2.part" "$2"
    fi
    echo "$3  $2" | sha256sum -c --quiet -
    chmod +x "$2"
}
fetch "$LINUXDEPLOY_URL" "$TOOLS/linuxdeploy-x86_64.AppImage" "$LINUXDEPLOY_SHA"
fetch "$GTK_PLUGIN_URL" "$TOOLS/linuxdeploy-plugin-gtk.sh" "$GTK_PLUGIN_SHA"

cmake --build "$BUILD" --target tinta
rm -rf "$OUT/AppDir"
DESTDIR="$OUT/AppDir" cmake --install "$BUILD" --prefix /usr --component tinta
if ldd "$OUT/AppDir/usr/bin/tinta" | grep -q linuxbrew; then
    echo "tinta links Linuxbrew libraries: configure BUILD with /usr/bin/pkg-config" >&2
    exit 1
fi

# The system's tools only: the GTK plugin asks pkg-config where GTK keeps its
# modules, and must find the GTK tinta links. linuxdeploy finds the plugin on
# PATH and runs extracted (no FUSE needed for the tool itself)
PATH="$TOOLS:/usr/bin:/bin"
export PATH
export DEPLOY_GTK_VERSION=4
export APPIMAGE_EXTRACT_AND_RUN=1
cd "$OUT"
linuxdeploy-x86_64.AppImage --appdir "$OUT/AppDir" --plugin gtk \
    --desktop-file "$OUT/AppDir/usr/share/applications/tinta.desktop" \
    --icon-file "$OUT/AppDir/usr/share/icons/hicolor/256x256/apps/tinta.png"

# The plugin forces X11 (XWayland) for old GTK 3 crashes; Tinta's GTK 4
# window runs natively on Wayland, so X11 is only the fallback
HOOK="$OUT/AppDir/apprun-hooks/linuxdeploy-plugin-gtk.sh"
sed -i 's|^export GDK_BACKEND=x11.*$|export GDK_BACKEND="${GDK_BACKEND:-wayland,x11}" # Tinta: native Wayland first|' "$HOOK"
grep -q 'GDK_BACKEND="${GDK_BACKEND:-wayland,x11}"' "$HOOK"

export LDAI_OUTPUT="$OUT/Tinta-x86_64.AppImage"
linuxdeploy-x86_64.AppImage --appdir "$OUT/AppDir" --output appimage
ls -l "$LDAI_OUTPUT"
