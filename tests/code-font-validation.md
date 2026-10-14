# Default code face validation: Monaspace Argon

Validated on Linux on 2026-10-14. Tinta's default code font is now
Monaspace Argon (GitHub Next, OFL 1.1), shipped in
`third_party/fonts/monaspace-argon`, instead of Consolas, which only Windows
ships. Built-in themes that used Consolas, the source editor's gutter and
the edit rail use it; Arctic, Midnight and Abyss keep their Cascadia faces.

## Checks

- `code_font` (ctest, Linux and Windows CMake): seven built-in themes code
  in the default face; the four code formats and the gutter use it; it is
  monospaced (`iiii` and `WWWW` measure the same); a missing or empty face
  resolves to it. Mutation-checked: a resolver that passes names through
  fails the missing-face cases.
- `compat_dwrite`: Monaspace Argon is registered (it is not installed on
  the system) and Consolas text measures identically to Argon text.
- `inline_code_baseline` gains Argon in its face matrix; all upstream
  suites still pass (43/43).
- `tests/fixtures/code-font-monaspace.md` rendered in Paper at 1000 px and
  Dracula at 600 px, and every fixture re-rendered: bold, italic and
  bold-italic code; code in a heading, link, table, quote and wrapped line;
  aligned columns; `=>`, `->`, `!=` and `===` stay separate glyphs (no
  ligatures unless a typography asks for them); box drawing is Argon's own
  (it joins across lines, as in any monospace face); CJK and emoji fall back
  cleanly. The Cascadia themes render code in Argon on Linux.
- PDF export embeds `MonaspaceArgon-Regular` (`pdffonts`).

## Limits

- Windows: nothing loads bundled fonts there yet, so the default resolves
  to Consolas unless Monaspace Argon is installed. Not run on Windows.
- DOCX names the theme's code face; Word substitutes on a machine without
  Monaspace Argon. HTML lists `'Monaspace Argon', Consolas, monospace`.
- Only four faces ship (Regular, Italic, Bold, BoldItalic); other weights
  are synthesised by fontconfig if a theme asks for them.
