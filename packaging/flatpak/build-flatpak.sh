#!/bin/sh
# Builds Tinta as a Flatpak from this checkout and bundles it in one file.
#
#   packaging/flatpak/build-flatpak.sh OUT [--install]
#
#   OUT        output directory: the build dir, the repo, and Tinta.flatpak
#   --install  also install the bundle for this user (`flatpak run
#              io.github.tigerhawkvok.Tinta`)
#
# Needs flatpak-builder, org.gnome.Sdk//50 and
# org.freedesktop.Sdk.Extension.rust-stable//25.08:
#   flatpak install --user flathub org.gnome.Sdk//50 \
#       org.freedesktop.Sdk.Extension.rust-stable//25.08
set -eu

[ $# -ge 1 ] || { sed -n '2,13p' "$0"; exit 2; }
HERE=$(dirname "$(realpath "$0")")
mkdir -p "$1"
OUT=$(realpath "$1")
APP=io.github.tigerhawkvok.Tinta

flatpak-builder --user --force-clean --disable-rofiles-fuse \
    --state-dir="$OUT/.flatpak-builder" --repo="$OUT/repo" \
    "$OUT/build-dir" "$HERE/$APP.yml"
flatpak build-bundle "$OUT/repo" "$OUT/Tinta.flatpak" "$APP" \
    --runtime-repo=https://flathub.org/repo/flathub.flatpakrepo
ls -l "$OUT/Tinta.flatpak"

if [ "${2:-}" = --install ]; then
    flatpak install --user --noninteractive -y --bundle "$OUT/Tinta.flatpak"
fi
