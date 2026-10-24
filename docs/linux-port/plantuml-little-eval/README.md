# plantuml-little corpus evaluation

The harness behind `../plantuml-little-evaluation.md`. It scores
plantuml-little against Java PlantUML on the examples from plantuml.com's
documentation, using the crate's own reference setup and comparator, so a
future version can be re-tested in a few minutes.

Everything is written under `$PUL_EVAL` (default `~/.cache/tinta-pul-eval`);
nothing here touches the Tinta build. Needs Rust 1.95, Java 17+, Node, `rsvg-convert`,
and for the contact sheets the system Python with Pillow (`/usr/bin/python3`).

```sh
sh setup.sh [REF]                  # supramark at REF (default main), jar, runner
python3 -I corpus.py               # ~960 examples from 27 docs pages (cached HTML)
python3 -I java_refs.py            # Java references, 8 at a time (~2 min)
python3 -I little_run.py wasm      # plantuml-little, Graphviz as in the references
python3 -I little_run.py native    # ... and as a bundled Tinta would ship it
python3 -I score.py wasm           # buckets, per page
python3 -I text_split.py           # how much of 'text' is only split differently
python3 -I clusters.py             # wrong types, error messages, crashes
python3 -I constructs.py PAGE      # constructs over-represented in a page's failures
/usr/bin/python3 -I sheet.py BUCKET N [SEED]   # Java | plantuml-little contact sheet
sh checker_check.sh                # the comparator against the crate's own references
```

Windows (needs Tinta's cross toolchain from `platform/windows/build-windows.sh`
in `$TINTA_CROSS`, default `~/.cache/tinta-cross`, and Wine):

```sh
sh win_build.sh                    # pul-run.exe, static CRT, prebuilt Windows Graphviz
python3 -I wine_run.py             # the corpus under Wine (scratch prefix)
python3 -I win_drift.py            # Windows vs Linux, and each vs Java
```

## How it scores

- **References:** PlantUML 1.2026.2 (SHA-256 pinned), `-tsvg -pipe`, the SANDBOX profile.
  - Graphviz is the crate's `scripts/wasm-dot-wrapper.sh`: the same wasm Graphviz 14.1.5 the crate links in its tests.
  - Fonts: DejaVu as the generic families.
  - This matches the crate's `tests/reference/VERSION`.
  - `checker_check.sh` confirms it reproduces the crate's committed references, and that two different diagrams fail.
- **Comparator:** `svg-cmp` runs the crate's `tests/support/svg_compare.rs` unchanged. A diagram passes if it is byte-equal, equal after the crate's normalisation, or within its ±2.51 pt numeric tolerance.
- **Buckets for diagrams that differ:**
  - wrong diagram type (`data-diagram-type`);
  - different drawn text;
  - shape counts or size more than 10% off;
  - otherwise "close".
- **Calibration:** the buckets are only a sort. Look at `sheet.py` samples before quoting a bucket as right or wrong.
- **Corpus:** the scraped examples stay in `$PUL_EVAL`. They are plantuml.com's content and are not committed.
