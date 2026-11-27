# Inline code in headings (#1): validation

2026-10-29. Philip (#1, a Windows screenshot): in `` ## `git` `` the code is
drawn at body code size, small under a heading.

## Cause

- **Viewer:** `inlineCodeFormat` returned one of the four fixed code formats
  (14 px at 1x) whatever the text around the span was.
- **Word export:** wrote every code run at 10.5 pt (`w:sz` 21), even inside a
  24 pt H1.
- **HTML export:** already scaled (`code{font-size:0.9em}`).
- **PDF:** follows the viewer.

## Fix

Inline code keeps body code's proportion to body text (14 : 16, 7/8) against
the text around it, as GitHub's `code { font-size: 85% }` does:
- **Viewer:** `inlineCodeFormat` takes the surrounding format and returns a
  code format at 7/8 of its size. Formats other than the body's are made on
  first use and cached in `App::scaledCodeFormats` (released with the other
  formats).
- **Word export:** each paragraph carries a code size, 7/8 of a heading
  style's (H1 42, H2 34, H3 29, H4 24, H5 21, H6 18 half-points). Elsewhere
  it stays 21.

Bold, italic and links still pick the code face's weight and style. Code in
a heading stays the regular code weight, as before.

## Checked

- **Fixture:** `tests/fixtures/heading-inline-code.md` has code at every
  heading level and a code-only heading (the reported case). It also has
  code with emphasis in a heading, then body, bold, italic and link code,
  lists, a quote that wraps, a table, a callout with code in its title, a
  fenced block, and a final paragraph.
- **`inline_code_baseline`:** runs over this fixture and the #262 fixture,
  with six code fonts, two themes, 1x and 1.5x, at 1050 and 650 px.
  - Every inline code run must be 7/8 of the largest text on its line, within
    0.3 px (1816 comparisons), besides keeping that text's baseline.
  - The code-only `## `git`` must be 7/8 of the H2 size.
  - In the Word export, its run must be `w:sz` 34 while body code stays 21.
  - **Mutation check:** with the surrounding format not passed, the test
    fails (`code` 14 px beside 26 px text, expected 22.75).
- **Full suite:** ctest 65/65. The Windows cross-build compiles.
- **By eye:** the headless renderer in Paper at 1000 px and Midnight at
  560 px, and a PDF export (page 1 at 70 dpi).
  - Heading code grows with its level, down to H6 at body-code size.
  - Chips hug the larger code.
  - Body, list, quote, table and callout code is unchanged.

## Not covered

- **Word:** checked through its XML only, not opened in Word.
- **A row split across pages (not from this change):** in the PDF, the
  table's first row starts at the foot of page 1 (an empty row box) and its
  text is on page 2. The 4.0.12 release splits the row the same way.
