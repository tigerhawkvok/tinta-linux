#!/usr/bin/env python3
"""Exported PDF pages keep their ink inside the page margins.

A table split between rows, a quote or a callout running across a page break
must not draw its rules into the margin on either side of the break: page 1
once ended with an empty slice of a table row and the table's column rules
ran on to the paper's edge, and the next page's rules started above its top
margin. Tinta exports the fixture (private configuration, so the default
19.05 mm margins), pdftoppm rasterises every page at 72 dpi, and each page's
non-white pixels must lie within the margins.

Usage: pdf_margin_check.py TINTA FIXTURE.md OUT_DIR
Needs pdftoppm (poppler-utils) and Pillow. Exit status 1 on any failure.
"""
import glob
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from render_fixtures import export_pdf  # noqa: E402

from PIL import Image  # noqa: E402

MARGIN_PX = 19.05 / 25.4 * 72  # the default margin at 72 dpi: 54 px
SLACK_PX = 2  # anti-aliasing and the 1.5 DIP edge a rule on the break draws with


def main():
    tinta, fixture, out = (os.path.abspath(a) for a in sys.argv[1:4])
    os.makedirs(out, exist_ok=True)
    pdf = os.path.join(out, os.path.splitext(os.path.basename(fixture))[0] + ".pdf")
    code, ms, err = export_pdf(tinta, fixture, pdf, "Paper")
    if code != 0 or not os.path.exists(pdf):
        print(f"FAIL export: exit {code}: {err}")
        return 1
    for old in glob.glob(os.path.join(out, "page-*.png")):
        os.remove(old)
    subprocess.run(["pdftoppm", "-r", "72", "-png", pdf, os.path.join(out, "page")], check=True)
    pages = sorted(glob.glob(os.path.join(out, "page-*.png")))
    failures = 0
    if len(pages) < 2:
        print(f"FAIL pages: {len(pages)}; the fixture must break across pages")
        return 1
    for path in pages:
        page = Image.open(path).convert("L")
        # Ink: anything clearly darker than the white paper
        ink = page.point(lambda v: 255 if v < 245 else 0).getbbox()
        name = os.path.basename(path)
        if not ink:
            print(f"FAIL {name}: blank page")
            failures += 1
            continue
        left, top, right, bottom = ink
        limit_top = MARGIN_PX - SLACK_PX
        limit_bottom = page.height - MARGIN_PX + SLACK_PX
        ok = top >= limit_top and bottom <= limit_bottom
        print(f"{'PASS' if ok else 'FAIL'} {name}: ink rows {top}..{bottom} of {page.height}, "
              f"margins allow {limit_top:.0f}..{limit_bottom:.0f}")
        failures += not ok
    print(f"{len(pages)} pages, {failures} failures ({ms:.0f} ms export)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
