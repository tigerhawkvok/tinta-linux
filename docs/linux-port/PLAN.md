# Tinta on Linux: port plan

Status: proposal, 2026-10-13. Basis: tinta `c58981a` (3.8.0; identical to
upstream `oipoistar/tinta` master), md-viewer `45cb36a` (0.3.0 + edit toggle).

Goal: Tinta's UX (tabs, unified editor, annotations, native Mermaid and math,
exports, themes) running natively on Linux (Wayland and X11). From md-viewer we
take its Linux reach, its packaging, and the inline editing it tried
("live preview"). Startup and footprint should stay close to Tinta's.

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
| md-viewer inline editing | Obsidian-style block swap landed in #203 (+4.4k lines) and was reverted within 2 days; `main` now has only a Preview/Edit toggle | `git show 68a38ab`, `9b68f50` |
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
(`flatpak/`, `aur/`, `snap/`, `data/*.desktop`, metainfo), and the editing
research in #203 (`docs/research/editing-*.md` at `68a38ab`).

### 2.2 A compatibility layer instead of a rewrite

Upstream ships about 30 commits a month. If the port rewrites the ~20k lines
of rendering code against a new abstraction, it is out of date within weeks,
and every later upstream feature has to be ported by hand.

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
  and the island list. Linux CI turns any new Win32/D2D call upstream adds
  into a compile error, which we fix by extending the shim.

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

## 3. Architecture

```
shared Tinta code (upstream-identical): App, render, overlays, editor, input, tabs, exports...
        |  D2D / DWrite / WIC / Win32 subset (as upstream calls it)
platform/linux/include/   compat headers: types, COM-ish refcounting, D2D1:: helpers
platform/linux/src/
  gfx_cairo.cpp      ID2D1RenderTarget/DeviceContext, brushes, strokes, paths, bitmaps
  text_pango.cpp     IDWriteFactory/TextFormat/TextLayout/Typography/FontFallback, UAX#14
  imaging.cpp        IWIC decode (gdk-pixbuf), scale, PNG encode; SVG via librsvg
  win32_shim.cpp     timers, invalidation, cursors, capture, key state, PostMessage,
                     MultiByteToWideChar (CP_UTF8), file APIs over POSIX, ShellExecute
  main_gtk.cpp       GtkApplication, window, event to WM_* synthesis into the existing WndProc
  peers_dbus.cpp     window registry replacing FindWindowExW/WM_COPYDATA
  clipboard, dialogs, print_gtk, im_gtk, portal settings (dark mode), file monitor
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

**Process model.** Keep one window per process, mirroring Windows, so `g_app`
stays valid. Each process registers `cc.tinta.Window.<pid>` on the session bus
with `OpenFiles`, `AcceptTab` and `LastActive`. That replaces `FindWindowExW`
and `WM_COPYDATA` in `main_d2d.cpp` and `tabs.cpp`. To let a window raise
itself on Wayland, the launching process forwards its `XDG_ACTIVATION_TOKEN`
over D-Bus.

## 4. Where emulation isn't enough

| Windows mechanism | Linux replacement | Kind |
|---|---|---|
| `WM_NCHITTEST` custom title bar with tabs | undecorated window; the same hit-test feeds `gdk_toplevel_begin_move`/`begin_resize`; Tinta draws its own window buttons (GNOME requires client-side decorations anyway) | island in `main_d2d.cpp` |
| IMM32 composition (`WM_IME_*`, `ImmSetCompositionWindow`) | `GtkIMContext` preedit, commit and `set_cursor_location` (IBus and Fcitx5) | island in `editor.cpp` |
| Synchronous clipboard reads, `MessageBoxW`, `GetOpenFileNameW`/`GetSaveFileNameW` | GTK4 equivalents are async. The shim spins a nested main loop until the result arrives, like GTK3's `gtk_clipboard_wait_for_text`. Re-entrancy is the risk | shim |
| Tab tear-off ghost (`UpdateLayeredWindow`, `GetCursorPos`) and cross-window drop | GTK DnD with an `application/x-tinta-tab` payload; the drag icon is the rendered card; a `NO_TARGET` drop spawns a new window | island in `tabs.cpp` |
| Pin on top (`HWND_TOPMOST`) | X11: `_NET_WM_STATE_ABOVE`. Wayland has no protocol for it, so the pin is hidden there | island |
| `--pos`, `--cascade`, `SetWindowPlacement`, opening a new window at the drop point | works on X11; on Wayland the compositor places windows and only size is kept | degrade, documented |
| `ID2D1PrintControl` + XPS print package | `GtkPrintOperation` driving Tinta's own pagination; PDF export goes straight to `cairo_pdf_surface` | island in `print.cpp` |
| WinHttp update check | off in distro and Flatpak builds; AppImage only, via libsoup3 or a `curl` subprocess | island |
| Registry: `/register`, light/dark mode | `xdg-mime default` on a shipped `.desktop` file; portal `org.freedesktop.appearance color-scheme` | island |
| `%APPDATA%\Tinta`, portable `settings.ini` next to the exe | `$XDG_CONFIG_HOME/tinta`; portable mode when `settings.ini` sits next to the binary or `$APPIMAGE` | `config_dir.cpp` |
| `ShellExecute` (URLs, reveal in Explorer) | `gtk_uri_launcher`; `org.freedesktop.FileManager1.ShowItems` | shim |
| Polling `GetFileTime` for reload and live annotations | `GFileMonitor` (inotify) | shim |

## 5. Phases

Each phase ends in a state that can be shipped or demonstrated. Durations are
**unmeasured**. After phase 1, the shim's size and real velocity will give a
basis for estimating the rest.

**Phase 0: Linux build of the pure core.** Add a platform branch to CMake
(`if(WIN32)` keeps today's build byte-for-byte). Build md4c, markdown, mermaid,
math parser, frontmatter, footnotes, document and wordcount, plus their
existing tests (`mermaid_tests`, `math_parser_tests`, `document_tests`,
`wordcount_tests`, `footnote_tests`, `keymap_tests`) with GCC 15 and Clang 22.
Fix MSVC-isms (`_stricmp`, `_wcsicmp`, wchar width). Add an `upstream` remote
and an `ubuntu-latest` CI job, and add a `pull_request` trigger so both
platforms gate merges.
*Exit:* `ctest` green on Linux for the pure suites.

**Phase 1: headless rendering (most of the shim).** Build `gfx_cairo`,
`text_pango` and `imaging`, wire up the existing `--exportpdf`/`--printpages`
CLI paths, and add `--render-png out.png --width N --theme T`.
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

**Phase 2: interactive viewer.** `main_gtk.cpp`, event synthesis, timers,
cursors, custom title bar, HiDPI/fractional scale, dark-mode follow, XDG
config, drag-and-drop in, clipboard writes (text, HTML, PNG, TSV), open
dialog, URL launch, reveal-in-folder, file monitor. Measure time to first
frame on real hardware, and pin `GSK_RENDERER` in-process if Vulkan init
dominates (a userspace setting; no driver changes).
*Exit:* `input_tests` and the viewer-side `run_*_tests.cmake` suites run on
Linux; the README's viewer feature list is checked by hand on GNOME/Wayland
and X11, and the results are recorded.

**Phase 3: editor and IME.** `GtkIMContext`, paste (text and screenshot
bitmaps), save and save-as dialogs, drafts, multi-caret. Alt+click and
Alt-drag collide with window-manager move bindings on some desktops (KDE,
Xfce); document this and keep it rebindable through the existing keymap.
*Exit:* the editor suites (`editor_features`, `editor_context`,
`wrapped_cursor`, `table_input`) pass; a manual IME session with Fcitx5 or
IBus (Japanese, Chinese) is recorded.

**Phase 4: tabs, multiple windows, single instance.** The D-Bus peer
registry, "open in tabs" from the file manager, tear-off and cross-process
tab moves through DnD, and activation tokens.
*Exit:* `tab_drop`, `tab_hover_card` and `window_drag` suites pass; a manual
two-window drag is recorded on GNOME/Wayland and KDE/X11.

**Phase 5: print, export, pandoc.** GTK print, Cairo PDF with links (rerun
`tests/validate_pdf_links.py` on Linux output), DOCX image encoding through
Cairo PNG, pandoc launched with `g_spawn`.
*Exit:* `print_and_word_export` passes; the PDF link validator passes.

**Phase 6: packaging.** Ship an AppImage (the analog of the portable
`tinta.exe`), Flatpak (Flathub, starting from md-viewer's manifest), AUR
`-git`/`-bin`, and a `.deb` via CPack. Install the `.desktop` file, AppStream
metainfo, hicolor icons from `tinta.ico`, and a shared-mime-info entry for
`.mmd` (no standard type exists). `--register` maps to `xdg-mime`.

**Phase 7: inline editing (live preview).** §6.

**Later: macOS.** GTK4 runs there, so the same backend would work, with a
non-native feel. A CoreText/CoreGraphics backend behind the same compat
headers is the native option. Not scheduled.

## 6. Inline editing (live preview)

Typora/Obsidian style: the document renders everywhere except the block under
the caret, which shows its raw Markdown and is edited in place. This is a
*platform-independent* feature, so it belongs in shared code. Windows benefits
too, and it is a natural upstream PR.

Why it fits Tinta better than it fit md-viewer:

- Viewer layout is already per top-level block (`layoutNextBlock`,
  `layoutElement`). Every run carries `docStart`/`docLength`, and
  `scrollAnchors` maps source offsets to y positions.
- `editor.cpp` (4.1k lines) already owns the caret, multiple carets, undo
  (`EditAction`), IME, wrapping and the Markdown assists. Live mode gives the
  editor a *view*, an island laid out at the active block's position. It does
  not create a second editor widget per block. Two of md-viewer's failure
  modes, undo fragmented per widget and IME inside a swapped widget, don't
  arise.

The remaining hard part, also md-viewer's main finding, is **layout jump on
swap**. Mitigations:

- Lay out the island in the block's own body font and width, with syntax
  markers dimmed rather than inserted where possible.
- Relayout only the two swapped blocks and shift later display-list items
  by Δy. A `shiftLayoutItems` variant for y is needed; today's only shifts x.
- Pin the scroll anchor to the caret line.

Order of work:

1. paragraphs and headings
2. lists and blockquotes, kept atomic
3. tables
4. code, math and Mermaid, shown as source with the rendered output beneath
   while active

Regression tests assert block geometry before and after a swap: a Δy budget,
and no movement for blocks above the active one.

Coordinate with upstream before starting. Upstream changes `editor.cpp`
often, and building this in the fork first guarantees conflicts.

## 7. Validation (AGENTS.md on Linux)

- Fixtures go in `tests/fixtures/` as mixed documents, as AGENTS.md already
  requires. New `linux-*` fixtures cover font fallback, CJK, emoji, RTL,
  math-font fallback and HiDPI.
- Headless rendering means fixtures render to PNG and PDF on CI with **no
  display**, at normal and narrow widths, across all 10 themes. Output goes to
  `build/out/` (ignored), with PNGs uploaded as CI artifacts.
- `input_tests` keep their approach (a message-only window becomes a hidden
  App on an offscreen Cairo target) and drive the real `WndProc` without a
  compositor.
- Interactive checks happen on this desktop: Ubuntu 25.10, GNOME Wayland
  (`wayland-0`) and an X11 session. Each phase's exit records what was checked
  and what is still limited.

## 8. Upstream relationship

- Remotes: `origin` is this fork; `upstream` is `oipoistar/tinta`. Merge
  upstream regularly (no rebase); both CIs gate each merge.
- Once phase 2 is stable, ask the upstream maintainer whether they'd take the
  CMake split and `platform/linux/` as-is. Nothing changes on Windows. If they
  accept, the fork stops being a fork. This is an outward-facing decision and
  Philip's call.

## 9. Risks, ranked

1. **Text metrics.** Pango/FreeType and DirectWrite differ in ascent/descent,
   line gap, uniform line spacing and trailing-whitespace width. Layout code
   measures rather than assumes, which helps; tests with absolute pixel
   values will need tolerances. Caught in phase 1.
2. **Re-entrancy in the nested-loop shims** (clipboard reads, modal
   dialogs). If it bites, convert those call sites to callbacks, which adds a
   few islands.
3. **Wayland limits**: window placement, always-on-top, global cursor
   position, focus stealing. These degrade by design and are documented, not
   hidden.
4. **Upstream churn into Win32-heavy code** (`tabs.cpp`, `main_d2d.cpp`,
   `input.cpp`), where the islands live. Mitigation: keep islands small and
   merge often.
5. **Startup**: 112–130 ms to first frame measured headless, against
   Windows Tinta's advertised <100 ms. GTK init alone is 42 ms. Measure on
   real hardware in phase 2 before optimizing.
6. **Pango cost per layout** (28 µs warm). Fine for incremental layout of
   large documents (43 KB in 21 ms); revisit only if a fixture shows layout
   above budget.

## 10. Open questions

1. **What "inline markdown" means.** This plan assumes Obsidian-style live
   preview (§6). If it meant md-viewer's current single-pane Preview/Edit
   toggle, phase 7 shrinks to a small task; Tinta's `Ctrl+E` and
   `Ctrl+Shift+E` already cover most of it.
2. **Track upstream (compat layer, recommended) or hard-fork Linux-only?**
   A hard fork allows a cleaner abstraction but has to port every upstream
   feature by hand.
3. **Is macOS in scope?** If it is first-class rather than "later", the
   toolkit decision (§2.3) needs another look.
