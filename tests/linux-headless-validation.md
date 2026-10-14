# Linux phase 1 validation: the app headless

Validated on 2026-10-13/14 on Ubuntu 25.10 (GCC 15.2; Linuxbrew glib
2.88/Pango 1.58/cairo 1.18/librsvg 2.62 for the default build, the system's
glib 2.86/Pango 1.56/librsvg 2.60 for `build-system`, Clang 22 for
`build-clang`). Phase 1 of `docs/linux-port/PLAN.md`: the whole app compiles
and runs on the compat layer, without a window yet.

## What runs

- `build/tinta` is Tinta's own `WinMain` behind a Linux `main()`. Headless
  options: `--render-png=PATH`, `--size=WxH`, `--scale=N`,
  `--settle-ms=N`, `--repaint=N`. The existing `--printpages`,
  `--exportpdf`, `--exporthtml` and `--exportdocx` paths run unchanged.
- Compat layer (`platform/linux/src`): DirectWrite on Pango, including
  custom font fallback and `Draw` to text renderers; Direct2D on Cairo, with
  PDF print controls and halved-bitmap levels; WIC on gdk-pixbuf (lazy
  decode-at-size, file DPI, PNG/JPEG encoding with DPI); a headless Win32
  layer (windows, the message queue, synthesised `WM_PAINT`, timers, capture
  messages, clipboard and OLE snapshots); files, processes, locale, XDG
  folders, GIO shell open, libcurl downloads.
- Selawik ships as the Segoe UI stand-in (`third_party/fonts/selawik`).

## Automated checks

`ctest --test-dir build`: **42/42 pass, none disabled** (24 s). This covers:

- all 18 upstream `input_tests` suites, plus upstream's code block,
  blockquote, footnote, syntax, frontmatter, theme, math layout, inline
  style, context menu, i18n, path and config suites, run unmodified.
  `file_path`, `config_path` and `frontmatter` carry Linux cases where
  upstream tested Windows-only semantics (folder locks, `\\?\` paths,
  share-mode locks);
- `compat_dwrite`: metrics, hit testing, line breaking, CJK fallback order
  (直 in the Japanese face under the Japanese order, the Chinese face under
  the Chinese one), Selawik registered and used for Segoe UI;
- `compat_d2d_wic`: DPI from files, one decode per scaled frame, exact
  scaled sizes and colours, bitmap levels that follow pixel changes, arc
  bounds, a Letter-size PDF page box;
- `linux_headless_render`: an absolute path argument opens the document and
  the window paints;
- `linux_headless_pdf`: Tinta's own `--exportpdf` through the print path;
- `inline_code_baseline` (#262, see `inline-code-baseline-validation.md`).

The new checks were mutation-tested: each fails when its bug is restored
(double image decode, stale bitmap levels, fallback ignored, old code
centring, full-line chips, PDF pages released before the print control
closes).

`build-system` (system libraries only, no Linuxbrew library linked) and
`build-clang` (Clang 22): 40/40 each, the same suite without merman, which
they build without.

## PDF export

`render_fixtures.py --pdf` runs `--exportpdf` on all 49 fixtures: 49 valid
PDFs, 102 pages, all US Letter (the locale's measurement system is US).
`pdfinfo` reports no errors; `pdftotext` extracts the text; `pdffonts` shows
embedded subsets (Selawik, DejaVu Sans Mono for code). A page rasterised with
`pdftoppm` was inspected: print theme, math, table and headings intact.
The first run aborted in every document: cairo-pdf writes recording sources
when the document finishes, and `print.cpp` releases each page's command
list before closing the print control. The print control now keeps each
page alive until it has finished the PDF.

## Rendered and inspected

`tests/linux_render/render_fixtures.py` renders every `tests/fixtures/*.md`
headless (49 documents). Inspected by eye: the regression control, math
(matrices, fractions, roots, binomials, big operators), native Mermaid
(flowchart, sequence, pie), images (PNG/JPEG, inside tables, translucent,
panorama, broken/oversized/missing placeholders, 300 DPI), syntax
highlighting, the wide CJK table and the wrapped-cursor fixture. Variants
checked: Paper and Midnight, 1000 and 600 px, 1x and 2x. The day-2 renders
were pixel-diffed against day 1; every difference was traced to a change
(code chips, CJK face order, ASCII spaces next to CJK, Selawik).

## Measured

| | Value |
|---|---|
| Startup to first frame, headless, typical fixture | ~60 ms (465 ms wall minus 405 ms settle) |
| 6000x4000 JPEG decoded to 5016x3344 | 240 ms, was 557 ms (two decodes, slower filter) |
| Image stress fixture, first frame | 2.1 s, was 8.6 s |
| Repaint with three large images, 1000x2600 | 12.4 ms median, 113 ms without bitmap levels |

## Limits

- No window: the GTK layer is phase 2. Until then the clipboard is
  in-process, dialogs report "cancelled", message boxes answer no/cancel on
  stderr, and `ShellExecute` opens through GIO (untested here; headless runs
  never trigger it).
- The app icon is a Windows resource and is missing from the caption.
- Mermaid still renders natively; merman is built and tested but not yet
  wired into layout (MM2/MM3).
- Remote images (libcurl) and pandoc exports are implemented but not
  exercised by any check yet.
- Selawik has no italic (fontconfig slants it) and no kerning; non-Latin
  text falls through to the rest of the stand-in stack.
- Windows itself was not rebuilt; the shared-code changes compile there in
  principle (`fs_path.h`, `joinPath`, the #262 change) but are unchecked.
