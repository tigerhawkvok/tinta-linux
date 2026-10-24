# plantuml-little: bundling a PlantUML renderer?

Evaluated 2026-10-23 at Philip's request: could Tinta Mono bundle
[plantuml-little](https://crates.io/crates/plantuml-little), a Rust
reimplementation of PlantUML, instead of running a PlantUML that is already
installed (Java, or a launcher script)?

**Recommendation: not now.** It is fast and small, but on everyday diagrams
the crates.io release draws six of thirteen wrong. The monorepo's `main`
(re-tested below) fixes one and still draws five wrong, where the external
PlantUML is right. Keep the external tool (#248, 72b1296), and look again as
the crate matures: it is actively developed. It could be a fast path for
diagram types it gets right, but only behind a per-type allowlist and a
file-access guard.

## The crate

- Version 1.2026.2-4 (2026-07-05). It was first published 2026-04-21 and
  has about 1,000 downloads.
- It claims byte-exact SVG parity with Java PlantUML 1.2026.2 for 29 diagram
  types, backed by 337 reference tests.
- Licence: GPL-3.0, LGPL-3.0, Apache-2.0, EPL-2.0 or MIT, at our choice.
  MIT or Apache-2.0 suits the fork.
- Its crates.io repository link (github.com/Actrium/plantuml-little)
  returns 404. The crate now lives in the Actrium/supramark monorepo under
  `crates/plantuml-little`, next to its Graphviz and font-metrics crates, and
  that repository is active (pushed 2026-10-23).
- **Output:** SVG only. Tinta already turns diagram SVG into PNG (merman's
  resvg path, librsvg), so that would fit.
- **Layout:** Graphviz-based types (class, component, state, use case) link
  `graphviz-anywhere`. Its build script downloads a prebuilt native
  library from a GitHub release at build time, unpinned and unverified. We
  would pin it with a SHA-256 and pass `GRAPHVIZ_ANYWHERE_DIR`, as with the
  merman tarballs.
- **Windows:** an asset for x86_64-pc-windows-msvc exists. The crate's README
  says that build has no pango, so Graphviz estimates text widths and
  class, component and state layouts drift. It is not known whether it
  links against our static CRT.
- **Safety:** there is no sandbox (a stated non-goal). `!include` reads local
  files relative to the document (or the working directory), and `<img:...>`
  reads any local path into the SVG. Remote fetching is the optional
  `remote` feature and would stay off, but an exported HTML file could
  still carry the contents of local files. Bundling it would need a guard
  that refuses includes and images outside the document's folder. Today's
  external PlantUML runs in its own SANDBOX profile.

## Measured

A prototype (`pul-proto`): plantuml-little with only `metrics-ttf-parser`
(no CLI, no `remote`), Rust 1.95, release, `opt-level=s`, LTO, stripped.
- **Build:** 47 s from clean.
- **Size:** 8.7 MB, Graphviz linked statically; it needs the system's
  libexpat, zlib and libstdc++.
- **Corpus:** thirteen diagrams. Seven are the PlantUML fences of
  `tests/fixtures/plantuml-diagrams.md` and `plantuml-preview.md` plus
  `plantuml-standalone.puml`. Six are written for this evaluation: state,
  component, use case, mindmap, Gantt and JSON.
- **Speed:** each conversion took 0.1 to 4.2 ms. Java PlantUML 1.2026.9
  (Linuxbrew, one JVM per diagram, as Tinta runs it) took 287 to 461 ms.
- **Output:** none byte-equal; the versions and fonts differ. Rasterised
  side by side with rsvg-convert:

| Diagram | plantuml-little |
|---|---|
| Sequence (three) | Matches |
| Class, association | Matches |
| State | Matches |
| JSON | Matches |
| Class, `extends` | **Wrong**: `class Dog extends Animal` becomes a class named "Dog extends Animal"; both inheritance arrows are lost |
| Activity | **Wrong**: the merge diamond and one flow line are missing |
| Component, package | **Wrong**: no arrows; another layout |
| Use case | **Wrong**: drawn as a sequence diagram |
| Gantt | **Wrong**: `starts at [Port]'s end` ignored, no task table |
| Invalid syntax | **Wrong**: draws a class "A" instead of reporting the error |
| Mindmap | Right content, another style (a yellow root) |

Six of the thirteen are wrong, which is not the parity the crate claims.
The diagrams are ordinary: `extends`, a use case and a component package
are the first things the PlantUML documentation shows.

## Re-tested on the monorepo's main (2026-10-23)

Philip pointed at github.com/Actrium/supramark/tree/main/crates/plantuml-little.
`main` was at d9e28b3 (2026-10-23). It still says version 1.2026.2-4, but it
carries fixes since the crates.io release: hangs and unbounded recursion
(#277, today), non-ASCII text, and CJK metrics.

The same prototype, built against that source (path dependency, the same
features), took 46 s and came to 8.8 MB, at the same speed. Against the
crates.io release:
- **The activity diagram is fixed:** the merge diamond and its lines match
  Java now.
- **The other twelve SVGs are byte-identical** to the crates.io release's, so
  the other five failures stand.

Why they fail:
- **`class Dog extends Animal`:** the crate's own fixtures have no class
  diagram using `extends`, so it is untested; the parser takes the whole
  line as the class name.
- **Use case:** `Reader --> (Open file)` uses the parenthesis shorthand, the
  form PlantUML's use-case documentation introduces first. The crate's
  fixtures declare `usecase "..." as UC1` explicitly, and without that it
  detects a sequence diagram.
- **Component package, Gantt dependency, invalid syntax:** unchanged.

**Safety is unchanged:** there is still no sandbox or file-access option.
`convert`, `convert_with_base_dir` and `convert_with_input_path` all read
`!include` and `<img:...>` from disk.
