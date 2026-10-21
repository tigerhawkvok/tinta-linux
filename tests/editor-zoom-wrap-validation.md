# Editor zoom with word wrap (#266)

Validated on Windows with the Release build on 2026-10-21.

## Problem

The reporter's video shows edit mode with word wrap on and long Chinese
paragraphs. After Ctrl+wheel zooms the text, wrapped paragraphs in the
source are drawn over the next lines.

With word wrap on, the editor counts the rows each logical line wraps into
and places lines by those counts. The counts were rebuilt only when the
wrap width changed. Zooming changes the font size, not the width, so the
old counts stayed: a line that now wraps onto more rows was drawn over the
next one, and one that needs fewer left a gap. A theme change to another
code font goes through the same path (found by reading the code, not
tested separately).

## Change

Whenever the editor's text format is rebuilt (zoom, theme or code font,
DPI), the row counts are marked stale and the cached line layouts are
dropped, so the next paint counts and lays out at the new font
(`updateOverlayFormats`, `src/d2d_init.cpp`).

## Automated checks

`wrapped_cursor_navigation` (`zoomKeepsWrappedRows`): opens
`tests/fixtures/editor-zoom-wrap-266.md` in the editor with word wrap on: long
English and Chinese paragraphs, a list, a quote, a table, a code block,
inline code, a link and math. At 650 and 1050 px, with the preview shown
and hidden, it sends 10 Ctrl+wheel notches up over the source (1.0x to
2.0x) and 14 down (to 0.6x), each followed by the zoom timer's catch-up
and a paint. After every notch, each line's reserved rows must equal the
rows its layout wraps into at the current font.

Before the change it failed in all four configurations on the first notch
(for example "7 rows drawn in 6 reserved"). After it, it passes.

## Limits

- Without word wrap every line is one row, and that path was already
  right.
