# Theme editor fonts apply on save (#265)

Validated on Windows with the Release build on 2026-10-21.

## Problem

After choosing another font in the theme editor and saving, the open
document kept the old fonts until it was reopened or a Print/PDF preview
was opened and cancelled.

`applyTheme` skips rebuilding the document's text formats when the fonts
are unchanged. It decided that by comparing `app.theme`'s font names with
the new theme's, but:

- `app.theme` points into the custom theme's own strings. Saving from the
  editor writes the new names into those strings (the editor saves under
  the theme's own name), so the "old" names already read as the new ones
  and the check found nothing to rebuild. When a new name is longer, the
  string moves and the check read freed memory.
- The heading font was not compared at all, so changing only the heading
  font (Text / Headings / Code, added in 3.8.1) never applied.

## Change

- `updateTextFormats` records the text, code and heading faces it built
  the formats from (`App::formats*FontFamily`).
- `applyTheme` compares the new theme with that record, heading face
  included, so it never reads through `app.theme`'s possibly rewritten or
  freed strings.

## Automated checks

`inline_code_theme` (`tests/theme_tests.cpp`, isolated portable run):
`themeEditorFontsApply` lays out `tests/fixtures/theme-editor-fonts-265.md`
(headings 1–6, emphasis, links, inline and fenced code, a quote, lists, a
table, Chinese text and math) in a saved custom theme, then saves over that
theme in use, as the editor's Save does, four times:

1. new text and code fonts;
2. longer names, so the theme's strings are reallocated;
3. only the heading font;
4. the heading font back to inheriting the text font.

After each save it checks the text, code and six heading formats and that
the laid-out runs use the saved faces.

Restoring the old comparison fails 16 checks: the save over the theme in
use (8), the heading-only change (6) and the relayout (2). The longer-name
save happened to pass, because the freed memory still held the old name.

## Notes

- Saving under a new name went through the same check: a new text or code
  font applied, but a heading-only change did not. Both apply now.
