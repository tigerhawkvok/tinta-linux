# Issue #262 validation: inline code on the text baseline

Reported in [oipoistar/tinta#262](https://github.com/oipoistar/tinta/issues/262):
inline code sat about half a letter height below the surrounding text. The
reporter used Noto Sans for text and MonoLisa for code. Validated on Linux
(the fork's headless build) on 2026-10-14; the change is in shared code and
applies to Windows as well, where it is not yet checked.

## Cause

`layoutInlineContent` (`src/render.cpp`) top-aligned every text run in a
line box of `1.7 × body size`, but centred inline code in that box by
assuming the code's line is `1.2 × code size` tall. The two baselines met
only when the code face's ascent happened to fit that guess. Measured with
the compat layer's metrics at 100%:

| Body + code | Body baseline | Old code baseline | Offset |
|---|---|---|---|
| Noto Sans + MonoLisa (reporter) | 17.10 px | 20.04 px | 2.94 px low (4.4 px at 150%) |
| Segoe UI + Consolas stand-ins | 17.10 px | 18.19 px | 1.09 px low |

## Change

- Code text is shifted by the difference between the body format's
  first-line baseline and the code format's, both measured once per
  paragraph per format. This covers atomic spans and long spans that wrap.
- The chip hugs the code's own line box, padded by 0.2 × code size, and is
  clamped inside the text line so table cells never clip it. Before the
  fix it filled the whole 1.7× line, which hid the offset in body text and
  made a tall slab around code in headings.

## Checks

- `tests/fixtures/inline-code-baseline-262.md`: the reporter's sentence, code in
  a heading, bold, italic, a link, beside math and a superscript, in nested
  and ordered lists, a wrapping quote, table cells, a span longer than a
  narrow column, next to a fenced block, and a final paragraph.
- `inline_code_baseline` (ctest, Linux and Windows CMake): the fixture in
  Paper and Midnight, at 100% and 150%, at 1050 and 650 px, with code faces
  Consolas, Cascadia Code, Courier New and Lucida Console, plus the reporter's
  Noto Sans + MonoLisa pair: 718 code-run comparisons. Every inline code
  baseline is within 0.75 px of its line's body text; the code glyphs sit
  inside the text line; the chip covers them, its padding is at most
  0.2 × code size + 1 px, and above and below differ by under 1 px unless
  the line box clips the chip.
- Both assertions verified by mutation: restoring the old centring fails
  578 of 718 comparisons; restoring the full-line chip fails the chip check.
- Rendered with `tests/linux_render/render_fixtures.py`: the fixture with
  `inline-code-baseline-262-themes.ini` (dark, Noto Sans, MonoLisa) at 150%
  before and after, and in Paper at 600 px and Midnight at 1000 px. Inspected
  by eye: every span shares its line's baseline, including each segment of
  the wrapped identifier.
- All 49 fixtures re-rendered and pixel-diffed against the previous day's
  renders. Differences are the code chips, plus Han faces and spacing from
  the font fallback work in the same session. No geometry changed elsewhere,
  including the wide CJK table.

## Limits

- Not run on Windows. The baseline arithmetic uses DirectWrite's own line
  metrics there, so the result should hold. Chips get shorter: with the
  Linux stand-ins for the defaults, 21.9 px instead of 27.2 px at 100%
  (code line 16.3 px + 2 × 2.8 px). The Windows value is unmeasured.
- Highlight backgrounds (`==text==`) still fill the whole line box.

## Upstream #259 additions (ported 2026-10-23)

Upstream fixed #262 differently in b1d8020 (Tinta 3.8.1), and also #259.
The port keeps this fork's code-span alignment and chip, and takes what
upstream adds beyond it:

- Each text run is placed by its own first-line baseline, so a run that
  draws fallback glyphs (Chinese beside a Latin face, emoji) stays on its
  format's Latin baseline instead of dropping by the fallback font's taller
  ascent. Code spans get the same correction.
- Code spans line up with the face of the style around them (a heading's
  bold face), and the source shown for unsupported TeX sits on the
  surrounding baseline.
- Emphasis inside a heading keeps the heading's size, through bold-italic
  heading faces. They load from the bundled-font collection like the other
  heading faces (upstream passes none).
- Upstream's app-wide baseline cache is not taken; `layoutInlineContent`
  keeps its per-format cache.
- The Linux compat layer gains `IDWriteTextLayout::GetFontFamilyName` by
  position, which upstream's test uses.

Checked on Linux: upstream's `codeBaselines` checks in `superscript_and_learn`
on `fixtures/inline-code-baseline-259.md` pass, and `inline_code_baseline`
passes 856 comparisons.
