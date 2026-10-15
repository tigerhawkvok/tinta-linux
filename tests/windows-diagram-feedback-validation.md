# Diagrams on real Windows: feedback and fixes

On 2026-10-15 Philip ran the cross-built exe on a real Windows box against
his work README. The README has a 77-line `stateDiagram-v2` and is not in
the repo. He reported:

1. **Labels:** visible.
2. **Opening:** a stall of "maybe 2-5 s", then responsive. The diagram
   overflowed the window, and the hover Fit button worked.
3. **Sharpness:** crisp at full size, "somewhat fuzzy" when fitted.
4. **Exports:** PDF export included the diagram.
5. **Copy:** "copy diagram as image" worked.
6. **Wombat:** headings worked.
7. **SmartScreen:** it complained (the exe is unsigned); `settings.ini`
   untested.

## Fuzzy when fitted: the bitmap was resampled

The screen bitmap is rasterized for the device pixels it covers, but the
rectangle it was drawn into came from the layout:
`diagram.width × scale`, fractional, at a fractional `y`. merman's raster
size is not always `ceil(width × scale)`.

Philip's README diagram through `merman_svg --png`:

| scale | layout size | PNG size |
|---|---|---|
| 1 | 2133.56 × 2022 | 2134 × 2022 |
| 0.6734 | 1436.74 × 1361.61 | 1438 × 1362 |

At the fitted scale the PNG is a full pixel wider. D2D's default linear
filter then resampled every column. At full size the sizes happened to
agree, which matches what Philip saw.

Fix:
- On screen, `layoutMermanDiagram` gives the rectangle the bitmap's exact
  pixel size, at whole-pixel coordinates.
- The draw loop snaps such device-sized bitmaps to whole device pixels
  after the scroll offset and transform, and draws them with
  nearest-neighbour filtering. A bitmap whose size differs from its
  rectangle, such as a photo, still scales smoothly.
- Print and PDF are unchanged (300 dpi bitmaps in layout rectangles).

Checked:
- `merman_layout_tests` now requires an exact size and a whole-pixel origin
  for every diagram.
- With the layout snap disabled it fails three checks on both raster
  paths:
  - full size: top at y = 543.2
  - zoomed: top at y = 747.2
  - fitted: a 321 px bitmap in a 320 px rectangle on the PNG path
- On Philip's README diagram (Linux, librsvg path), label edge energy rises
  from 28.05 to 29.15 at full size. The fitted case is where the old
  resampling was severe.
- Small text in a fitted diagram is still softer than the body text.
  resvg draws it with greyscale antialiasing and no hinting at roughly
  0.7× size. That is inherent to the raster, not resampling.

## A wide diagram was cut off: no sideways scroll

Native diagrams widen `contentWidth`; merman diagrams never did. A wide
unfitted diagram was clipped at the window edge with no horizontal
scrollbar. It now widens the content to its right edge plus the 40 px
margin, as native diagrams do. A fitted or narrower diagram stays in the
column, and the margin absorbs its possible one-pixel raster overshoot,
so no pointless 1 px scrollbar appears.

Checked:
- At a 420 px window, an unfitted wide diagram makes the content reach its
  right edge. With every wide diagram fitted, the content is no wider than
  the window.
- Without the change the first check fails: content 420 px, diagram right
  edge 724 px.
- Philip's README diagram at 1100 px now shows the horizontal scrollbar.

## The stall: every diagram was rendered twice on Windows

Tinta starts on a software render target and switches to a hardware one
once the GPU is warm (`WM_APP_GPU_READY`). The switch drops every device
bitmap. On the PNG path that meant a second merman render per diagram,
synchronously on the UI thread. A DPI change did the same.

merman's PNG bytes are now kept per raster key: diagram, theme and scale,
32 entries, least recently used dropped. A new render target only decodes
them again.

Checked:
- `merman_layout_tests` (PNG path) recreates the render target and lays
  out again. The diagrams come back with no new merman render. With the
  cache bypassed, that is 4 renders and the check fails.
- Under Wine, Philip's README diagram went from `raster ×2 403.7 ms` to
  `raster ×1 206.8 ms`. The second pass is a 70 ms decode.

The remaining cost is still well short of 2-5 s. On Linux the Windows path
costs 197 ms (SVG 21.6, raster 123, decode 52); under Wine it is about
370 ms. So the rest of the stall is still unmeasured on real Windows. The
stats overlay now shows it.

## Stats overlay: diagram timings

Press `S`, or start with `--stats`, to show the stats overlay. It now has
a third, language-neutral line once a diagram has been drawn:

    merman ×N: SVG … ms | raster ×N … ms (1st …) | decode … ms

- **SVG:** merman's layout.
- **raster:** merman's PNG on Windows, librsvg on Linux. The first raster
  includes merman's one-time system font scan.
- **decode:** PNG into a bitmap (WIC).

`statsOverlayHeight` sizes the box; the overlays stacked above it use it.

## Tools

- `merman_svg in.mmd out.png --png SCALE`: writes the PNG the Windows
  viewer draws. It reports PNG against layout size, plus SVG and PNG time
  (first and repeat calls).
- `render_fixtures.py --stats`: renders with the overlay.
- `build-png/`: a Linux tree with `-DTINTA_MERMAN_PNG=ON`. It runs the
  Windows diagram path natively; Rust from scratch took 204 s.

ctest: 49/49 in `build/`. `merman_layout` also passes in `build-png/`.

## The 2.9 s: merman scanned every system font (4.0.1 measurements)

Philip read the new stats line from v4.0.1 on his work laptop, for the
same README:

| | first render | after one zoom step |
|---|---|---|
| SVG layout | 22.7 ms | |
| merman PNG | 3,053.9 ms | 3,219.7 - 3,053.9 = 165.8 ms |
| decode | 29.0 ms | 32.4 ms |

A render costs 166 ms there, against 123 ms on Linux. The remaining
2.89 s was paid once per process: merman's raster builds its font
database by opening every file in `C:\Windows\Fonts`
(`fontdb::load_system_fonts`). The same scan takes about 16 ms on Linux.
Per-file opens on a managed laptop are the likely cost; that part is
unmeasured.

Fix:
- `cmake/patches/merman-raster-font-files.patch`, applied by
  `cmake/apply_patch.cmake` when merman is fetched. When
  `MERMAN_RASTER_FONT_FILES` is set (paths joined like PATH), merman loads
  only those files. If none of them loads, it scans as before.
- On Windows, before the first PNG, Tinta resolves a family list to files
  through DirectWrite's system font set (`prepareMermanRasterFonts`). The
  font cache answers the names, so no file is opened to find them. The
  list:
  - every theme's face and the rest of the diagram stack;
  - fontdb's generic families (Arial, Times New Roman, Courier New);
  - Windows' per-script fallback faces (Segoe UI and its symbol, emoji
    and historic faces, the CJK UI faces, Nirmala UI, Leelawadee UI and
    the rest).
- Linux leaves the variable unset; its scan is fast.
- The stats line adds `N fonts X ms` for the list.
- `TINTA_TRACE_FONTS=<file>` writes the list to a file. Windows GUI
  processes have no stderr.

Checked:
- `merman_raster_fonts`, registered only when `TINTA_MERMAN_PNG`. Each
  case renders in a forked child, because the font database is built once
  per process:
  - Selawik only renders;
  - adding an unused Argon leaves the PNG byte-identical;
  - Argon only draws differently;
  - an unloadable list matches the unset variable (the system scan).
- With the patch reversed, "Argon only draws differently" fails: merman
  ignores the list and scans.
- Wine, with `--stats` and the trace: 13 files named. They are Wine's
  stand-ins for Courier New, Times New Roman, Arial and Microsoft YaHei,
  plus Cambria and the host's Noto Sans and DejaVu Sans in all styles.
  Labels draw.
  - Selawik, copied into the prefix's Fonts folder without registration,
    is not an installed font, and DirectWrite rightly leaves it out.
  - Wine's timings (240 ms for the list, 450 ms first raster) are not
    representative: Wine builds the font set itself, while Windows serves
    it from the font cache service.
- Not yet measured on real Windows. The stats line will show the list
  time, the file count and the first raster.
