# PlantUML on Linux

Validated on Linux (Xvfb, X11 backend, and headless exports) on 2026-10-23,
with PlantUML 1.2026.9 from Linuxbrew (a `plantuml` launcher script that
runs Linuxbrew's OpenJDK, Graphviz alongside). Upstream's PlantUML support
(#248, Tinta 3.8.1) is ported in 72b1296 and 19b4dc6.

## What Linux needed

- **The compat layer** (6a143cc, aed9b33) gained events,
  `WaitForMultipleObjects`, the environment-block calls,
  `GetFileAttributesExW` and job objects. A job owns process groups: a
  process created `CREATE_SUSPENDED` (the create, assign, resume pattern)
  starts in its own group, so a timeout or a cancel ends java and Graphviz
  with it. A process opened by pid now reports its end, including as a
  zombie.
- **Paths:** upstream joined work paths with `\`, which is part of a file
  name on Linux: one render queue directory became `work\<key>`, and no
  render started. The joins use `plantuml::kPathSeparator` (`/` off
  Windows).
- **Discovery:** the KDE session's PATH lacks Linuxbrew, which adds itself
  in shell start-up files a desktop launch never reads. When PATH has no
  `plantuml`, Tinta Mono looks in the known install locations: Linuxbrew
  (system and per-user), Homebrew, `/usr/local/bin` and `/usr/bin`, then the
  distributions' `plantuml.jar` with Java from PATH.
- **The Browse dialog:** the compat file chooser now reads a Windows
  `*.exe` filter as "all files", since Linux programs carry no suffix, and
  `pandoc.exe` as also matching `pandoc`. Before, Settings could not pick
  the `plantuml` script, nor Pandoc.
- **Desktop integration:** `--register` and Set as default add a
  `text/x-plantuml` MIME type for `*.puml` and `*.plantuml`, and make Tinta
  Mono its default. The generated desktop entry is named Tinta Mono (it
  still said Tinta) and mentions PlantUML.
- **Wording:** Settings names the launcher `plantuml`, not `plantuml.exe`.
  The Default viewer card, the Linux first-run prompt and its confirmation
  list `.puml` in every language.

## Checked

- `plantuml_parser`, `plantuml_parser_selfcheck` and `plantuml_in_app`
  (ctest) are upstream's suites. They run against the fake PlantUML tool,
  with the suite made portable: narrow paths, the PATH separator, `getpid`
  and a shell background child. Among other things they check:
  - the sandbox environment, timeouts, exit codes and the render queue;
  - a timeout ending the tool's own child, through the job;
  - the dark-theme Word export embedding a PNG on the theme background.

  A new check: with no `plantuml` on PATH, the first known install
  location that exists is used.
- `linux_register` (ctest) now also expects `text/x-plantuml` in the
  desktop entry and `mimeapps.list`, the Tinta Mono name, GIO's default for
  `text/x-plantuml`, and GIO typing a `.puml` file as `text/x-plantuml`.
- **Real PlantUML, headless:** `--exporthtml` and `--exportpdf` of
  `tests/fixtures/plantuml-diagrams.md` take 2.0 s and 1.9 s. The HTML
  meets the fixture's real-tool gate: four diagram blocks (sequence, class
  and activity, plus the Mermaid control) and two fallbacks to source (the
  anchorless fence and the invalid syntax). The PDF draws the three
  diagrams. With the KDE session's PATH (no Linuxbrew), the export finds
  the tool through the known locations and draws the same three.
- **The built app on Xvfb, with the KDE session's PATH:**
  `tests/fixtures/plantuml-preview.md` renders its sequence and class
  diagrams in the light and dark themes, the dark one on the dark palette.
  Settings > Editor fits all eight cards in 640 px, and the PlantUML card
  shows the resolved `/home/linuxbrew/.linuxbrew/bin/plantuml`.
  Screenshots are in `build/plantuml-gui/`.
- Full ctest: 64/64.

## Limitations

- Not yet run on Windows; the shared-code changes keep the Windows
  behaviour (`\` separators there, the same file filters).
- No `.puml` file was opened from a file manager on the live KDE session;
  `linux_register` checks the MIME and default wiring through GIO.
- The Flatpak build cannot reach a host PlantUML (sandbox); untested
  there.
