# List item spacing: validation

Requested on 2026-10-15. Philip found that his work README's "Mandatory
flags" list ran together. Its items are one-paragraph flag descriptions
that wrap over two to four lines, and Tinta stacked list items one body
line apart, so a wrapped line read like the next flag. He asked for at
least 1.5 line heights between bullet lines.

## The rule (`render.cpp` `layoutList`)

- Between list items there is at least half a body line of space (0.5 ×
  1.7 × the body font size). A list of one-liners therefore advances 1.5
  lines per item: 41.6 px against a 27.2 px line at 16 px.
- An item's own wrapped lines keep the paragraph pitch, so each item reads
  as one block.
- Space an item already leaves below its content counts toward the half
  line. That covers a loose item's paragraph margin and a trailing quote,
  code block or nested list. It is measured as the distance from the
  item's lowest drawn content to the layout position (the helper
  `contentBottomSince`, now shared with blockquotes), so loose lists and
  block-ending items are not spaced twice. An earlier version that listed
  block types by hand added a full extra half line after items ending in a
  quote. Philip's README showed it, and it was fixed before commit.
- **HTML export:** `li+li{margin-top:0.8em}`, half of its 1.6em line. A
  loose item's paragraph margin collapses into it, the same rule in CSS.
- **DOCX** already puts 8 pt after every paragraph, list items included:
  about half its 15.6 pt line. Unchanged.

## Checked

- **`list_spacing`** (`tests/list_spacing_tests.cpp`) measures marker and
  text geometry at 16 px:
  - one-line items advance at least 1.5 lines
  - after an item wrapped at 420 px there is at least half a line
  - nested items advance at least 1.5 lines
  - an item ending in a quote is followed by half a line, once
  - loose items advance between 1.5 and 1.75 lines (not doubled)

  Mutations:
  - With no gap, one-liners advance 28 px (1.03 lines) and the wrapped,
    one-line and nested checks fail.
  - With the trailing space ignored, loose items advance 54.8 px (2 lines)
    and the item ending in a quote gets 27.6 px; those checks fail.
- **Philip's README** (excerpts rendered in ignored scratch space only;
  the file stays out of the repo):
  - "Mandatory flags" now reads as distinct flags.
  - In the PR-procedure ordered list, items ending in quotes keep their
    spacing.
  - The "Conventions Used" nesting (five levels) separates siblings and
    keeps each parent attached to its first child.

  The README's `i.`/`ii.` sub-items are not list markers in Markdown, and
  GitHub shows them inline too.
- **Fixture** `tests/fixtures/list-item-spacing.md` (flags with code spans
  and wrapped descriptions; ordered, nested, task, loose and quoted lists;
  a table and code):
  - rendered at 1000 px in Paper and at 560 px in Wombat
  - exported to PDF (3 pages) and HTML (carries the rule)
- ctest 49/49.
