# Contents jumps across a reflow

Validated on Linux (Xvfb, X11 backend) on 2026-10-23. Reported by Philip on
Windows: clicking a Contents item jumps to the right place, then the view
scrolls away as the panel closes; reopening the panel puts you back.

## Problem

An open Contents panel narrows the reading column. A click scrolled to the
heading's position in that narrow layout, then closed the panel. The next
frame saw a wider viewport and re-flowed the document, which moved every
heading up, but the scroll offset stayed the same number of pixels, so the
view landed further down the document. Reopening the panel restored the
narrow layout, so the same offset showed the heading again.

The same thing happened to any reader whenever the column changed width:
the file browser opening or closing, or the window being resized.

## Change

- `reflowForViewportWidth` (`render.cpp`) runs once a frame before layout.
  When the viewport width changes it marks the layout dirty, as before, and
  now also notes the reader's place, in this order:
  1. a heading that Contents just jumped to, if the view has not scrolled
     since;
  2. the top of the document;
  3. otherwise, the source offset under the reading line, found by
     interpolating between the scroll anchors.
- `restoreReadingPlace` runs after the layout and scrolls back to that
  place in the new layout. It finishes the layout first when the place lies
  beyond what the viewport-first layout reached.
- The reading line is where a jump lands its target (`readingLineOffset`:
  the tab strip plus 14 px), now shared with `scrollToHeadingY`.
- Edit mode is untouched: its preview follows the source.

## Checked

- `contents_and_panel_resize` (ctest) runs on
  `tests/fixtures/toc-jump-reflow.md`. That fixture has 26 headings between
  long wrapping paragraphs, plus tables, lists, quotes and code.
  - Contents docked right, then left, jumping to headings 5, 17 and 25: the
    jump puts the heading at the reading line, the panel closes, and after
    the reflow the heading is still at the reading line (to within 1 px,
    or at the document's end for the last heading). The heading must move
    more than 20 px in the reflow, so the fixture really exercises it.
    Contents still marks the heading it jumped to.
  - From a place 45% down the document, opening the file browser and then
    narrowing the window to 800 px keep the reading line in the same
    paragraph, at the same fraction of it (within 1%).
  - A document at its top stays at its top.
  - With `restoreReadingPlace` disabled, the jump check and both width
    checks fail.
- In the built app, on Xvfb with real input: Tab opens Contents, typing
  "tannery" filters it, and a click on "11. The tannery" jumps there.
  - Run at 1100 px in the light theme and at 760 px in the dark theme.
  - In both, the heading stays at the top after the panel closes, and
    again when Contents reopens and narrows the column.
  - Screenshots are in `build/toc-jump/`.
- Full ctest: 60/60.

## Limitations

- Not yet run on Windows.
- When a heading rewraps at the new width, the general place (not a
  Contents jump) lands within that heading's block, at the same fraction of
  it, rather than exactly at its first line.
