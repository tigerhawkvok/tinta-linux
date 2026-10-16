# Tinta on Linux: port plan

Status (2026-10-16). Both platforms ship from this branch: the latest release is **v4.0.5**
(`eab2aef`) on `tigerhawkvok/tinta-linux`, an AppImage plus a cross-built portable `tinta.exe`.

| Area | State |
|---|---|
| Phases 0–2 | done |
| Phase 3 | done except a recorded IME session |
| Phase 4 | done |
| Phase 5 | done: printing goes through GTK's print dialog |
| Phase 6 | AppImage, plus a locally built Flatpak |
| merman | MM1, MM2, MM3 and MM5 done; MM4 became search over diagram source |

Decisions: §10 (2026-10-13/14) and §10.12 (2026-10-15); hard fork since 2026-10-14. Basis: tinta
`c58981a` (3.8.0; identical to upstream `oipoistar/tinta` master), md-viewer `45cb36a` (0.3.0 +
edit toggle).

Goal: Tinta's UX (tabs, unified editor, annotations, native Mermaid and math,
exports, themes) running natively on Linux (Wayland and X11). From md-viewer we
take its Linux reach, its packaging, and its Mermaid coverage: a ```` ```mermaid ````
fence that renders there should render in Tinta too. Startup and footprint
should stay close to Tinta's.

## 1. Measurements behind the plan

| Fact | Number | How measured |
|---|---|---|
| Tinta source | 56.6k lines C++17 (`src/` + `include/`) | `wc -l` |
| No platform API at all | 15.6k lines: md4c glue, `markdown`, all 13 `mermaid*` files, `math_parser`, `frontmatter`, `footnotes`, `document`, `wordcount` | grep for D2D/DWrite/WIC/Win32 symbols per file |
| D2D/DWrite/WIC surface | 123 distinct methods, 1,706 call sites; the top 40 methods cover 1,540 (90%) | `grep -o '->Method('` |
| Win32 surface | dominated by `InvalidateRect` 289, `SetCursor` 65, timers 43, capture 55, `GetKeyState` 33 | same |
| Render pipeline | layout builds a retained display list (`layoutTextRuns/Rects/Lines/Shapes/Connectors`), and painting walks it | `include/app.h:1040-1110`, `src/render.cpp` |
| Process model | one `App` per process (`g_app`); windows find each other with `FindWindowExW` + `WM_COPYDATA` | `src/main_d2d.cpp:53`, `src/tabs.cpp:2007` |
| Strings | `std::wstring` throughout; 13 surrogate-pair sites; 36 `MultiByteToWideChar`/`WideCharToMultiByte` calls in 11 files | grep |
| Upstream velocity | 233 commits since 2025-12-05; 30 in the last 30 days | `git log` |
| md-viewer | Rust/egui; `main.rs` 7.4k lines + vendored `egui_commonmark` and `epaint`; ~35 MB binary (README) | `wc -l`, README |
| md-viewer Mermaid | rendered by merman 0.3.0, a Rust port checked against Mermaid's own fixtures (merman HEAD tracks Mermaid 12.1) | `Cargo.lock`, merman README |
| Tinta Mermaid coverage, before | 272 of 548 examples in Mermaid's syntax docs render; 892 of 1,908 realistic merman fixtures. The biggest single cause: a leading `---` frontmatter block made `detectKind` return `None` | `tests/mermaid_corpus/` |
| Tinta Mermaid coverage, after M1 | 336 / 548 docs examples; 1,228 / 1,908 realistic fixtures; no regressions | same, after `7c4ed53` |
| merman coverage | 548 / 548 docs examples; 1,892 / 1,908 realistic fixtures | `merman_corpus_probe`, §6.1 |
| merman cost | +14–22 MB stripped depending on profile and math (default: ~22 MB); Rust 1.95 in the build | §6.1 |
| Mermaid upstream churn | 3 commits in 3 months touch `mermaid.cpp`/`mermaid_ext.*` | `git log` |
| GTK 4.22 startup | `gtk_init` 42 ms. Time to first painted frame: 112 ms with the `cairo` renderer, 130 ms with `ngl`, 300+ ms with the default `vulkan` | spike under Xvfb with software GL, 3–5 runs each; **real-GPU numbers unmeasured** |
| Pango throughput | `CHANGELOG.md` (43 KB): 750 layouts (Tinta-style element measure + per-line segments), 42.6k clusters, 21 ms warm / 36 ms cold (28 µs per layout) | spike `seg.c`, Pango 1.58 |

The spike programs and the commands to rerun them are in
[`spikes/`](spikes/README.md).

## 2. Decisions

### 2.1 Port Tinta rather than grow md-viewer

Tinta already has the UX we want. Bringing it to md-viewer would mean writing
the multi-caret editor, annotations, tab tear-off, print/DOCX/HTML exports and
the 22 native Mermaid families again in Rust/egui, on an immediate-mode
framework that md-viewer had to fork twice (`egui_commonmark`, `epaint`) to get
acceptable typography. What we take from md-viewer is knowledge: its Linux
gotchas (`docs/LESSONS.md`, devlogs 050–052 on snap and Wayland), its packaging
(`flatpak/`, `aur/`, `snap/`, `data/*.desktop`, metainfo), and its Mermaid
coverage as the bar to meet (§6).

### 2.2 A compatibility layer instead of a rewrite

Upstream ships about 30 commits a month. If the port rewrites the ~20k lines
of rendering code against a new abstraction, it is out of date within weeks,
and every later upstream feature has to be ported by hand.

> **2026-10-14: hard fork.** Nothing goes upstream, so staying close to
> upstream is no longer a goal (§8, §10.2). The compat layer stays, because it
> works and keeps the port small, but shared code is now changed freely
> wherever that is the cleaner fix. The diff budget below is retired; read the
> rest of this section as the history of how the layer came about.

So the port keeps the shared code byte-identical to upstream wherever
possible:

- **Linux-only compat headers** (`platform/linux/include/{windows.h,d2d1.h,d2d1_1.h,dwrite.h,wincodec.h,wrl/client.h,...}`)
  implement only the subset Tinta uses, on Cairo, Pango and GTK4. On Linux,
  `#include <d2d1.h>` resolves to them; Windows never sees them.
- **A real platform layer** where emulation would be dishonest: the entry
  point, window chrome, IME, multi-window, print jobs, dialogs, clipboard
  reads (§4). These become a short, enumerated list of `#ifdef _WIN32`
  islands, tracked in `docs/linux-port/IFDEFS.md`.
- **Diff budget:** an upstream merge should conflict only in `CMakeLists.txt`
  and the island list. The Linux build turns any new Win32/D2D call upstream
  adds into a compile error, which we fix by extending the shim.

Rejected alternative: a clean `gfx::`/`plat::` abstraction implemented by both
D2D and Cairo. It is better code, but it touches every render call site, so
it only makes sense if upstream adopts it. If upstream turns out to be
interested (§8), the shim's API is already a precise spec for that
abstraction.

### 2.3 GTK4 + Cairo + Pango (+ HarfBuzz, fontconfig, gdk-pixbuf, librsvg, GIO)

- Pango is the Linux counterpart of DirectWrite. It provides shaping, bidi,
  UAX#14 breaks (`pango_get_log_attrs` replaces `AnalyzeLineBreakpoints`), font
  fallback, hit testing (`xy_to_index`/`index_to_pos`), cluster iteration and
  ellipsizing. Cairo covers every D2D primitive in use: rects, rounded rects,
  ellipses, paths, arcs, dashes, transforms, axis-aligned clips, bitmaps and
  color fonts.
- One Cairo backend serves four targets: the screen (`GtkDrawingArea`),
  offscreen PNG, PDF (`cairo_pdf_surface` with `CAIRO_TAG_LINK` for links), and
  print (`GtkPrintOperation`). Screen, PDF and print therefore come out of the
  same drawing code.
- GTK4 also provides Wayland/X11, IME (`GtkIMContext`), clipboard, DnD,
  portal file dialogs, D-Bus, file monitoring and HiDPI/fractional scaling.
- Everything is already installed on this desktop: gtk4 4.22.4, cairo 1.18.4,
  pango 1.58.2, harfbuzz 14.3.1, librsvg 2.62.1. Linking dynamically should keep
  the binary near Windows Tinta's ~2.5 MB; **this is unmeasured** until the
  first link.
- Rejected: SDL3 + Skia (no text layout engine, weak IME, no printing, and
  Skia's build cost); Qt6 (heavy runtime, slower startup, and it would never
  be shared with Windows); egui (see 2.1).
- Watch item: Cairo rasterizes on the CPU. A 12-paragraph draw took ~3 ms in
  the spike. Full-window paint time at 4K is **unmeasured**. If it goes over
  8 ms, move the screen path to `GtkSnapshot`/GSK render nodes (GPU glyph
  cache) behind the same shim. Export stays on Cairo.

### 2.4 Mermaid: merman replaces the native parsers

Decided 2026-10-13 after measuring both. merman renders every Mermaid docs
example; the native parsers render 61% even after fixes. The costs are build
and size: Rust in the build, and ~22 MB added to the binary (ruled
acceptable). Design and
numbers: §6.

## 3. Architecture

As built (2026-10-16; about 8k lines in `platform/linux/src/`):

```
shared Tinta code: App, render, overlays, editor, input, tabs, exports, merman_engine...
        |  D2D / DWrite / WIC / Win32 subset (as the shared code calls it)
platform/linux/include/   compat headers: types, COM-ish refcounting, D2D1:: helpers,
                          tinta_prelude.h + msvc_crt.h (MSVC CRT spellings, force-included)
platform/linux/src/
  d2d_cairo.cpp           ID2D1RenderTarget/DeviceContext, brushes, strokes, paths, bitmaps
  dwrite_pango.cpp        IDWriteFactory/TextFormat/TextLayout/FontFallback on Pango, UAX#14
  wic_imaging.cpp         IWIC decode and encode (gdk-pixbuf), scaling; SVG through librsvg
  win32_window.cpp        HWNDs, messages, timers, capture, clipboard (local + desktop)
  win32_system.cpp        files, registry-free settings, ShellExecute, WinHTTP over libcurl,
                          locale, known folders (XDG)
  gtk_window.cpp          GTK4 windows: events to WM_*, IME, file and tab drops, tab drags,
                          clipboard reads, dialogs, single instance and colour scheme on D-Bus
  desktop_integration.cpp --register: .desktop file, icons, .mmd MIME type, defaults
  main_linux.cpp, win32_base.cpp, win32_input.cpp, com_streams.cpp
```

**Events.** The existing `WndProc` and the `handleMouse*`/`handleKey*`
functions compile unchanged. `main_gtk.cpp` turns GTK4 controller events into
the messages those functions already handle:

- key: GDK keyval to `VK_*`
- IME commit: `WM_CHAR`
- click, motion, scroll: `WM_*BUTTON*`, `WM_MOUSEMOVE`, `WM_MOUSEWHEEL`
  (precise touchpad deltas accumulated to 120 units)
- draw: `WM_PAINT`; resize: `WM_SIZE`; close-request: `WM_CLOSE`
- `SetTimer`/`PostMessage`: `g_timeout_add`/`g_idle_add`, which are
  thread-safe, so the async image, layout-chunk and folder-search workers keep
  posting as they do today

**Strings.** `std::wstring` stays. On Linux `wchar_t` is UTF-32, so the shim
indexes text in *wstring units* (UTF-16 on Windows, code points on Linux) and
maps to Pango's UTF-8 byte offsets internally. Shared code's offsets stay
self-consistent on both platforms. The 13 surrogate-pair branches become dead
code on Linux. Tests that hard-code UTF-16 lengths for astral characters need
expectations that depend on the platform.

**Fonts.** `CreateTextFormat` resolves Windows family names through an alias
table, so themes and `themes.ini` stay unchanged:

| Windows name | Linux resolution order |
|---|---|
| Segoe UI (36 refs) | the system UI font (`sans-serif`) |
| Consolas, Cascadia Code, Cascadia Mono | Cascadia Code → JetBrains Mono → DejaVu Sans Mono → `monospace` |
| Cambria Math | Cambria Math → STIX Two Math → Latin Modern Math → Noto Sans Math → DejaVu Math TeX Gyre |
| Yu Gothic UI, Microsoft YaHei UI, Malgun Gothic, JhengHei | Noto Sans CJK JP / SC / KR / TC |
| Segoe UI Emoji, Segoe UI Symbol | Noto Color Emoji, then fontconfig fallback |

**Process model.** One window per process, as on Windows, so `g_app` stays
valid.

- **Single instance** (`a86d94b`): the first interactive Tinta owns
  `org.tinta.Instance` on the session bus. `FindWindowW(L"Tinta")` in a later
  process returns a stand-in whose `WM_COPYDATA` travels there, so a plain
  `tinta file.md` joins the running window as a tab. `--cascade` and `--new`
  windows stay separate processes.
- **Raising on Wayland:** the launching process forwards its
  `XDG_ACTIVATION_TOKEN`.
- **Tabs between windows** go through the desktop's drag and drop, not
  D-Bus (§4).

## 4. Where emulation isn't enough

| Windows mechanism | Linux replacement | Kind |
|---|---|---|
| `WM_NCHITTEST` custom title bar with tabs | undecorated window; the same hit-test feeds `gdk_toplevel_begin_move`/`begin_resize`; Tinta draws its own window buttons (GNOME requires client-side decorations anyway) | island in `main_d2d.cpp` |
| IMM32 composition (`WM_IME_*`, `ImmSetCompositionWindow`) | `GtkIMContext` preedit, commit and `set_cursor_location` (IBus and Fcitx5) | island in `editor.cpp` |
| Synchronous clipboard reads, `MessageBoxW`, `GetOpenFileNameW`/`GetSaveFileNameW` | GTK4 equivalents are async. The shim spins a nested main loop until the result arrives, like GTK3's `gtk_clipboard_wait_for_text`. Re-entrancy is the risk | shim |
| Tab tear-off ghost (`UpdateLayeredWindow`, `GetCursorPos`) and cross-window drop | **done** (`dcdd21b`): GTK DnD with an `application/x-tinta-tab` payload; the drag icon is the tab card (Cairo/Pango); a `NO_TARGET` drop tears off a `--cascade` window; the source's own strip takes the tab back | island in `tabs.cpp` + `gtk_window.cpp` |
| Pin on top (`HWND_TOPMOST`) | **removed** on both platforms (`566b287`, Philip's call) | n/a |
| `--pos`, `--cascade`, `SetWindowPlacement`, opening a new window at the drop point | works on X11; on Wayland the compositor places windows and only size is kept | degrade, documented |
| `ID2D1PrintControl` + XPS print package | **done**: Print exports the same pages to a temporary PDF (`cairo_pdf_surface`) and GTK's print dialog prints it (`gtk_print_dialog_print_file`; the print portal in a sandbox). One drawing path serves PDF and print | island in `print.cpp` |
| WinHttp update check | **done**: the compat WinHTTP runs over libcurl, so the check and Check now work in every build | shim |
| Registry: `/register`, light/dark mode | `xdg-mime default` on a shipped `.desktop` file; portal `org.freedesktop.appearance color-scheme` | island |
| `%APPDATA%\Tinta`, portable `settings.ini` next to the exe | `$XDG_CONFIG_HOME/tinta`; portable mode when `settings.ini` sits next to the binary or `$APPIMAGE` | `config_dir.cpp` |
| `ShellExecute` (URLs, reveal in Explorer) | `gtk_uri_launcher`; `org.freedesktop.FileManager1.ShowItems` | shim |
| Polling `GetFileTime` for reload and live annotations | `GFileMonitor` (inotify) | shim |

## 5. Phases

Each phase ends in a state that can be shipped or demonstrated. Durations are
**unmeasured**. After phase 1, the shim's size and real velocity will give a
basis for estimating the rest.

**Phase 0: Linux build of the pure core. Done (`32b17f8`).**

- `CMakeLists.txt` gains a six-line hook after `SOURCES`/`HEADERS`: non-Windows
  hosts include `platform/linux/linux.cmake` and stop there.
- `platform/linux/include/windows.h` is the start of the shim: types, `VK_*`,
  `CP_UTF8` conversion, `GetStringTypeW` (via GLib), `SYSTEMTIME`/`FILETIME`
  and `GetKeyState`.
- A force-included `tinta_prelude.h` supplies MSVC's CRT spellings (`_stricmp`,
  `swprintf_s`, ...) and the standard headers MSVC includes transitively, so
  no shared file changed.
- `keymap`, `math_parser`, `mermaid_parser`, `document_types` and `word_count`
  pass under GCC 15.2 and Clang 22.1. `footnote_tests` needs the full app
  sources, so it waits for phase 1.
- CI is deferred while all work stays local; the release workflow is
  untouched.

**Phase 1: headless rendering. Done (2026-10-14); record in
`tests/linux-headless-validation.md`.** All 49 fixtures render to PNG and
PDF headless; 42/42 tests pass, including every upstream layout and input
suite with no tolerances (after Selawik, §10.10). The Windows reference
renders were not made: there is no Windows machine in the loop. The
original plan follows.

Build `gfx_cairo`,
`text_pango` and `imaging`, wire up the existing `--exportpdf`/`--printpages`
CLI paths, and add `--render-png out.png --width N --theme T`. Link md4c
statically: FetchContent currently builds it as `libmd4c.so` on Linux.
*Exit:*
- Every file in `tests/fixtures/` renders to PNG and PDF headless.
- The layout suites (`code_block_layout`, `blockquote_layout`,
  `table_layout`, `math_layout`, `inline_style`, `theme`) pass. Each assertion
  is classified first as relative (must pass as-is) or DirectWrite-absolute
  (gets a documented tolerance).
- PNGs are reviewed side by side with Windows reference renders of the same
  fixtures, produced by the existing `render_*.ps1` harnesses on a Windows
  machine (or on a Windows CI runner, if D2D renders correctly there; that is
  untested).

**Phase 2: interactive viewer. Done.** `main_gtk.cpp`, event synthesis, timers,
cursors, custom title bar, HiDPI/fractional scale, dark-mode follow, XDG
config, drag-and-drop in, clipboard writes (text, HTML, PNG, TSV), open
dialog, URL launch, reveal-in-folder, file monitor. Measure time to first
frame on real hardware, and pin `GSK_RENDERER` in-process if Vulkan init
dominates (a userspace setting; no driver changes).
*Exit:* `input_tests` and the viewer-side `run_*_tests.cmake` suites run on
Linux; the README's viewer feature list is checked by hand on GNOME/Wayland
and X11, and the results are recorded.
*Status (2026-10-14):* the minimum is in (`platform/linux/src/gtk_window.cpp`):
- window, present and input
- custom caption, `DefWindowProc` moves and resizes
- HiDPI through `WM_DPICHANGED`
- text clipboard, GtkFileDialog and GtkAlertDialog

`tests/linux_render/gui_check.py` checks it on Xvfb (ctest `linux_gtk_window`);
see `tests/linux-gtk-window-validation.md`.

*Status (2026-10-16):*
- **Done since then:**
  - drag-and-drop in (`0d1574c`) and dark-mode follow through the portal;
  - a live-session pass on KDE Plasma Wayland: `desktop_check.py`, 14/14;
  - fractional scales checked pixel-exact in a nested KWin.
- **Rich clipboard formats were dropped** (§10.12): copying from the reader
  returns the Markdown behind the selection (`93ffb00`).

**Phase 3: editor and IME. Done, except the recorded IME session.**
`GtkIMContext` landed in `c506242` (dead keys checked on Xvfb). Screenshot
paste is in `c70a4ef`: assets folder, inline or Save as, with a remembered
choice. The original plan follows. `GtkIMContext`, paste (text and screenshot
bitmaps), save and save-as dialogs, drafts, multi-caret. Alt+click and
Alt-drag collide with window-manager move bindings on some desktops (KDE,
Xfce); document this and keep it rebindable through the existing keymap.
*Exit:* the editor suites (`editor_features`, `editor_context`,
`wrapped_cursor`, `table_input`) pass; a manual IME session with Fcitx5 or
IBus (Japanese, Chinese) is recorded.

**Phase 4: tabs, multiple windows, single instance. Done** (single instance
`a86d94b`; tab tear-off and drags between windows `dcdd21b`, ctest
`linux_tab_drag`). Not yet tried: a two-window drag on Philip's KDE Wayland
session. The original plan follows. The D-Bus peer
registry, "open in tabs" from the file manager, tear-off and cross-process
tab moves through DnD, and activation tokens.
*Exit:* `tab_drop`, `tab_hover_card` and `window_drag` suites pass; a manual
two-window drag is recorded on GNOME/Wayland and KDE/X11.

**Phase 5: print, export, pandoc.** *Status:*
- PDF, HTML and DOCX export work.
- Pandoc export's temp path is fixed (`9e19259`).
- Printing is done (`tests/linux-print-validation.md`): the system print
  dialog prints the PDF Tinta exports.

The original plan follows. GTK print, Cairo PDF with links (rerun
`tests/validate_pdf_links.py` on Linux output), DOCX image encoding through
Cairo PNG, pandoc launched with `g_spawn`.
*Exit:* `print_and_word_export` passes; the PDF link validator passes.

**Phase 6: packaging.** Ship an AppImage (the analog of the portable
`tinta.exe`), Flatpak (Flathub, starting from md-viewer's manifest), AUR
`-git`/`-bin`, and a `.deb` via CPack. Install the `.desktop` file, AppStream
metainfo, hicolor icons from `tinta.ico`, and a shared-mime-info entry for
`.mmd` (no standard type exists). `--register` maps to `xdg-mime`.
*Status (2026-10-14):* the AppImage came first, ahead of phases 3–5, as the
first usable package (Philip's priority). AppImage over Flatpak and Snap
because:
- it is one file to download and run, like the portable `tinta.exe`
- it needs no runtime, store or daemon
- it builds locally with no CI
- Flatpak (Flathub review) and Snap (snapd) both need infrastructure
  this fork does not have yet

`platform/linux/appimage/build-appimage.sh BUILD OUT` builds
`Tinta-x86_64.AppImage` from a system-library tree with pinned linuxdeploy
and its GTK plugin. The `.desktop` file and hicolor icons are in, and
`--register` adds the `.mmd` MIME type.

*Status (2026-10-16):*
- **Releases:** v4.0.1–v4.0.5 are GitHub releases with the AppImage, the
  exe, `SHA256SUMS` and, from 4.0.5, the third-party notices.
- **Flatpak** (Philip: "Flatpak for sure, others optional"):
  `packaging/flatpak/` has the manifest
  (`io.github.tigerhawkvok.Tinta`, GNOME 50 runtime, Rust from the SDK
  extension), AppStream metainfo, and `build-flatpak.sh`. It is a local
  build: md4c, Corrosion and merman's crates download during the build, so
  Flathub would need them vendored.
- **AUR and `.deb`:** optional.

**Windows from Linux (2026-10-14).** `platform/windows/build-windows.sh`
cross-compiles the portable `tinta.exe` with clang-cl and lld-link against
the MSVC CRT and Windows SDK from xwin. Philip accepted Microsoft's license;
the script asks for `TINTA_ACCEPT_MSVC_LICENSE=1` before the first download.
The shared code builds unchanged, with a static CRT and only Windows DLLs
imported. Under Wine, the window renders and `--exporthtml` works.
Rendering fidelity, print and PDF still need a real Windows machine. Record:
`tests/windows-cross-build-validation.md`. With a Windows toolchain on
Linux, MM5's merman build could also be cross-compiled
(`x86_64-pc-windows-msvc`), but its DirectWrite checks still need Windows.

**Mermaid track (§6), alongside the phases.** Done (§6.3).

**Later: macOS.** GTK4 runs there, so the same backend would work, with a
non-native feel. A CoreText/CoreGraphics backend behind the same compat
headers is the native option. Not scheduled.

## 6. Mermaid: merman replaces the native parsers

Decided 2026-10-13. A fence md-viewer renders should render in Tinta, and
[merman](https://github.com/Latias94/merman), the Rust Mermaid port md-viewer
uses, gets there directly. The measurements below shaped the design, not the
decision.

### 6.1 Measured (Linux; merman 0.9.0 at `3a4b8c3`, Rust 1.95)

| | Tinta native (after M1) | merman |
|---|---|---|
| Mermaid docs examples (548) | 336 | **548** |
| Realistic merman fixtures (1,908) | 1,228 | **1,892** through the wrapper (1,895 raw). 6 need the `math` capability (not built); 7 are docs fragments containing a literal `...`, which Mermaid also rejects; 3 are `info` diagrams with no `viewBox` |
| Native draws, merman rejects | n/a | 12 of 3,796: 6 math labels, 3 lenient syntax, 2 mindmap stress cases, 1 shape Mermaid lacks |
| Composite-state diagram with notes (a real README) | code fallback | renders |
| SVG render time, realistic fixtures | n/a | p50 2.3 ms, p95 8 ms, max 68 ms |
| Engine creation | n/a | 2–3 ms |
| Added size, stripped static link | n/a | 14–24 MB depending on profile (table below) |
| Of the 14.1 MB `z` build | n/a | 8.7 MB code, 2.7 MB unwind tables (kept: a panic must not abort Tinta), 1.2 MB data |
| Build | C++ only | adds Rust 1.95 via rustup (1.4 GB toolchain); ~70 s cold build of merman |

Cargo profile against size and SVG time. SVG-only unless noted; times are
over the 1,908 realistic fixtures:

| Profile | Added size (stripped) | p50 | p95 | max |
|---|---|---|---|---|
| `release` default (opt 3), with PNG | 23.7 MB | 2.3 ms | 8.0 ms | 68 ms |
| opt 2, fat LTO | 17.6 MB | 2.4 ms | 8.6 ms | 79 ms |
| opt `s`, fat LTO | 15.4 MB | 3.2 ms | 11.5 ms | 125 ms |
| **opt 3, fat LTO, with `math`** (current default) | **22.4 MB** (test binary) | n/a | n/a | n/a |
| opt `z`, fat LTO | 14.1 MB | 5.1 ms | 20.7 ms | 235 ms |
| opt `z`, fat LTO, with PNG (the Windows set) | 16.7 MB | n/a | n/a | n/a |

Rendering costs for one large diagram (2,091 × 1,947 px):

| Path | 1× | 2× |
|---|---|---|
| SVG through librsvg, merman's drop shadows on | 1,000 ms | 2,200 ms |
| SVG through librsvg, shadows off | **10 ms** | **37 ms** |
| merman's own PNG (resvg), shadows off | 98 ms | 277 ms |

### 6.2 Design

- **Build.** CMake builds the `merman-ffi` static library from a pinned merman
  checkout (FetchContent at `3a4b8c3`), through Corrosion or a cargo custom
  command.
  - Features: `all-diagrams,svg,layout-cytoscape`, plus `png` on Windows.
  - `math` is included. `layout-elk` (EPL-2.0) is not.
  - Profile: `opt-level=3`, fat LTO, `panic=unwind`. Speed over size: size
    was ruled not a concern on 2026-10-13, since anything under ~100 MB is
    invisible on disk.
- **Switch.** A CMake option `TINTA_MERMAN`, ON by default in this fork.
  - OFF builds upstream's native renderer exactly as today. That keeps a build
    with no Rust and keeps upstream merges cheap.
  - When ON, the native `mermaid*.cpp` stay linked as the fallback. The viewer
    and export call sites get `#if` islands (§4).
- **Shared wrapper** `merman_engine.{h,cpp}`, platform-free:
  - one lazily created engine;
  - `renderSvg(source, theme)` returns the SVG, its size (from the `viewBox`:
    Mermaid writes `width="100%"`) and label boxes;
  - a failure falls back to the **native renderer**, then to the code block
    captioned with merman's message. The corpus showed 12 diagrams native
    draws and merman rejects (6 of them math labels), so native stays linked
    as a safety net: nothing that renders today stops rendering.
- **Options.**
  - `svg.pipeline = resvg-safe`, so there is no `foreignObject`.
  - Drop shadows off via `site_config.themeCSS`: 100× faster in librsvg, and
    Tinta's diagrams are flat anyway.
  - `presentation.theme` roles, series palette, font family and size come from
    the active Tinta theme. `themes.cpp` already resolves diagram roles.
  - A diagram's own frontmatter or `%%{init}%%` still overrides these, as in
    Mermaid.
- **Text measurement** (MM2, `66684c7`). merman's default metrics are
  font-agnostic, and labels overflowed their shapes with them, even in the
  default theme.
  - The third Tinta patch on merman (`merman-tinta-text-width.patch`) adds
    a process-wide *line-width* hook. Requests then use
    `DeterministicTextMeasurer::with_width_callback`: merman keeps its own
    wrapping and line heights and takes Tinta's widths.
  - `render.cpp` measures with DirectWrite, which is Pango on Linux, in the
    first installed family of the label's CSS list.
  - The 22-operation host ABI was set aside, because using it would mean
    copying merman's wrapping into C++.
  - Record: `tests/merman-label-width-validation.md`.
- **Display, Linux.** librsvg onto Cairo.
  - Rasterize once per diagram per zoom × scale, then blit on each paint.
  - Print and PDF draw the SVG as vectors.
- **Display, Windows.** Direct2D's SVG renderer ignores `<text>`, `<marker>`
  and `<style>` (Microsoft's "SVG Support" page), so it can't draw merman
  output.
  - merman's PNG (resvg) becomes a D2D bitmap, rendered off the UI thread
    like the async image path.
  - The size comes from the SVG first, so layout doesn't jump when the bitmap
    lands.
- **Search** (`85690a4`, in place of MM4's label rects). Philip asked why
  search couldn't read the Mermaid code behind a diagram, and now it does:
  matches in a diagram's source outline the diagram, and the results list
  quotes the source line.
- **Exports.**
  - HTML: inline SVG with the `parity` pipeline, which browsers render
    natively.
  - DOCX: PNG at 2×.
  - PDF and print: vector on Linux, a high-resolution bitmap on Windows.
  - Copy as image: PNG at 2×.
- **Math labels.** These are on: the `math` feature uses RaTeX, which is MIT
  with OFL-1.1 KaTeX fonts.
  - RaTeX puts formulas inside HTML `foreignObject` labels, which
    `resvg-safe` drops. So `$$` sources render twice, and `merman_engine`
    lifts each math-only formula from the `parity` SVG onto the safe one
    by absolute position, with an inline theme-colour style.
  - Labels mixing text and math keep only their text.
  - Details: `tests/mermaid-merman-validation.md`.
- **Licences.** merman's `rust-render-base` scope is MIT, ISC, BSD-3,
  Apache-2.0 and DOMPurify's Apache-2.0-or-MPL-2.0.
  - **Shipped** (`d48c8ce`): `packaging/THIRD-PARTY-NOTICES.md`, generated
    by `packaging/notices/generate_notices.py`.
  - It is embedded in both binaries and opened from Settings → Updates →
    Licenses.
  - ctest `notices_current` keeps it in step with the pinned merman.

### 6.3 Steps

| Step | What | Needs |
|---|---|---|
| MM1 | CMake + merman build + `merman_engine` wrapper + tests; corpus harness gains a merman mode as the regression gate | **done** (`2a8ad90`, `tests/mermaid-merman-validation.md`) |
| MM2 | Label widths from the face that draws them | **done** (`66684c7`): a line-width hook patched into merman, measured through DirectWrite/Pango |
| MM3 | Hook into `layoutCodeBlock`, HTML and DOCX export behind `TINTA_MERMAN`, with merman → native → code fallback | **done** (2026-10-14): viewer, print, PDF, HTML (parity SVG) and DOCX (PNG) |
| MM4 | Search and selection rects from the SVG labels | **replaced** by search over the diagram's source (`85690a4`) |
| MM5 | Windows: MSVC build of merman, DirectWrite measurement, async PNG | **done**: drawing on Philip's Windows laptop since 4.0.1. Font-scan fix (`8f644d9`, first raster 3,054 → 166 ms); math labels through a lifted SVG raster (`1bf6b50`); measurement shared with MM2. Async rendering is not done (renders are synchronous) |

What this supersedes:

- M2–M5, the native parser fixes, are dropped.
- M1 (`7c4ed53`) and `tests/mermaid_corpus/` stay. They serve the
  `TINTA_MERMAN=OFF` build and give the before/after numbers.

## 7. Validation (AGENTS.md on Linux)

- Fixtures go in `tests/fixtures/` as mixed documents, as AGENTS.md already
  requires. New `linux-*` fixtures cover font fallback, CJK, emoji, RTL,
  math-font fallback and HiDPI.
- Headless rendering means fixtures render to PNG and PDF with **no
  display**, at normal and narrow widths, across all 10 themes. Output goes to
  `build/out/` (ignored).
- `input_tests` keep their approach (a message-only window becomes a hidden
  App on an offscreen Cairo target) and drive the real `WndProc` without a
  compositor.
- Mermaid parser changes run `tests/mermaid_corpus/` before and after;
  `compare_runs.py` must report no regressions.
- Interactive checks:
  - **This desktop** runs **KDE Plasma 6.4.5 on Wayland** (not GNOME), one
    3440×1440 monitor at scale 1. `desktop_check.py` drives it through
    uinput, only into an active Tinta window, and never while the session is
    locked.
  - **Repeatable checks** run on Xvfb with a private D-Bus session
    (`gui_check.py`, `instance_check.py`, `tab_drag_check.py`).
  - **Windows checks** run under Wine on Xvfb, with Philip's laptop for the
    real thing.
  - Each feature records what was checked and what is still limited, in a
    `tests/*-validation.md` record.

## 8. Upstream relationship

- **Hard fork (2026-10-14).** This is never submitted upstream. Breaking
  changes to shared code are fine when they serve the fork. Upstream commits
  are optional cherry-picks, not something to merge on a schedule.
- No PRs. Work is committed locally.
- Pushes and releases happen when Philip asks. On 2026-10-15 he asked for
  GitHub releases on `origin` (`tigerhawkvok/tinta-linux`), to get builds
  onto his Windows laptop, and gave push permission for that loop.
  v4.0.1–v4.0.5 went out that way.
- Shared-code fixes such as the Mermaid work also fix Windows, for this
  fork's own Windows build.

## 9. Risks, ranked

1. **Text metrics.** Pango/FreeType and DirectWrite differ in ascent/descent,
   line gap, uniform line spacing and trailing-whitespace width. Layout code
   measures rather than assumes, which helps; tests with absolute pixel
   values will need tolerances. Caught in phase 1: wrap-point whitespace and
   trailing hits now follow DirectWrite, and Selawik replaces Noto Sans as
   the Segoe UI stand-in (§10.10), after which upstream's layout suites pass
   without tolerances.
2. **Re-entrancy in the nested-loop shims** (clipboard reads, modal
   dialogs). If it bites, convert those call sites to callbacks, which adds a
   few islands.
3. **Wayland limits**: window placement, always-on-top, global cursor
   position, focus stealing. These degrade by design and are documented, not
   hidden.
4. **Upstream churn into Win32-heavy code** (`tabs.cpp`, `main_d2d.cpp`,
   `input.cpp`), where the islands live. Mitigation: keep islands small and
   merge often.
5. ~~**Wide `printf` semantics.**~~ Resolved (`9e19259`). MSVC reads `%s` in a
   wide format as a wide string and glibc as a narrow one. The compat
   `swprintf_s` now rewrites formats to MSVC's meaning, so shared code and
   `languages.ini` strings format alike. It had cut the word count to its
   first digit and sent Pandoc's temp file to `/`.
6. **Startup**: 112–130 ms to first frame measured headless, against
   Windows Tinta's advertised <100 ms. GTK init alone is 42 ms. Measure on
   real hardware in phase 2 before optimizing.
7. **merman in the build.** Rust 1.95 via rustup on every build machine (the
   Flatpak uses the SDK's Rust extension instead, `TINTA_MERMAN_RUSTUP=OFF`);
   ~70 s cold build; ~22 MB in the binary against Tinta's ~2.5 MB (size
   ruled not a concern). Pinned
   by commit. `TINTA_MERMAN=OFF` builds without it.
8. **Pango cost per layout** (28 µs warm). Fine for incremental layout of
   large documents (43 KB in 21 ms); revisit only if a fixture shows layout
   above budget.

## 10. Decisions (2026-10-13)

1. **"Inline markdown" means Mermaid fences rendered in place, as md-viewer
   does**, not an inline editing mode. This became §6; the live-preview phase
   was dropped.
2. ~~Track upstream with the compat layer, and keep the option to break
   away.~~ Superseded 2026-10-14: **hard fork**. Shared code changes freely;
   no upstream PRs, ever.
3. **macOS later.** GTK4 or a CoreText backend behind the same headers; not
   scheduled.
4. **Local only.** No PRs. Pushes and releases only at Philip's request
   (§8).
5. **merman replaces the native Mermaid parsers.** Native stays linked as
   the fallback for the corpus diagrams merman rejects (6 once math is on);
   `TINTA_MERMAN=OFF` keeps an upstream-identical build.
6. **Math in diagram labels: yes.** RaTeX is built in, with the formula
   lifting of §6.2.
7. **Binary size is not a constraint.** Anything under ~100 MB is invisible
   on disk, so profiles favour speed.
8. **Phase 1 approved.**
9. **Hard fork** (2026-10-14; see 2 and §8).
10. **Selawik ships with Tinta** (2026-10-14): Microsoft's OFL,
    Segoe-UI-metric font, in `third_party/fonts/selawik`, registered as an
    application font and first in place of Segoe UI. Tinta's layouts are
    tuned for Segoe UI; with Noto Sans in its place, a table cell at 650 px
    wrapped text that fits on Windows. It has no italic (synthesised) and no
    kerning. The OFL notice belongs in phase 6's notices.
11. **Monaspace Argon is the default code face** (2026-10-14, Philip's
    pick; his own MonoLisa is not redistributable). OFL, shipped in
    `third_party/fonts/monaspace-argon` (Regular, Italic, Bold, BoldItalic of
    v1.400). It replaces Consolas in seven built-in themes, the editor
    gutter and the edit rail; Arctic, Midnight and Abyss keep their Cascadia
    faces, which reach Argon on Linux through the stand-in stack. Since
    `ffdf099` the Windows exe carries Argon too: four faces as resources,
    loaded as a private DirectWrite collection.
12. **Philip's answers on the outstanding list (2026-10-15).**

    | Item | His answer | Outcome |
    |---|---|---|
    | Printing | "Lowest priority" | done, last: GTK's print dialog prints the exported PDF |
    | Tab tear-off | "Approved" | done (`dcdd21b`) |
    | Screenshot paste | option D, "a multi option on each paste (with a rememberable default and corresponding setting)" | done (`c70a4ef`) |
    | Rich copy formats | "Let's actively kill this. Copying formatted content should return its corresponding rendering code." | done: copy returns Markdown (`93ffb00`); the diagram Image and table TSV buttons stay |
    | Pin on top | "Let's remove this" | removed (`566b287`) |
    | Diagram search | "Why can't this search the mermaid code backing it?" | done (`85690a4`) |
    | MM2; Windows math labels; Windows bundled fonts | "Approved" | all done |
    | License notices | "it should be added" | done (`d48c8ce`) |
    | This plan | refresh "Approved" | this revision |
    | Packaging | "Flatpak for sure, others optional" | §5, phase 6 |
    | CI | "We can defer for the moment" | deferred |
    | macOS | "nice to have … after Win/Nix feature parity" | after parity |
