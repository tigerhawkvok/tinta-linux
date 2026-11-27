# PlantUML review fixes (#248 follow-up)

Validated on Windows with the Release build on 2026-10-16.

## Background

PR #248 (PlantUML diagrams) was merged after this review. The review ran the
merge against current master from three angles: integration, external-process
safety and queue robustness. Each finding was verified independently. These
are the confirmed ones, all fixed here:

| Finding | Severity |
| --- | --- |
| A `@startuml <name>` from the document reached the tool, which names its output file after it (`../` could write images outside the work folder) | high |
| More than 32 diagrams in one document re-rendered forever (evict, re-request, evict) | high |
| A timeout ended only the direct child: Graphviz under java.exe kept running | medium |
| The Locate PlantUML dialog moved the working directory (#253), so a java.exe beside the jar won over PATH | medium |
| One Tinta window's stale-folder sweep could delete another window's work folder | medium |
| The print preview showed diagrams as source and never refreshed | medium |
| Word export under a dark theme: light labels on a transparent PNG on white paper | medium |
| Syntax errors (exit 200) were retried as transient, four tool runs each | low |
| Closing the window waited out a hung render, up to about 20 s | low |
| Word count was 0 for `.puml` files in edit mode | low |
| The PlantUML settings row ran into the panel footer; long tool paths ran past Browse | low |
| Fit-to-width toggles moved between diagrams while one was rendering | low |
| Choosing the tool did not re-render the open document | low |
| A `.puml` saved with a UTF-8 byte order mark never rendered | low |

## Change

- **Tool input.** The tool gets only the first block of a source, with its
  anchor reduced to a bare `@startuml`. The copy ends at the first line
  beginning `@end`, written as a bare `@enduml`, or at any line beginning
  `@start`. PlantUML opens a new, nameable block on any `@start<type>`
  line (`@startmindmap`, `@startjson`, even `@startumlz`). A document can no
  longer name the tool's output files, and later blocks are no longer
  rendered unseen. The image is always `input.png|svg`.
- **Process tree.** The tool starts suspended inside a job object that kills
  its processes when closed. A timeout, or a cancel from the queue, ends the
  whole job.
- **Queue.**
  - Shutdown sets a cancel event, so a hung render ends at once.
  - Exit 200 is permanent.
  - An entry adopted or looked up within the last 60 seconds is never
    evicted, so the open document's diagrams stay cached even past the cap.
  - The stale-folder sweep skips folders whose process is still running.
- **App.**
  - A byte order mark ahead of the anchor (a `.puml` saved "UTF-8 with
    BOM") is skipped when the anchor is matched. The tool's input starts
    with a bare anchor, and the document text, display, copy and saving keep
    the zero-width mark as before.
  - The editor counts `.puml` sources as plain text.
  - The Locate dialog goes through `runOpenFileDialog`.
  - Choosing the tool marks the layout dirty.
  - The reflow timer refreshes an open print preview.
  - Word export composites the diagram over the theme's background, sharing
    the code with the clipboard copy.
  - The Editor settings panel is 590 DIPs tall and card text is clipped.
  - A PlantUML fence takes its fit ordinal before its image exists.

## Automated checks

- **`plantuml_parser`** (updated):
  - `toolInput` cases, including `@startmindmap`, `@startumlz`, `@enduml '
    text`, `@END` and a byte order mark;
  - a document-named, two-block source reaches the tool as one unnamed block;
  - a `@startmindmap Pwned` block cannot name the image (the fake tool now
    names output as the real CLI does, from the block a `@start<type>` line
    opens);
  - a timeout ends a child the fake tool started (new
    `TINTA_FAKE_PLANTUML_CHILD_PIDFILE` switch);
  - exit 200 runs the tool once;
  - four diagrams over a cap of three, laid out ten times, run the tool four
    times and all stay cached;
  - shutdown returns within 1.5 s from a render sleeping 8 s;
  - a day-old folder of a running process survives the sweep and goes once
    the process ends.

  The two retry tests now use exit 1, a failure that may pass next time.
- **`plantuml_in_app`** (new target): with the fake tool, inside the real
  app code,
  - a pending diagram keeps the next diagram's fit key;
  - an open preview shows a render that lands while it is open;
  - a Midnight Word export embeds the diagram on the opaque theme background
    (a transparent fixture stands in for a dark render);
  - the Editor row clears the footer;
  - choosing the tool dirties the layout.
- **`editor_features`**: a `.puml` source counts words.

Each fix was reverted in turn, and its test failed every time:

| Fix reverted | Failing checks |
| --- | --- |
| Tool sees the document's block names | 6 |
| No job object | 1 |
| Shutdown does not cancel | 1 |
| Syntax errors retried | 2 |
| No working-set pin | 2 |
| Sweep ignores live owners | 1 |
| Preview not refreshed | 1 |
| Word gets the transparent PNG | 1 |
| Editor panel at the old height | 1 |
| Pending diagram takes no ordinal | 1 |
| Tool choice does not relay out | 1 |
| `.puml` counted as Markdown | 1 |
| Byte order mark not skipped | 2 |
| `@start<type>` lines not block boundaries | 5 |

The Locate dialog change has no headless test, because it needs the real
file dialog. It is the same one-line pattern the Pandoc row already uses.

The full suite passed 39 of 39, run serially.

## Second review

The fixes were reviewed again before committing. Two findings held:

- The first `toolInput` handled only `@startuml` and an exact `@enduml` as
  block boundaries, so `@startmindmap <name>` could still name the output.
- Blanking the byte order mark in the document text showed three spaces at
  the start of the source.

Both are fixed as described above. Four other claims were refuted.

## Desktop check

The built app ran portably with the fake tool, in Midnight
(`out/pr248-fixes/`, ignored):

- Ctrl+P on `plantuml-preview.md` showed the diagrams in the print palette.
- Settings → Editor showed the PlantUML row above the footer, with the tool
  path cut off before Browse.

## Limits

- Real PlantUML and Java are not installed here. The fake tool stands in
  for them.
- Whether the real CLI itself sanitizes path separators in block names was
  not checked. The tool no longer sees them, so it no longer matters.
- Word export now puts diagrams in light themes on the theme background
  too, as Mermaid diagrams already are.
