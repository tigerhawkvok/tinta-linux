# Linux phase 2 (minimum) and the AppImage: validation

Validated on 2026-10-14 on Ubuntu 25.10 with GCC 15.2, GTK 4.20.1 from the
system (`build-system`, which the AppImage is built from), and GTK 4.22.4 from
Linuxbrew (the default `build`). This is the minimum of phase 2 in
`docs/linux-port/PLAN.md`: Tinta in a real window. It is enough to read
documents and use the viewer from the keyboard and mouse.

## What runs

- `tinta [file.md]` opens a GTK 4 window: an undecorated `GtkWindow` with
  Tinta's own caption, tabs and caption buttons, presenting the
  `HwndRenderTarget` Cairo surface (`platform/linux/src/gtk_window.cpp`).
- Input goes into Tinta's message queue as the Win32 messages its WndProc
  already handles:
  - buttons and motion, including non-client hit tests and caption
    double-clicks
  - wheel and touchpad scrolling
  - keys, with `WM_CHAR` coming through `TranslateMessage` only for keys
    Tinta declines (the Windows contract)
  - focus, close requests, and resize as `WM_SIZE`
  - a change of scale as `WM_DPICHANGED`
- `GetMessage` runs the GLib main loop while the queue is empty, and
  `PostMessage` wakes it from any thread.
- `DefWindowProc` handles:
  - resize borders and edge resizing
  - caption moves
  - maximize, minimize, restore and close (`SC_*`)
  - `WM_CLOSE` → `DestroyWindow` → `WM_DESTROY`
  - the context menu on right-button-up
  - cursors
- Clipboard text crosses to and from the desktop (GdkClipboard). Open and
  Save dialogs use GtkFileDialog; message boxes use GtkAlertDialog.
- Headless runs never start GTK: `--render-png`, `--exportpdf`,
  `--exporthtml`, `--exportdocx`, `--printpages` and `--register`.
  Without a display, the interactive mode says so and exits with 1.
- The AppImage (`platform/linux/appimage/build-appimage.sh`):
  - It is built with linuxdeploy `1-alpha-20251107-1` and
    linuxdeploy-plugin-gtk `7a3fbc3`, both pinned and SHA-256 checked.
  - It bundles GTK 4.20, Pango, librsvg, libcurl and the fonts
    (Selawik, Monaspace Argon), plus `tinta.desktop` and hicolor icons
    taken from `resources/tinta.ico`.
  - The GTK plugin's forced `GDK_BACKEND=x11` is relaxed to
    `wayland,x11`, so Tinta runs natively on Wayland.

## Automated checks

- **`ctest --test-dir build`: 44/44.** That is the 43 earlier suites plus
  the new `linux_gtk_window`, which is registered when `TINTA_GUI_PYTHON`
  names a Python with Pillow and `xvfb-run` exists.
- **`build-system` and `build-clang`: 42/42 each.** These trees have no GUI
  Python configured, and merman is off.
- **`tests/linux_render/gui_check.py TINTA DOC [--scale N]`** checks the
  window, never on the user's session. It runs Tinta on a private Xvfb
  server with `GDK_BACKEND=x11` forced, `WAYLAND_DISPLAY` removed and an
  isolated XDG config. It drives Tinta with XTest and reads the screen and
  the X `CLIPBOARD` back through Xlib (ctypes). Its checks, on
  `tests/fixtures/gtk-window.md`:

  | Check | Pass condition | `build/tinta` 1× | AppImage 1× | `build/tinta` 2× |
  |---|---|---|---|---|
  | map | viewable window | 84 ms | 205 ms | 92 ms |
  | frame | equal to the headless `--render-png` (same size and scale) | 0.00% differ | 0.00% | 0.00% |
  | wheel | 5 notches change > 5% of pixels | 7.8% | 7.8% | 11.4% |
  | keys | Ctrl+F changes the frame, then typing changes it again | 0.23% / 0.097% | same | 1.36% / 0.345% |
  | clipboard | Ctrl+C (no selection) puts the marker on CLIPBOARD | 1,465 chars | same | same |
  | select | a drag then Ctrl+C copies part of the text | 119 of 1,465 | same | 48 of 1,465 |
  | resize | 700×500 is drawn to its corner and laid out again | 7.8% differs from the old frame cropped | same | 14.4% |
  | close | clicking Tinta's caption close button exits with 0 | exit 0 | exit 0 | exit 0 |

  The AppImage map time includes its FUSE mount (about 110 ms more than the
  bare binary). Xvfb uses `GSK_RENDERER=cairo`, because Vulkan runs in
  software there.
- **The checker was mutation-tested** with
  `$CLAUDE_JOB_DIR/tmp/mutate_gui.py`, which rebuilds with one piece of the
  window layer removed and requires the matching check to fail. All 6 were
  caught, and the unmutated build passes:

  | Mutation | Checks that failed |
  |---|---|
  | no `WM_CHAR` from `TranslateMessage` | keys |
  | no clipboard publish | clipboard, select |
  | no present after `EndDraw` | wheel, keys |
  | no `WM_SIZE` | resize, close |
  | `WM_CLOSE` does not destroy | close |
  | no client `WM_LBUTTONDOWN` | select |

- **AppImage headless parity.** `render_fixtures.py` ran the FUSE-mounted
  AppImage and the `build-system` binary on six fixtures: `gtk-window`,
  `code-font-monaspace`, `math-compatibility`, `mermaid-preamble`,
  `wide-table-239` and `inline-code-baseline-262`. All six PNGs are
  pixel-identical, and the PDF exports have the same sizes.
- **AppImage contents:**
  - 13 bundled font files (the same as `third_party/fonts`)
  - `RUNPATH` is `$ORIGIN/../lib`
  - `ldd` finds nothing outside the bundle and the base system, and no
    Linuxbrew paths
  - md4c is linked statically

## Looked at by eye

The checker's screenshots are in `build/gui-check*/`:
- the initial frame
- after the wheel
- the find bar showing "zqx" and "No matches"
- the 700×500 relayout, with its "Copied" notification bell
- the 2× frame, with crisp text at twice the size

## Not covered yet

- **Philip's GNOME/Wayland session.** Nothing was opened there, by design.
  Untested on a real compositor:
  - caption drag and edge resize (`gdk_toplevel_begin_move/resize` needs a
    window manager; Xvfb has none)
  - fractional scales (1.25, 1.5)
  - real-GPU time to first frame
  - the `wayland,x11` backend choice
- **IME and compose/dead keys.** Keys map to text with
  `gdk_keyval_to_unicode`; there is no `GtkIMContext` yet (phase 3).
- **Clipboard:** only text crosses to the desktop, not HTML, PNG or TSV.
  OLE clipboard snapshots stay in-process.
- **Not wired yet:** drag-and-drop of files into the window, the tab-drag
  ghost and tear-off windows (phase 4), single instance (each launch is a
  new process), print dialogs, and dark-mode follow.
- **GtkFileDialog and GtkAlertDialog** are written but not driven by the
  checker (no MessageBox path in the fixture, and Xvfb has no portal).
- **The AppImage's system floor**, measured with `objdump -T` over every
  bundled ELF: glibc ≥ 2.38 and the `libstdc++` of GCC 13 or later
  (`GLIBCXX_3.4.32`). Both stay with the host, per linuxdeploy's excludelist.
  Ubuntu 24.04, Debian 13 and Fedora 39 and later meet it; Ubuntu 22.04 does
  not. A build on an older base would widen that.
  Mermaid still uses the native parsers (merman is not wired into layout
  yet, MM3), so the AppImage is built with `TINTA_MERMAN=OFF`.
