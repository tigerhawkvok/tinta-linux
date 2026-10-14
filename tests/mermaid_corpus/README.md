# Mermaid corpus coverage

This harness measures how many real-world Mermaid diagrams Tinta's native
renderer draws, and how many fall back to a code block. It is a development
tool, not part of `ctest`: the corpora are external, and their sizes change
whenever upstream changes.

The probe makes the same decision as `layoutCodeBlock` in `src/render.cpp`:
`detectKind`, then either `mermaid::parse` plus layout, or `mermaidext::build`.
Each diagram runs in a forked child with a 5 s alarm, so crashes and hangs are
reported rather than ending the run. Linux only.

## Corpora

- **merman fixtures.** [merman](https://github.com/Latias94/merman), the Rust
  Mermaid port that md-viewer uses, ships about 4,000 `.mmd` files drawn from
  Mermaid's docs, cypress rendering tests, HTML demos and parser unit tests.
  The `pkgtests` origin is mostly parser edge cases. The *realistic* subset is
  `docs` + `cypress` + `html_demos`.
- **Mermaid's syntax docs.** The examples in
  `packages/mermaid/src/docs/syntax/*.md`, in `mermaid` and `mermaid-example`
  fences.

```sh
mkdir -p ~/corpus/merman ~/corpus/mermaid
curl -sSL https://codeload.github.com/Latias94/merman/tar.gz/3a4b8c393939b69372a8f05a2b971f7cdf6ae529 \
  | tar xz -C ~/corpus/merman --wildcards '*/fixtures/*'
curl -sSL https://codeload.github.com/mermaid-js/mermaid/tar.gz/4e3a85e6d46a0326379e04db3463bb3b53a3ca94 \
  | tar xz -C ~/corpus/mermaid --wildcards '*/packages/mermaid/src/docs/syntax/*'
```

## Run

```sh
cmake --build build --target mermaid_corpus_probe
python3 -I tests/mermaid_corpus/run_corpus.py build/mermaid_corpus_probe \
  ~/corpus/merman/merman-*/fixtures \
  ~/corpus/mermaid/mermaid-*/packages/mermaid/src/docs/syntax \
  build/out/mermaid-corpus/after
# Before/after a parser change: run once on each build, then
python3 -I tests/mermaid_corpus/compare_runs.py build/out/mermaid-corpus/before build/out/mermaid-corpus/after
```

`compare_runs.py` exits 1 if any diagram that rendered before no longer does.

## Baseline (2026-10-13, at the commits pinned above)

| Corpus | Diagrams | Rendering | In families Tinta draws |
|---|---|---|---|
| Mermaid syntax docs | 548 | 336 (61%) | 336 / 412 (82%) |
| merman, realistic origins | 1,908 | 1,228 (64%) | 1,228 / 1,809 (68%) |
| merman, all origins | 3,796 | 2,460 (65%) | 2,460 / 3,639 (68%) |

There were no crashes and no hangs.

The largest remaining gaps in the realistic merman set:

| Count | Family | Reason |
|---|---|---|
| 154 | flowchart | `n@{ shape: ... }` attribute syntax |
| 77 | gitGraph | only left-to-right graphs (`TB`/`BT` unsupported) |
| 67 | flowchart | edges Tinta can't parse: `A & B --> C`, `<-->`, `~~~`, `--->`, `>text]` nodes |
| 32 | state | composite states |
| 26 | xychart | x-axis forms |
| 23 | flowchart | unterminated quoted labels (Markdown strings) |

Families Tinta does not draw at all include zenuml, treeView, ishikawa,
cynefin, venn, wardley, usecase, eventmodeling, swimlane, agentflow and
railroad. Most are Mermaid 11/12 betas.
