"""Diff two run_corpus.py output directories row by row.

Prints the status transitions and every diagram that rendered before but no
longer does. Exits 1 when there is such a regression.

usage: python3 -I compare_runs.py <before out dir> <after out dir>
"""
import csv
import os
import sys
from collections import Counter

before_dir, after_dir = sys.argv[1:3]
regressed = False
for name in ("fixtures.tsv", "docs.tsv"):
    def load(directory):
        with open(os.path.join(directory, name)) as fh:
            return {(r["path"], r["index"]): r for r in csv.DictReader(fh, delimiter="\t")}

    before, after = load(before_dir), load(after_dir)
    transitions = Counter()
    for key, old in before.items():
        new = after.get(key)
        status = new["status"] if new else "missing"
        if status != old["status"]:
            transitions[(old["status"], status)] += 1
        if old["status"] == "ok" and status != "ok":
            regressed = True
            detail = new["detail"] if new else ""
            print(f"  REGRESSION {key[0]}#{key[1]}: ok -> {status} {detail}")
    summary = ", ".join(f"{a} -> {b}: {n}" for (a, b), n in transitions.most_common()) or "none"
    print(f"{name}: {len(before)} diagrams; changes: {summary}")
sys.exit(1 if regressed else 0)
