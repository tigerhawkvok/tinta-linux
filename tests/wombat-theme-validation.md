# Wombat theme: validation

Added on 2026-10-14 as built-in theme 11 (`src/themes.cpp`), from the
`themes.ini` section Philip ran on Windows. It goes last so saved theme
indices stay valid. Fixture: `tests/fixtures/wombat-theme.md`.

## The palette, as given

The colours are as given: background 242424, text F6F3E8, heading 4BC98A,
link 8AC6F2, code B8F53C on 0F0F0F, inline code CAE682, marks FFC125 behind
424242, blockquote 333333 and accent CC99CC. The syntax palette is Abyss's
(FF4DD8, B8F53C, 5C6660, FF8A3D, 2EE6CF, F5E663, C77DFF). Body text is in
Noto Sans and code in MonoLisa. Where MonoLisa is not installed, the
existing fallback picks Monaspace Argon, and Consolas on Windows.

## Heading face: Noto Sans Display instead of Segoe UI

Rendered side by side with the same palette: Selawik (the bundled
Segoe UI stand-in), Noto Sans Display and Ubuntu.
- **Chosen: Noto Sans Display.** It is the display cut of the body face, so
  headings stay in one family, and Noto ships on essentially every Linux
  distribution.
- Ubuntu is distinctive but Ubuntu-only.
- Selawik only imitates Segoe UI.

On Windows, where Noto Sans Display is usually missing, DirectWrite falls
back to its default UI face.

## Checked

- **`inline_code_theme`** (`tests/theme_tests.cpp`) asserts that:
  - Wombat is the last built-in.
  - It keeps its own inline-code colour (CAE682) and highlight colours
    (FFC125 behind 424242 text).
  - Every other built-in still inherits inline code from code.
  - Wombat joins the layout loop: inline-code runs in headings, tables,
    quotes and lists use the inline colour at 1050 and 650 px, and changing
    only the colour leaves the geometry alone.

  Mutation check: changing Wombat's inline colour to CAE683 fails the test,
  and restoring it passes.
- **Renders at 1000 and 560 px wide.** Exact-colour pixel counts at 1000 /
  560 px:

  | Colour | 1000 px | 560 px |
  |---|---|---|
  | background 242424 | 897,490 | 464,561 |
  | heading 4BC98A | 4,104 | 4,127 |
  | mark FFC125 | 6,093 | 6,261 |
  | code background 0F0F0F | 368,263 | 201,604 |
  | link 8AC6F2 | 101 | 69 |
  | inline code CAE682 | 90 | 77 |
  | keyword FF4DD8 | 180 | 187 |

  Looked at by eye: headings, marks, chips, the table, quote, lists, the
  three syntax-highlighted languages and the Mermaid diagram (mauve
  borders). At 560 px the code blocks scroll sideways rather than wrap, as
  in every theme.
- **PDF export** embeds Noto Sans (Regular, Bold, Italic),
  NotoSansDisplay-Bold and MonoLisa-Regular.

## Found on the way: the render harness hid user fonts

`render_fixtures.py` isolates `XDG_DATA_HOME`, and fontconfig finds per-user
fonts in `$XDG_DATA_HOME/fonts`. Philip's MonoLisa is in
`~/.local/share/fonts`, so under the harness a theme naming MonoLisa
silently got the Argon fallback. The evidence:
- Renders with `codefont=MonoLisa` and `codefont=Monaspace Argon` were
  pixel-identical.
- A trace showed `installedCodeFont(MonoLisa) = 0`.

The harness now links the user's real `fonts` directory into its isolated
data folder, so settings are still isolated but typefaces are not. `--env`
now also reaches `--pdf` exports. With that fix, the #262 fixture
(Noto Sans + MonoLisa) was re-rendered with MonoLisa actually in use: its
PDF embeds MonoLisa Regular, Bold and Italic, and inline code still shares
the baseline in a heading, bold, italic, a link and lists. Earlier records
that relied on the harness with `codefont=MonoLisa` were really showing
Argon.
