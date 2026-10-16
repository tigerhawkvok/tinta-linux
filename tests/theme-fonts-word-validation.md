# Theme fonts in Word and the theme editor (#256, follow-up)

Validated on Windows with the Release build on 2026-10-16.

## Report

Testing 3.8.0, the reporter found three things:

- **Chinese text in Word.** In Chinese Microsoft 365 Word, Chinese text in the
  Word export still showed in Word's East Asian default, DengXian, even though
  `font=Microsoft YaHei`. English and code used the theme fonts.
  - The reporter traced it to `styles.xml`. The fonts were only in
    `<w:docDefaults>`, and the `Normal` style every paragraph style is based on
    had no run properties.
  - Copying the defaults' `rFonts`, color and size into `Normal` fixed it in
    their Word.
- **Missing `headingfont=`.** `themes.ini` written by the theme editor had no
  `headingfont=` line to fill in.
- **Missing font choices.** The theme editor could only choose the main font,
  not `codefont=` or `headingfont=`.

## Cause

- `Normal` carried no fonts, and Chinese Word reads East Asian text from that
  style rather than from the document defaults. English Word, even with
  Chinese editing enabled, did not reproduce it here.
- The writer left `headingfont=` out whenever headings used the main font.
- The editor saved the code and heading faces of the theme it started from,
  with no way to change them.

## Change

- The Word export writes the body run properties (Latin, East Asian and
  complex-script faces, color, size) into the `Normal` style as well as the
  document defaults.
- `themes.ini` always has a `headingfont=` line, empty while headings use
  `font=`, and its header says so.
- A new `themes.ini`, created by "Open themes.ini for full control" when there
  is none yet, starts with a commented example that names every key,
  `headingfont=` and `codefont=` included.
- The theme editor has a Text / Headings / Code switch above its font list,
  styled like the settings' segmented controls.
  - The list sets the chosen face and scrolls to the current choice.
  - For headings, the first row, "Same as text", empties `headingfont=`.
  - The specimen shows headings and inline code in their chosen faces.
  - The editor starts from the base theme's three faces, so a theme derived
    from Abyss keeps its pinned heading face.
  - Save writes all three faces.
  - The strings are translated for every built-in language.

## Fixtures and automated checks

`fixtures/print-word-export-256.md` and `print-word-export-themes.ini` (from
the earlier #256 work) remain the mixed fixture: Chinese headings, body,
emphasis, inline code, links, a list, a quote, a table and code blocks.

- `print_and_word_export` now also checks that the `Normal` style names the
  body faces, for a CJK body face (SimSun) and for a Latin face whose CJK
  fallback is chosen from the screen (Georgia).
- `inline_code_theme` now drives the theme editor with real mouse handlers:
  1. It opens Settings, then Appearance, then New.
  2. It switches to Headings and clicks Georgia, switches to Code and clicks
     Courier New, then switches to Text and clicks Verdana.
  3. It saves, then checks `themes.ini` has `font=Verdana`,
     `headingfont=Georgia` and `codefont=Courier New`, and that the saved
     theme applies all three.
  4. It saves a second theme with headings set back to "Same as text" and
     checks for an empty `headingfont=` line that loads as the text face.
  5. It checks that the template for a new `themes.ini` names `font=`,
     `headingfont=` and `codefont=`.

Each check fails when its change is removed:

| Change disabled | Failures |
| --- | --- |
| No run properties in `Normal` | 2 |
| `headingfont=` only when set | 1 |
| The list always sets the text face | 3 |
| Save keeps the base theme's code and heading faces | 2 |

## Desktop check

In the built app:

- Settings → Appearance → New opened the editor with the switch above the font
  list. In Paper, Headings selected "Same as text", and Code scrolled to and
  selected Consolas.
- Picking Bahnschrift for headings changed the specimen heading at once.
- Saving with Constantia for code wrote `headingfont=Bahnschrift` and
  `codefont=Constantia`, and the document redrew with those faces.
- In Midnight at 720 px the editor and the switch fit and read clearly.

Captures are in `out/issue-256/editor/` (ignored).

## Limits

- Word in English never showed DengXian here, so the Word fix rests on the
  reporter's own test of the same change.
- The font list shows every installed family for code too; it does not
  filter to monospaced faces.

## Draft reply (not posted)

### #256

Thank you for tracking the Word problem down to `styles.xml`, that made the
fix easy. The next release writes the theme's fonts into the Normal style
too, exactly as you tested, so Chinese text in Word uses the theme font
instead of DengXian. `themes.ini` now always contains a `headingfont=` line
(empty means headings use `font=`), and the theme editor has a Text /
Headings / Code switch above the font list, so you can choose all three fonts
there.

谢谢您把 Word 的问题定位到 `styles.xml`，这让修复变得很简单。下一个版本会像您测试的那样，把主题字体也写入 Normal 样式，Word 中的中文将使用主题字体而不是等线。`themes.ini` 现在总会包含 `headingfont=` 这一行（留空表示标题使用 `font=`），主题编辑器的字体列表上方新增了“正文 / 标题 / 代码”切换，三种字体都可以在编辑器中选择。
