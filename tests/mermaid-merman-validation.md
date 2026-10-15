# merman engine validation (MM1)

Validated on Linux (Ubuntu 25.10, GCC 15.2, Rust 1.95.0) on 2026-10-13.
Nothing here was drawn by Tinta itself: the viewer hookup (MM3) needs the
Linux Cairo shim (phase 1). See *Not checked*.

## What was built

- `cmake/merman.cmake` fetches merman `3a4b8c3` (0.9.0) and Corrosion v0.6.1.
  It builds `merman-ffi` as a static library with
  `all-diagrams,svg,layout-cytoscape` at `opt-level=s`, fat LTO and one
  codegen unit (confirmed in `build.ninja`), on merman's pinned 1.95.0
  toolchain. The Linux build enables it through `TINTA_MERMAN`, which is ON by
  default.
- `merman_engine::renderSvg` is a platform-free wrapper over the ABI 3 C
  interface:
  - one lazily created engine, calls serialized;
  - `resvg-safe` SVG, so there is no `foreignObject`;
  - drop shadows switched off;
  - the host theme as merman `presentation.theme` roles;
  - the size taken from the root `viewBox`;
  - merman's failure message on error.

## Automated checks

`ctest --test-dir build --output-on-failure`: **7/7 passed**.

`merman_engine` parses two fixtures with Tinta's Markdown parser and renders
every Mermaid fence:

- `tests/fixtures/mermaid-merman.md`. A mixed document: a table, a quote, a
  list, `code` and emphasis. Its diagrams are ones the native parser rejected:
  - composite states with `<<choice>>`, `<<fork>>` and multi-line notes
    (modeled on a real README that showed as code);
  - flowchart `@{ shape }`, `&`, `<-->` and `~~~`;
  - a top-to-bottom git graph;
  - class namespaces.

  Fences 1–4 render. Fence 5, which is not valid Mermaid, fails with a
  message.
- `tests/fixtures/mermaid-preamble.md`: fences 1–8 render; the unclosed
  frontmatter control (fence 9) fails, as it does natively.
- The theme check: `surface` and `line` role colours appear in the SVG, and a
  font family containing quotes is JSON-escaped.

## Corpus (pinned merman `3a4b8c3` fixtures, Mermaid `4e3a85e` docs)

`merman_corpus_probe` is the same harness as `mermaid_corpus_probe`, rendering
through `merman_engine`:

| Corpus | Native (after M1) | merman |
|---|---|---|
| Mermaid syntax docs, 548 | 336 | **548** |
| Realistic merman fixtures, 1,908 | 1,228 | **1,892** |
| All merman fixtures, 3,796 | 2,460 | **3,759** |

`compare_runs.py` native → merman: 1,169 fail → ok and 142 unrecognized → ok
in the fixtures; 76 fail → ok and 136 unrecognized → ok in the docs.

It also flags **12 diagrams native draws and merman rejects**:

| Count | Cause |
|---|---|
| 6 | math labels (`math` capability not built) |
| 3 | lenient syntax: a radar axis trailing comma, a treemap `classDef` `color` token, a sequence `end`-keyword stress case |
| 2 | merman's SVG well-formedness postprocessor fails on mindmap HTML-sanitization stress fixtures |
| 1 | `ellipse` shape, which Mermaid itself rejects (a parser-only fixture) |

So the viewer must fall back from merman to the native renderer before the
code block (PLAN.md §6.2).

Three more realistic fixtures fail in the wrapper only. They are Mermaid's
`info` diagram, which prints the Mermaid version, and merman renders them,
but its root `<svg>` has no `viewBox` and no height (`width="100%"`,
`style="max-width:400px"`), so there is no reliable size to lay out. They
fall back as before. merman alone renders 1,895 of 1,908; through the
wrapper, 1,892.

## Math labels (`math` feature, added the same day)

With RaTeX built in, the 6 math-label fixtures render: 1,898 / 1,908
realistic fixtures. The remaining 10 are the 7 `...` docs fragments and the 3
`info` diagrams.

"Renders" was not enough here, and only drawing the output showed it.
merman places each formula as an inline `<svg>` inside an HTML
`foreignObject` label. The `resvg-safe` pipeline removes those, so the
formulas vanished: blank nodes and edges, and raw `$$\alpha$$` text for
sequence actors. `merman_engine` now:

- renders `$$` sources twice, `parity` and `resvg-safe`;
- walks the `parity` SVG with a stack of accumulated `translate()`s to give
  each math-only `foreignObject` an absolute box (anything under another
  transform is left out rather than misplaced);
- draws each formula over the safe render in a root group, centred in its
  box. The font size comes from box height ÷ formula height in em, which is
  14.0 px for every label in both fixtures. Widths don't work for this:
  node labels have a fixed CSS width.
- gives every glyph an inline `fill`/`stroke` style in the theme text
  colour. Without it, the diagram's `.node path` rules painted gradient fills
  and strokes onto the glyphs; CSS outranks presentation attributes.
- adds a bare `rect` under edge-label formulas inside a `.edgeLabel` group,
  which the diagram's own `.edgeLabel rect` rule paints;
- removes `<text>` elements that are only `$$...$$`.

Checked by drawing both math fixtures through librsvg, light and dark:
flowchart nodes and edges (x², ½, πr², √(x+3), an overbrace), and sequence
actors α and β plus the note. `merman_engine` tests assert the lifted count,
the inline style, and that no raw `$$` text remains.

Still missing:

- **Labels that mix text and math** keep the text and lose the formula
  ("Solve: $$\sqrt{2+2}$$" shows "Solve:"). Placing a formula inside
  browser HTML layout would be a guess.
- **RaTeX draws `cases` and `bmatrix` on one row.** That is its own output,
  not the lifting.

## Size and speed

- merman linked into `merman_engine_tests`, stripped: 13.9 MB at
  `opt-level=s` without math; **22.4 MB at `opt-level=3` with math**, the
  current default. Size was ruled not a concern on 2026-10-13.
- SVG render time over the realistic fixtures at `opt-level=s`: p50 3.2 ms,
  p95 11.5 ms, max 125 ms.
- Engine creation: 2–3 ms.

## MM3: merman in the viewer (2026-10-14)

What it does:
- With `TINTA_MERMAN` (on by default on Linux), a ```` ```mermaid ```` fence
  and a whole `.mmd` document draw through merman first. The fallbacks are
  the native renderer, then the code block.
- `render.cpp` `layoutMermanDiagram` asks merman for the diagram in the
  active theme's colours:
  - Roles match the native renderer: nodes fill with the code background,
    outlines take the accent, labels the text colour, and lines are text
    mixed into the background. Notes, actors, clusters and activations are
    derived from the same colours.
  - Series colours come from the syntax palette.
  - The body face is resolved through DirectWrite, so librsvg gets the
    installed stand-in (Selawik for Segoe UI). The body size is used too.
- The SVG is cached per source and theme (up to 64), so merman runs only
  when either changes, not on every relayout.
- The SVG is rasterized through the Direct2D SVG path (librsvg on Linux),
  sharing `rasterizeSvg` with SVG images, at exactly the device pixels the
  diagram covers. It is cached in the image cache under that scale.
  - Diagrams centre in the column like native ones.
  - Wide diagrams scroll sideways until the reader's fit-to-width toggle
    shrinks them, as native diagrams do.
  - Print and PDF place the same rectangle at 300 dpi (3.125 pixels per CSS
    pixel, within the largest bitmap).
- The compat layer's `DrawBitmap` used halved bitmap levels on every
  target. They now apply to raster targets only, so PDFs keep full
  resolution: the diagram images went from about 150 to 300–353 ppi, and
  ordinary images in PDFs benefit too. `compat_d2d_wic` checks that a 400 px
  bitmap in a 100 DIP box reaches the PDF as 400×400; with levels on every
  target, that check fails.

Checked:
- **`merman_layout`** (`tests/merman_layout_tests.cpp`, on
  `tests/fixtures/mermaid-merman.md`) asserts that:
  - the four diagrams the native parser rejects are drawn as four bitmaps,
    and the invalid fifth fence stays a code block
  - every bitmap is exactly its rectangle in pixels, at 1× and at 2× zoom
    (the diagram doubles)
  - switching to Wombat draws it again
  - at 420 px a diagram offers fit-to-width, and enabling it shrinks the
    diagram inside the window

  Mutations each fail the matching check: merman off, a fixed 2× raster,
  fit ignored, and a cache key without the theme.
- **Looked at in Paper and Wombat.** The state diagram with
  `<<choice>>`/`<<fork>>`, nested states and a multi-line note, the
  Mermaid 11 flowchart (`n@{ shape: ... }`, `A & B`, `<-->`), the
  top-to-bottom git graph and the class namespaces all render. Before, the
  native parser showed all four as code. The invalid source shows as code.
- ctest 46/46 with merman on.

## Not checked

- **Host text measurement (MM2).** merman's default metrics still decide
  label boxes; librsvg then draws with the real face, so labels can sit
  tight against their boxes.
- **Search and selection over diagram labels (MM4).** Native flowchart
  labels are in `docText`; merman diagrams are images.
- **HTML and DOCX exports** still use the native renderer.
- **Windows (MM5).** merman is off in the cross-built exe.
- Shipping the OFL-1.1 KaTeX font notice, which is packaging work for
  phase 6.
- The first configure on a machine without the 1.95.0 toolchain. Here it was
  already installed.
