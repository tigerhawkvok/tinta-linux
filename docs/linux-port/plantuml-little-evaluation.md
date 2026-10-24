# plantuml-little: bundling a PlantUML renderer?

Evaluated 2026-10-23 at Philip's request: could Tinta Mono bundle
[plantuml-little](https://crates.io/crates/plantuml-little), a Rust
reimplementation of PlantUML, instead of running a PlantUML that is already
installed (Java, or a launcher script)?

**Recommendation: don't bundle it, and don't take on fixing it wholesale.**
Keep the external tool (#248, 72b1296). Send upstream the few small,
clear-cut fixes listed under "PR targets" below, and re-run the harness as
the crate matures; it is actively developed.

On the 958 examples in PlantUML's own documentation (corpus run, below),
`main` matches Java on 94 (10%). By a generous structural measure it gets
at most a third roughly right. The failures do not cluster: beyond a few
fixable groups, about 500 are spread across every diagram type. On Windows,
the prebuilt Graphviz breaks the layout of 99 more diagrams and makes the
exe depend on the Visual C++ runtime.

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
  class, component and state layouts drift. Measured in the corpus run
  below: the drift is real and the library brings in the dynamic Visual C++
  runtime.
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

## Corpus run: PlantUML's documentation (2026-10-23)

The question was whether the failures cluster in a few constructs, which
would make upstream fixes worthwhile, or are spread out. The harness is in
`plantuml-little-eval/` (README there); a re-run takes a few minutes.

### Setup

- **Corpus:** every example (`<textarea>`) on 27 diagram pages of plantuml.com,
  deduplicated: 958 diagrams. The board, chart, packet and Chen pages
  returned 404.
- **References:** Java PlantUML 1.2026.2 (the crate's target, SHA-256
  pinned), SANDBOX profile.
  - Graphviz is the crate's own wasm build (14.1.5), through its wrapper script.
  - Fonts: DejaVu.
  - This is the crate's documented reference setup. It reproduces the crate's
    committed references: 19 of 20 sampled fixtures.
  - **The JDK matters.** The system OpenJDK 21 matches only 2 of 20 (text
    measures differently); Linuxbrew's OpenJDK 27 is the one that reproduces
    them. The harness now checks this before it runs.
  - Java rendered 951. Five came back as error images and two as nothing.
- **Comparator:** the crate's own `tests/support/svg_compare.rs`, unchanged.
  A diagram passes if it is byte-equal, equal after its normalisation, or
  within its ±2.51 pt tolerance.
- **Backends:** the native `graphviz-anywhere` (what Tinta would ship) and
  the wasm one give byte-identical output for 886 of 887 diagrams. The native
  numbers are therefore the parity numbers too.

### Results (main at d9e28b3, of the 951 Java rendered)

| Outcome | Diagrams | By eye (seeded random samples) |
|---|---:|---|
| Passes the crate's comparator | 94 (10%) | right |
| Same type, text, shape counts and size (±10%), but not a pass | 57 | 6 of 8 acceptable; 2 wrong (participant order reversed; a colour lost and arrows mirrored) |
| Same text, shapes or size off by more than 10% | 124 | 3 of 8 acceptable; 5 wrong (tangled loop, a node under the wrong parent, groups or highlights lost) |
| Different drawn text | 548 | 1 of 8 acceptable (Gantt calendars missing, regex and Salt misparsed, notes lost) |
| Another diagram type detected | 58 | 0 of 6 acceptable |
| plantuml-little error | 68 | |
| plantuml-little crash | 2 | |

- **How many are right:** between 94 (10%, exact) and about 305 (32%,
  counting every diagram that draws the right text and type as right).
  The samples put the real figure nearer a fifth.
- **The text bucket is real:** only 4 of the 548 draw the same characters
  split differently. 490 differ by more than 5% of their characters.
- **Speed:** plantuml-little ran all 958 in about 1 s (8 processes; median
  1.3 ms, 95th percentile 6 ms, worst 150 ms). Java took 110 s; median
  833 ms, with Graphviz running through Node.

### Where the failures come from

The clusters a few fixes could address:
- **Diagram-type detection, 92 diagrams:**
  - 58 detected as another type. Use case, component and deployment
    ("DESCRIPTION") came out as sequence (22) or class (18). Class came out
    as description (6), activity as description (4), and others.
  - 34 more fail with "Unsupported diagram type: unknown" (creole 8, timing 6,
    activity 5, commons 4).
  - Example: the use-case parenthesis shorthand, `Reader --> (Open file)`.
- **Gantt, 99 of 99:** the documentation's Gantt examples come out as an
  empty "D1 D2" header without Java's calendar, so Gantt is essentially
  unimplemented beyond basics.
- **Malformed SVG, 24 diagrams:**
  - The legacy activity parser keeps the quotes in node names
    (`id="start-to-"First Action""`), and the class and state renderers
    don't escape link IDs, while the component renderer does (`xml_escape`).
  - librsvg, and so Tinta, refuses such SVG and draws nothing.
  - Java writes `start-to-First Action`, so the fix can match it exactly.
- **Crash, 2:** a panic in `layout/sequence_teoz/builder.rs:2705` (teoz
  sequence examples).
- **Silent acceptance:** where Java reports an error (7), plantuml-little
  draws something for 6.

Everything else, about 500 diagrams, is spread across every type:
- sequence: 57 of 89 wrong;
- class: 67 of 86;
- activity, state, timing, salt, creole, nwdiag and others in proportion.

No construct explains most of a type's failures. In sequence diagrams the
most over-represented ones (quoted names, `<<stereotypes>>`, teoz, styled
text) each appear in only 10-40% of the failing examples. In class diagrams
it is packages, enums, `hide`/`remove` and notes. That is the "long tail"
case: fixing it is an open-ended port, not a few PRs.

### Windows

- **Build:** cross-built with Tinta's toolchain (lld-link, xwin CRT and SDK,
  `+crt-static`) and the crate's prebuilt Windows Graphviz (v0.2.6).
  - Result: a 9.9 MB exe.
  - **It imports `MSVCP140.dll`, `VCRUNTIME140.dll` and the UCRT DLLs**, which
    come from the prebuilt `graphviz_api.lib`. Tinta's exe imports none of
    them; it is fully static. Bundling would need Graphviz built from source
    with the static CRT.
- **Layout, under Wine:**
  - **99 of 887 outputs differ from the Linux build**, all Graphviz types
    (class 25, state 24, deployment 21, ER 8, object 6, component 5, use case 3, ...).
  - 43 of them change size by more than 5%, up to 1.83 times wider.
  - Exact passes fall from 94 to 75.
  - By eye they are broken: a class diagram loses `HashMap` and overlaps,
    deployment labels pile up, ER edges loop, state edges and labels run out
    of the frame.
  - Wine runs the same Windows Graphviz code, whose text widths are
    estimates without pango, so the result should carry over to Windows. Not
    checked on a real Windows machine.

### PR targets

Small, clearly scoped and useful to the crate whether or not Tinta ever
bundles it. Each is drafted for Philip to review and send; nothing has been
sent.
1. **A file-access policy.**
   - An option to refuse local reads, or confine them to the document's
     folder.
   - It would cover `!include` and its variants (`src/preproc/mod.rs`, three
     sites), included archives (`src/preproc/include.rs`) and `<img:...>`
     (`src/render/svg_richtext.rs`).
   - Needed before anyone embeds the crate in a viewer.
2. **Malformed link IDs (24 diagrams):** strip the quotes as Java does, and
   escape IDs in the class, state and activity renderers.
3. **The teoz panic (2 diagrams).**
4. **Maybe: description-type detection (92 diagrams).** It is the largest
   single cluster, but a medium-sized change.
