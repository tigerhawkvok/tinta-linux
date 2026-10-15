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

## On a real compositor (later on 2026-10-14)

Philip's desktop turned out to be KDE Plasma 6.4.5 on Wayland (KWin, one
3440×1440 monitor at scale 1), not GNOME. He allowed headed runs while he
was away and unlocked the session for them.

- **`tests/linux_render/desktop_check.py TINTA DOC`** drives Tinta on the
  live session. It uses uinput virtual devices (an absolute pointer and a
  keyboard, which KWin treats as hardware), queries window state through
  KWin scripting and captures with spectacle.
  - Every key and click first checks that KWin's active window is Tinta and
    that the point lies inside its frame. It aborts otherwise.
  - It refuses to run while the session is locked, and it puts the user's
    Klipper clipboard back afterwards.
  - **The AppImage passes 14/14:**

    | Check | Result |
    |---|---|
    | backend | `GdkWaylandDisplay (wayland-0)`: native Wayland, not XWayland |
    | frame | identical to the headless capture |
    | caption move | moved exactly (−200, +120) |
    | edge resize | +150 px, laid out again |
    | maximize / restore | a caption double-click fills the 3440×1396 work area; a second one restores |
    | wheel, keys | work |
    | clipboard | Klipper receives the copy |
    | paste | from Klipper into the find bar |
    | minimize, close | from Tinta's caption buttons |
- **`tests/linux_render/scale_check.py TINTA DOC --scale N`** checks
  fractional scales. It runs a nested KWin on a virtual output, with its
  own socket, D-Bus session and XDG folders.
  - The scale is set with `kscreen-doctor`, because the virtual backend
    ignores `--scale`.
  - Its first version shared the user's XDG folders. KWin then wrote
    `~/.config/kwinoutputconfig.json` and `kwinrc` and applied a stored
    `Virtual-0` output at scale 1. The live displays' entries were
    unaffected. Backups taken afterwards are at
    `~/.config/{kwinoutputconfig.json,kwinrc}.bak-nested-kwin-20261014-2121`,
    and the check has used private folders since.
  - **At 1.25, 1.5 and 2 it passes 3/3 each.** Tinta is on
    `GdkWaylandDisplay`, GTK reports the exact scale (800×560, 666×466 and
    500×350 logical), and the window is pixel-identical to a headless
    `--render-png` at that scale, so nothing resamples it.
  - Checker check: the same 1.5× capture holds no match for 2× or 1.25×
    renders of the same size.
- **Dark-mode follow** (`tests/linux_render/follow_system_check.py`).
  - How it works:
    - `RegGetValueW(...Themes\Personalize, AppsUseLightTheme)` answers from
      the XDG desktop portal's `org.freedesktop.appearance color-scheme`,
      which both KDE and GNOME serve.
    - The portal's `SettingChanged` signal becomes
      `WM_SETTINGCHANGE("ImmersiveColorSet")`, which Tinta already handles.
  - The test runs a fake portal on a private bus and Tinta on Xvfb, with
    `followSystemTheme=1`, light = Paper and dark = Wombat.
    - Dark at launch opens in Wombat (36,36,36).
    - Switching to light repaints in Paper (245,241,232).
    - Switching back to dark returns to Wombat.
    - 3/3 pass.
  - Mutations: without the portal read, start-dark and to-dark fail;
    without the subscription, to-light fails.
- **Files dropped from another application.** A `GtkDropTarget` for file
  lists on `WS_EX_ACCEPTFILES` windows posts `WM_DROPFILES`, with an HDROP
  serving `DragQueryFileW` and `DragFinish`. Tinta opens the files in tabs.
  - `gui_check.py` builds `drag_source.c` (a GTK file-manager stand-in) and
    drags `markdown-regression-control.md` onto Tinta over XDND. The title
    goes from "Tinta - gtk-window.md" to
    "Tinta - markdown-regression-control.md". `gui_check` is now 9/9.
  - With the drop target disabled, that check fails.
- **`TINTA_TRACE_GTK=1`** logs the GDK display type and, per resize, the
  scale and pixel size. The desktop and scale checks rely on it.
- **Single instance** (`tests/linux_render/instance_check.py`, on a private
  bus and Xvfb).
  - How it works:
    - Every interactive Tinta queues for `org.tinta.Instance` on the session
      bus.
    - In a later process, `FindWindowW(L"Tinta")` returns a stand-in `HWND`
      (invisible, without a rectangle) whose `WM_COPYDATA` becomes a D-Bus
      call. The owner re-sends it to its window.
    - The launcher's `XDG_ACTIVATION_TOKEN` comes along so the window may
      take focus. The desktop file now says `StartupNotify=true`.
    - Copies that arrive inside a dialog's nested loop wait for it to end.
  - Results, 4/4:
    - A second launch exits 0 in 64 ms, and the first window's title names
      the new file.
    - A `--render-png` run while a window is up is not routed.
    - `--cascade` still opens its own window.
    - After the owner exits, the next launch takes over.
  - Mutation: without the remote detection, the second launch opens its own
    window and "joins" fails.
  - `gui_check.py` now runs under `dbus-run-session` too, so that tests
    never join, or host, the user's Tinta.
- **`--register` and the first-run "default viewer?" prompt**
  (`desktop_integration.cpp`; ctest `linux_register`, in private XDG
  folders). They write:
  - `applications/tinta.desktop`, whose Exec is the AppImage (`$APPIMAGE`)
    or the binary, quoted per the Desktop Entry spec
  - hicolor icons
  - a `text/vnd.mermaid` package for `*.mmd`, then refresh the MIME and
    desktop databases
  - Tinta as the default for `text/markdown`, `text/x-markdown` and
    `text/vnd.mermaid` in `mimeapps.list`, keeping the user's other entries
    and comments

  The check passes, and GLib's `gio mime text/markdown` names
  `tinta.desktop`. Without the `mimeapps.list` write it fails. The prompt
  and confirmation use Linux wording (`fileassoc.linux.*`, in English,
  Chinese, Japanese, Korean, German, French and Italian), and the Windows
  "open Settings" step is skipped.

## Not covered yet

- **Real-GPU time to first frame on Plasma.** The desktop check's map time
  includes KWin queries.
- **IME beyond dead keys.** Each window has a `GtkIMMulticontext`.
  - Plain keys pass through it first. A commit of the key's own character
    changes nothing (`WM_KEYDOWN`, then `WM_CHAR` via `TranslateMessage`).
  - Composed text becomes `WM_CHAR`, and a key kept for a composition sends
    nothing.
  - Commits outside key handling (IBus/Fcitx, Wayland text-input) become
    `WM_CHAR`.
  - `preedit-start` posts `WM_IME_STARTCOMPOSITION`, and Tinta's
    `ImmSetCompositionWindow` caret moves GTK's candidate window.

  `gui_check`'s `dead-keys` check (US-international on its Xvfb) types
  dead_acute + e into the find field and reads back `é`. Without the input
  method step it reads `e`. Not driven here: an IBus or Fcitx session for
  CJK, and inline preedit drawing (Tinta shows the text once committed).
- **Clipboard:** only text crosses to the desktop, not HTML, PNG or TSV.
  OLE clipboard snapshots stay in-process.
- **Not wired yet:** the tab-drag ghost and tear-off windows (phase 4), and
  print dialogs.
- **GtkFileDialog and GtkAlertDialog** are written but not driven by the
  checker (no MessageBox path in the fixture, and Xvfb has no portal).
- **The AppImage's system floor**, measured with `objdump -T` over every
  bundled ELF: glibc ≥ 2.38 and the `libstdc++` of GCC 13 or later
  (`GLIBCXX_3.4.32`). Both stay with the host, per linuxdeploy's excludelist.
  Ubuntu 24.04, Debian 13 and Fedora 39 and later meet it; Ubuntu 22.04 does
  not. A build on an older base would widen that.
  Mermaid still uses the native parsers (merman is not wired into layout
  yet, MM3), so the AppImage is built with `TINTA_MERMAN=OFF`.
