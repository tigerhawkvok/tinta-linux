# Flatpak: validation

Philip, 2026-10-15: "Flatpak for sure, others optional".

## What is in `packaging/flatpak/`

- **`io.github.tigerhawkvok.Tinta.yml`**:
  - Runtime: GNOME 50. Rust comes from `org.freedesktop.Sdk.Extension.rust-stable`
    (1.98.1), not rustup: `TINTA_MERMAN_RUSTUP=OFF` is the new option in
    `cmake/merman.cmake`.
  - Install: Tinta's own component only (`cmake --install --component
    tinta`, as the AppImage does). The `cmake-ninja` build system installed
    everything, and md4c's own install rules fail as a FetchContent
    subproject (`md4c.pc` missing).
  - Sandbox permissions:
    - Wayland with X11 fallback, and DRI;
    - network, for the update check;
    - `--filesystem=host`: documents open and save anywhere, with their images
      beside them;
    - `--own-name=org.tinta.Instance`, for single instance.
  - The desktop file and icon are renamed to the app id.
- **`io.github.tigerhawkvok.Tinta.metainfo.xml`**: AppStream metadata.
  `appstreamcli validate --no-net` passes (one pedantic note), and
  `flatpak-builder`'s compose step accepts it.
- **`build-flatpak.sh OUT [--install]`** builds the repo and a single-file
  bundle, `OUT/Tinta.flatpak`.

This is a **local build**: md4c, Corrosion and merman's crates download
while it builds (`--share=network` in build-args). Flathub would need them
vendored (for example with `flatpak-cargo-generator`), which is not done.

## Checked

- **Build:** `build-flatpak.sh build-flatpak` builds from this tree in 410 s
  (including merman's Rust), and the bundle is 8.9 MB. It needs
  `org.gnome.Sdk//50` and the Rust extension, installed for this user
  (about 1.4 GB downloaded, 4.3 GB on disk).
- **`tests/linux_render/flatpak_check.py`**, run from the build directory
  inside the sandbox (`flatpak build`, X11 only, Xvfb, its own config):
  - `--render-png` of `tests/fixtures/mermaid-label-fit.md` renders (exit 0);
  - `/app/share/tinta/fonts` holds Selawik and Monaspace Argon, and
    `/app/share/metainfo` the metainfo;
  - the window opens in Paper. Body text is in Selawik, inline code in Argon,
    and the Mermaid diagrams are drawn by merman with measured labels
    (screenshot).

## Not checked

- Installing the bundle (`flatpak install --bundle`) and running it with
  `flatpak run` on Philip's KDE Wayland session. The portals (file dialogs,
  print, colour scheme) and the per-app config in
  `~/.var/app/io.github.tigerhawkvok.Tinta` are untested.
- Single instance between two sandboxed instances.
- A Flathub submission: it would need vendored sources and a review.

## Note: the first run overwrote the real settings

The first sandboxed run had no `XDG_CONFIG_HOME` of its own. `flatpak build`
gives no per-app directories, so Tinta read Philip's
`~/.config/Tinta/settings.ini` and saved it back on exit:

- the window became 0,0 1024 × 768;
- the fixture went to the top of Recent.

The two test entries in Recent were removed again. The file as it stood
then is kept as `settings.ini.bak-flatpak-test-20261016-013742`. His
earlier window size was not recoverable. `flatpak_check.py` now passes its
own `XDG_CONFIG_HOME`, and the real file's timestamp did not change on
later runs.
