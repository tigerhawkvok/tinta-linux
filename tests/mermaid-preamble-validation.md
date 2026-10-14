# Mermaid frontmatter and directive validation

Validated on Linux (Ubuntu 25.10, GCC 15.2, Clang 22.1) on 2026-10-13. There
was no Windows machine and no Linux app build yet, so nothing in this record
was drawn on screen. See *Not checked* below.

## Problem

A Mermaid fence renders in md-viewer but shows as a plain code block in Tinta
when the diagram opens with a YAML frontmatter block (`---` / `title:` or
`config:` / `---`). Mermaid's own documentation now does this routinely.
`mermaidext::detectKind` skipped the `---` lines, but not the keys between
them, so it read `title:` as the diagram keyword and returned `None`. Multi-line
`%%{ }%%` directives failed the same way, and `flowchart-elk` was not
recognized as a flowchart.

## Changes

- `mermaid::stripPreamble` blanks a leading frontmatter block and every
  `%%{ }%%` directive, keeping newlines so error line numbers still match the
  source.
- `detectKind`, `mermaid::parse` and `mermaidext::build` strip the preamble
  first. Every caller benefits: the viewer, HTML export and DOCX export all go
  through these three functions.
- `flowchart-elk` is accepted as a flowchart header.

## Automated checks

`ctest --test-dir build --output-on-failure` on Linux: **6/6 passed**.

- `mermaid_parser` (`testPreamble`):
  - frontmatter before flowchart, sequence and state diagrams
  - a multi-line directive before a pie chart
  - error line numbers after frontmatter
  - an unclosed frontmatter block, which must still fall back
  - line-count preservation
  - `flowchart-elk`
- `mermaid_fences` runs `tests/fixtures/mermaid-preamble.md` through Tinta's
  Markdown parser and the same render-or-fall-back decision `layoutCodeBlock`
  makes. The fixture is a mixed document: headings, emphasis, a link, a
  table, a YAML code block that must stay text, and diagrams at the top level,
  in a blockquote and in a list item.
  - Checking the checker: built against the pre-fix Mermaid sources, the same
    test fails fences 1–8 and passes fence 9 (the unclosed control).

## Corpus

The diagrams were run through the same decision with a probe outside the repo:

| Corpus | Rendering before | Rendering after |
|---|---|---|
| Mermaid `develop` syntax docs (v12.2.0), 548 examples | 272 (50%) | 336 (61%) |
| merman fixtures from Mermaid docs, cypress and html demos, 1,908 | 892 (47%) | 1,228 (64%) |
| all merman fixtures, 3,796 | 2,009 | 2,460 |

A row-by-row diff shows **no regressions**. The only status changes are
"unrecognized → rendering" (64 docs examples, 451 fixtures) and
"unrecognized → recognized, still falls back on later syntax" (7 and 189).

## Not checked

- Visual output. Diagrams after a preamble take the existing layout paths, but
  none was drawn: there is no Linux app yet and no Windows machine was
  available.
- The frontmatter `title` is dropped, not drawn. Mermaid shows it above the
  diagram.
- HTML export recognizes only a lowercase `mermaid` fence (`export_html.cpp`
  compares case-sensitively), while the viewer accepts any case. Left
  unchanged: that file cannot be built on Linux yet.
