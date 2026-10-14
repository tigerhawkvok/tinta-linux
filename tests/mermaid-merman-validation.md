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

## Size and speed

- merman linked into `merman_engine_tests`, stripped: 13.9 MB.
- SVG render time over the realistic fixtures at `opt-level=s`: p50 3.2 ms,
  p95 11.5 ms, max 125 ms.
- Engine creation: 2–3 ms.

## Not checked

- Any rendering inside Tinta (MM3, after phase 1), on Windows (MM5), or with
  host text measurement (MM2). Default metrics leave labels tight against
  their boxes.
- The `math` capability: its size and its OFL font notices.
- The first configure on a machine without the 1.95.0 toolchain. Here it was
  already installed.
