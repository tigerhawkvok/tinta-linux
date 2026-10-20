# Linked scrolling in the split editor

Validated on Linux (Xvfb, X11 backend) with the Debug build on 2026-10-19.
Reported by Philip on Windows: "the code and preview panes should have linked
scrolling when the edit pane is open (doing this should fix the odd scroll
behavior of the rendered markdown when the edit pane is open)".

## Problem

The preview followed the source, never the other way round, and it followed
in whole source lines.

- **Whole-line mapping.** Each frame took the source's top *line*, as an
  integer, and placed the preview at that line's byte offset. Inside a long
  line (a paragraph is one source line) the preview stood still while the
  source scrolled through it. When the next line reached the top, the preview
  jumped the paragraph's whole rendered height.
- **The wheel over the preview** scrolled the source by 60 px of source and
  moved the preview however far that mapped to: often not at all, sometimes
  hundreds of pixels.
- **The preview's own scrollbar** was overwritten by the next frame's sync, so
  a drag or a track click snapped back.

Measured on `tests/fixtures/linked-scroll.md` at 1050 x 900, stepping the
source 4 px at a time from top to bottom:

| Mapping | Steps where the preview stood still | Biggest single jump | Mean step |
|---|---|---|---|
| Old, wrapped | 1967 of 2123 | 366 px | 4.6 px |
| New, wrapped | 0 of 2123 | 18 px | 4.6 px |
| Old, unwrapped | 610 of 753 | 366 px | 12.8 px |
| New, unwrapped | 0 of 753 | 70 px | 12.8 px |

Unwrapped, a whole paragraph is a single 27 px source row, so 70 px per 4 px
of source is that row's share of the paragraph. It no longer arrives as one
jump.

## Change

- **The pane you scroll leads.** The wheel or scrollbar over the preview
  scrolls the preview by its own step, and the source follows it. Scrolling
  the source in any way gives the lead back: its wheel or scrollbar, the
  caret, typing, or search. The preview then follows the source.
- **Both directions map through the scroll anchors,** at fractional source
  positions. The source's top edge becomes a byte offset within its line,
  counting wrapped rows when wrapping is on. The preview's top edge
  interpolates between the anchors around it. The two mappings are inverses:
  handing the lead back moves nothing.
- **The ends meet.** The preview at its top puts the source at its top, and
  the preview at its end puts the source at its end. The source at its end
  still lets the preview overshoot to reach a tall tail (#85).
- `editorSyncPreviewScroll` (in `editor.cpp`) holds the per-frame sync that
  used to sit inline in `render()`, so tests can drive it.

## Checked

- **`docked_preview_and_hints` (ctest)** loads `linked-scroll.md`, wrapped and
  unwrapped, and checks:
  - the preview never steps back, and moves on every 4 px source step;
  - wrapped, no step exceeds 8 times the mean;
  - three wheel notches over the preview move it exactly 3 x 60 px, the
    source follows, and the next frame leaves both where they were;
  - handing the lead back anywhere in the document moves the preview by
    less than 1 px (measured worst: 0.002 px);
  - the wheel over the source takes the lead back;
  - a click on the preview's scrollbar track jumps the preview and the
    source follows instead of pulling it back.

  With the old whole-line mapping restored temporarily, the smoothness and
  round-trip checks failed. The scrollbar check first failed on a real bug:
  a scrollbar click left the lead's recorded source position stale, and the
  next frame took the lead back. The fix is the single
  `editorPreviewTakesLead`, used by the wheel and both scrollbar paths.
- **`linux_linked_scroll` (ctest, Xvfb):** the built app with real X wheel
  input. It runs three windows: 1100 px light theme wrapped, 700 px dark
  theme wrapped, and 1100 px light theme unwrapped. 15/15 checks pass:
  - the wheel over the preview scrolls it, and the source follows;
  - a second later the preview has not been pulled back;
  - the wheel over the source scrolls both panes;
  - 40 notches back up over the preview return both panes to the top,
    with the preview pixel-identical to the first frame.
- **Screenshots** after preview-led and source-led scrolling at both widths
  show the same text at the top of both panes. They are in
  `build/linux-render/linked-scroll/`.
- Full ctest: 60/60.

## Limitations

- Not yet run on Windows; the Windows build is cross-compiled only.
- Only the wheel and the scrollbar give the preview the lead. Other preview
  navigation in edit mode is unchanged.
- A tall block (a diagram, an image, a long table) is one anchor. Inside it
  the mapping is linear in source bytes, so the preview moves faster than the
  source there.
- Touchpad smooth scrolling and Windows precision touchpads were not
  exercised. Both arrive as wheel deltas on the same path.

## Blocks that open with a line break

Validated on Windows (MSVC 14.50, Ninja, Release) on 2026-10-19. Reported by
Philip on Windows: in a private README, linked scrolling stopped at the
first paragraph after one heading, in either pane.

### Problem

md4c sends soft and hard line breaks, and a code block's indents and blank
lines, from its own string literals, not from the input. The parser set each
element's offset at its first text callback as `text - inputStart`, so an
element whose first text was such a literal got an address outside the
source. That happens to a paragraph, tight list item or quote that opens
with a link, emphasis or code span running to the end of its first line, and
to a fenced block that opens with a blank or indented line. The anchor list
only accepts increasing offsets, so the bad one shut out every later anchor:
the preview stood still while the source scrolled on, and a preview-led
scroll sent the source to its end.

### Change

`ParserContext::trackOffset` (`markdown.cpp`) takes offsets from input text
only. Literals that open an element count back from the input text after
them, never past text already sent, so the offset still lands on the
element's first rendered character: the line break, the blank line, the
indent.

### Checked

- `document_types` (ctest): a document with each case keeps every offset
  inside the source and its blocks in source order; the paragraph and both
  code blocks start exactly at their break, blank line and indent.
- `docked_preview_and_hints` (ctest) also runs the linked-scroll checks on
  `tests/fixtures/linked-scroll-soft-breaks.md`, wrapped and unwrapped, and
  checks both fixtures' anchors stay inside the source and reach its last
  block. Without the change the new fixture fails the anchor check and the
  preview stalls; with it, both fixtures pass.
- The private README, run through the same checks locally: its anchors
  reach its end and the preview follows past the reported paragraph. Its
  remaining stalls sit on one 2.5 KB source line (a link's URL) that renders
  as two short lines.
- Full ctest: 37/38. `math_layout` fails to link with and without the change
  (`fontCollectionFor` is in `bundled_fonts.cpp`, which that target leaves
  out).

### Limitations

- Not run on Linux.
- A code line indented with tabs gets more literal spaces than its source
  bytes, so its block's offset can land a few bytes early, never before the
  previous text.
