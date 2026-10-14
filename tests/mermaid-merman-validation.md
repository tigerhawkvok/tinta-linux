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

## Not checked

- Any rendering inside Tinta (MM3, after phase 1), on Windows (MM5), or with
  host text measurement (MM2). Default metrics leave labels tight against
  their boxes.
- Shipping the OFL-1.1 KaTeX font notice, which is packaging work for
  phase 6.
- The first configure on a machine without the 1.95.0 toolchain. Here it was
  already installed.
