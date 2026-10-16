# MM2: diagram labels measured in the face that draws them: validation

Approved by Philip on 2026-10-15 ("MM2 … Approved").

## The problem

merman sizes every node, actor and box from its label's width. Without a
host, it estimates those widths from built-in metrics, but Tinta draws
labels in the theme's face. The estimates fall short wherever the faces
differ. In the default theme (Selawik on Linux, Segoe UI on Windows):

- "WWWWMMMM" overflowed its class box on both sides;
- "WaitingForTheHostToAnswer" filled its state box edge to edge;
- the class box's member lines met its border;
- an edge label ran into its arrowhead.

A wider face (DejaVu Sans, or a serif via `themes.ini`) made each of these
worse. v4.0.5 under Wine shows the same overflow.

## Design

- **merman already has the right shape.** `DeterministicTextMeasurer::
  with_width_callback` is a full measurer built from a host's *line* width.
  merman keeps its own whitespace, `<br>`, wrapping and line-height rules
  and uses the host's widths for every operation. The C ABI exposes only the
  22-operation host protocol. Implementing that in C++ would mean copying
  merman's wrapping policy, and declined operations fall back to estimates,
  which would mix measured and estimated widths in one layout.
- **The patch** `cmake/patches/merman-tinta-text-width.patch`, the third
  Tinta patch:
  - `merman-bindings-core` holds a process-wide line-width hook. Render
    requests with no host text-measurement service install it as a uniform
    `TextMeasurementProfile`.
  - `merman-ffi` exports `tinta_merman_set_text_width`. A negative width
    falls back to merman's estimate for that line.
  - The SVG, parity-SVG and PNG operations all go through it, covering the
    Linux display, the exports and the Windows raster.
- **`merman_engine::setTextWidth`** sets the hook through a trampoline. It
  parses CSS weights and styles and catches anything the host throws.
- **`render.cpp` `labelWidth`** measures with DirectWrite (Pango on Linux,
  through the compat layer). One implementation serves both platforms.
  - **Face:** the first installed family in the label's CSS list, as
    librsvg/fontconfig and resvg pick it. Bundled faces count as
    installed.
  - **Generic names:** fontconfig resolves them on Linux; on Windows they
    map as resvg's font database does (`sans-serif` → Arial, and so on).
  - **Width:** the advance including trailing spaces, so wrap candidates
    measure as merman composes them.
  - **Cache and locking:** results are cached under a mutex, because merman
    may call from any thread.
  - **Lifetime:** installed once per process from `mermanTheme`, holding
    its own reference to the DirectWrite factory.
  - **Opt-out:** `TINTA_MERMAN_ESTIMATED_LABELS=1` keeps merman's estimates,
    for comparisons and the test's control.

## Checked

- **`merman_label_widths`** (`tests/merman_label_width_tests.cpp`, ctest).
  It renders through `mermanDiagramSvg` in DejaVu Sans, reads each shape's
  width (a `<rect>`, or a class box's path) and its drawn label rows from the
  SVG, and measures the widest row in the same face.
  - **Measured:** a state, a bold class title, a wrapped flowchart node and
    a single-line flowchart node each hold their label, within four ems.
  - **The control:** under merman's estimates, the state (209 px against a
    227 px label) and the class box (117 against 134) are too narrow, so the
    check can fail. Flowchart minimum widths hide the error at these
    lengths.
  - **The hook:** a host that reports 40 px a character widens a state to
    at least 240 px. CSS weights parse.
  - **Mutation:** `labelWidth` declining every line fails the state and
    class checks.
- **Fixture** `tests/fixtures/mermaid-label-fit.md` (flowchart, sequence,
  class and state with wide, narrow, wrapping and mixed labels, plus prose,
  a table, a list and a quote), with `mermaid-label-fit-themes.ini` (Paper
  DejaVu and Paper Serif). Rendered in Paper and Paper DejaVu before and
  after:
  - every class, state and sequence label now sits inside its shape;
  - the DejaVu edge label wraps clear of its arrow;
  - narrow labels get narrower boxes.
- **The Windows path:**
  - `build-png` (Linux, merman's PNG) fits the same way, and its
    merman/label tests pass.
  - `tinta.exe` under Wine, v4.0.5 against this build: the class title,
    member line and state name all overflowed before and all fit now.
- **Cost:** 60 distinct flowchart, class, state and sequence diagrams from
  merman's fixtures, 3 runs each.
  - HTML export: median 0.42 s either way (the output differs, so the
    measurer ran).
  - Headless render (`--render-png`): 1,563 → 1,643 ms, i.e. +80 ms or
    about 1.3 ms a diagram.
- ctest 56/56.

## Limits

- Vertical metrics remain merman's: line heights are 1.1 (SVG) or 1.5
  (HTML) em, not the face's ascent and descent.
- Layout choices that aren't measurement stay as they are. A flowchart
  node's minimum width and an edge label's distance from its arrow are
  examples.
- The HTML export's parity SVG is laid out in Tinta's faces. A browser
  without those faces draws the labels in its own fallback.
- Not checked on a real Windows desktop (Segoe UI, ClearType).
