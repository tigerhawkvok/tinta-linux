"""Run mermaid_corpus_probe over merman's fixtures and Mermaid's syntax docs.

Writes fixtures.tsv and docs.tsv to <out dir> and prints pass rates by fixture
origin, by diagram family and by failure reason. Compare two runs with
compare_runs.py.

usage: python3 -I run_corpus.py <probe> <merman>/fixtures <mermaid>/packages/mermaid/src/docs/syntax <out dir>
"""
import csv
import os
import re
import subprocess
import sys
from collections import Counter, defaultdict

probe, fixtures, docs, outdir = sys.argv[1:5]
# Deferred/upstream-only/error fixtures are not diagrams merman renders
SKIP = {"_deferred", "_upstream", "error"}
# docs, cypress and html_demos come from Mermaid's docs and rendering tests;
# pkgtests are parser unit tests, mostly edge cases
REALISTIC = ("docs", "cypress", "html_demos")


def origin(name):
    for key in ("upstream_docs", "upstream_cypress", "upstream_html_demos", "upstream_pkgtests"):
        if name.startswith(key):
            return key.replace("upstream_", "")
    return "merman/other"


def run(paths, extra=()):
    proc = subprocess.run([probe, *extra, "-"], input="\n".join(paths), text=True,
                          capture_output=True, check=True)
    rows = []
    for line in proc.stdout.splitlines():
        parts = line.split("\t")
        parts += [""] * (6 - len(parts))
        rows.append(dict(zip(("path", "index", "kind", "status", "detail", "header"), parts)))
    return rows


mmd = []
for dirpath, _, files in os.walk(fixtures):
    top = os.path.relpath(dirpath, fixtures).split(os.sep)[0]
    if top not in SKIP:
        mmd += [os.path.join(dirpath, f) for f in files if f.endswith(".mmd")]
fixture_rows = run(sorted(mmd))
# Paths are stored relative to the corpus root so runs against different
# checkouts compare row by row
for r in fixture_rows:
    rel = os.path.relpath(r["path"], fixtures)
    r["path"] = rel
    r["family"] = rel.split(os.sep)[0]
    r["origin"] = origin(os.path.basename(rel))

doc_files = sorted(os.path.join(docs, f) for f in os.listdir(docs) if f.endswith(".md"))
# The docs show most examples in mermaid-example fences
doc_rows = run(doc_files, ("--accept", "mermaid-example"))
for r in doc_rows:
    r["path"] = os.path.relpath(r["path"], docs)
    r["family"] = r["path"][:-3]
    r["origin"] = "docs"

os.makedirs(outdir, exist_ok=True)
for name, rows in (("fixtures.tsv", fixture_rows), ("docs.tsv", doc_rows)):
    with open(os.path.join(outdir, name), "w", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=list(rows[0].keys()), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def table(rows, key):
    stats = defaultdict(Counter)
    for r in rows:
        stats[r[key]][r["status"]] += 1
    print(f"{key:26s} {'total':>6s} {'ok':>6s} {'ok%':>5s} {'unrec':>6s} {'fail':>6s} {'crash':>6s} {'hang':>5s}")
    total = Counter()
    for k in sorted(stats, key=lambda k: -sum(stats[k].values())):
        c = stats[k]
        n = sum(c.values())
        total.update(c)
        print(f"{k:26s} {n:6d} {c['ok']:6d} {100 * c['ok'] / n:4.0f}% {c['unrecognized']:6d} "
              f"{c['fail']:6d} {c['crash']:6d} {c['timeout']:5d}")
    n = sum(total.values())
    print(f"{'TOTAL':26s} {n:6d} {total['ok']:6d} {100 * total['ok'] / n:4.0f}% "
          f"{total['unrecognized']:6d} {total['fail']:6d} {total['crash']:6d} {total['timeout']:5d}")
    supported = [r for r in rows if r["kind"] != "none"]
    ok = sum(r["status"] == "ok" for r in supported)
    if supported:
        print(f"families Tinta draws: {ok}/{len(supported)} ok ({100 * ok / len(supported):.0f}%)")


def reasons(rows, n=25):
    counts = Counter()
    for r in rows:
        if r["status"] == "fail":
            counts[(r["kind"], re.sub(r"^line \d+: ", "", r["detail"])[:70])] += 1
        elif r["status"] == "unrecognized":
            counts[("unrecognized", r["header"].split(" ")[0][:40])] += 1
        elif r["status"] in ("crash", "timeout"):
            counts[(r["kind"], f"{r['status']} {r['detail']}")] += 1
    for (kind, why), k in counts.most_common(n):
        print(f"{k:5d}  {kind:14s} {why}")


realistic = [r for r in fixture_rows if r["origin"] in REALISTIC]
print("== merman fixtures by origin")
table(fixture_rows, "origin")
print("\n== merman fixtures from docs, cypress and html demos, by family")
table(realistic, "family")
print("\n== Mermaid syntax docs by page")
table(doc_rows, "family")
print("\n== top failure reasons, Mermaid syntax docs")
reasons(doc_rows)
print("\n== top failure reasons, merman fixtures from docs, cypress and html demos")
reasons(realistic)
