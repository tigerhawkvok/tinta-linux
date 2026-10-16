#!/usr/bin/env python3
"""Printing on Linux, on a private D-Bus session and Xvfb: Ctrl+P opens the
preview, Enter the GTK print dialog (in-process: GDK_DEBUG=no-portals),
which prints the PDF Tinta exports.

  dialog    the system print dialog opens over the window
  printed   "Print to File" writes a PDF with as many pages as --exportpdf
            of the same document gives
  cancel    a cancelled dialog leaves no temporary PDF behind

HOME and the Documents folder point into the output directory: "Print to
File" writes there, never into the real home. Positions assume GTK 4's
print dialog at 625 x 516 (the printer list's first row, the buttons at
its bottom right).

Usage: print_check.py TINTA DOC [--out DIR]
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def pdf_pages(path):
    """Page count: pdfinfo (poppler) when present. Cairo compresses its page
    tree into object streams, where a byte search cannot see it."""
    if shutil.which("pdfinfo"):
        info = subprocess.run(["pdfinfo", path], capture_output=True, text=True).stdout
        found = re.search(r"^Pages:\s+(\d+)", info, re.M)
        return int(found.group(1)) if found else 0
    data = open(path, "rb").read()
    counts = [int(n) for n in re.findall(rb"/Type\s*/Pages\b[^>]*?/Count\s+(\d+)", data)]
    return max(counts) if counts else 0


def inner(args):
    from gui_check import X
    from render_fixtures import isolated_env

    out = args.out
    results = []

    def report(name, ok, detail):
        results.append(ok)
        print(f"{'PASS' if ok else 'FAIL'} {name}: {detail}", flush=True)

    def environment(tag):
        env = isolated_env(os.path.join(out, ".home-" + tag), None, "")
        with open(os.path.join(env["XDG_CONFIG_HOME"], "Tinta", "settings.ini"), "a") as f:
            f.write("windowWidth=900\nwindowHeight=700\nwindowMaximized=0\ncheckUpdates=0\n")
        home = os.path.join(out, "home-" + tag)
        os.makedirs(os.path.join(home, "Documents"), exist_ok=True)
        with open(os.path.join(env["XDG_CONFIG_HOME"], "user-dirs.dirs"), "w") as f:
            f.write(f'XDG_DOCUMENTS_DIR="{home}/Documents"\n')
        env.update(HOME=home, GDK_DEBUG="no-portals", TINTA_TRACE_GTK="1", GDK_BACKEND="x11")
        env.setdefault("GSK_RENDERER", "cairo")
        env.pop("WAYLAND_DISPLAY", None)
        return env, home

    x = X()

    def dialog():
        return next((w for w in x.toplevels() if x.title(w[0]) == "Print"), None)

    def open_dialog(env, tag):
        log = open(os.path.join(out, f"tinta-{tag}.log"), "w")
        proc = subprocess.Popen([args.tinta, args.doc], env=env, stdout=log, stderr=log)
        start = time.monotonic()
        while time.monotonic() - start < 20 and not x.toplevels():
            time.sleep(0.05)
        time.sleep(1.5)
        _, gx, gy, gw, gh = max(x.toplevels(), key=lambda t: t[3] * t[4])
        x.click(1, gx + gw // 2, gy + gh // 2)
        time.sleep(0.2)
        x.key("Control_L", "p")
        time.sleep(1.5)
        x.key("Return")
        start = time.monotonic()
        while time.monotonic() - start < 10 and dialog() is None:
            time.sleep(0.1)
        time.sleep(1.0)
        return proc

    def stop(proc):
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

    def temp_pdfs(pid):
        tmp = os.environ.get("TMPDIR", "/tmp")
        return [f for f in os.listdir(tmp) if f.startswith(f"tinta-print-{pid}-")]

    # The reference: the same document's PDF export
    env, home = environment("export")
    reference = os.path.join(out, "export.pdf")
    subprocess.run([args.tinta, args.doc, "--exportpdf", reference], env=env, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, timeout=60)
    expected = pdf_pages(reference) if os.path.exists(reference) else 0

    env, home = environment("print")
    proc = open_dialog(env, "print")
    d = dialog()
    report("dialog", d is not None, f"{(d[3], d[4]) if d else None}")
    if d is not None:
        x.grab(0, 0, 1600, 1000).save(os.path.join(out, "1-dialog.png"))
        x.click(1, d[1] + 84, d[2] + 92)  # the printer list's first row: Print to File
        time.sleep(1.0)
        x.click(1, d[1] + d[3] - 45, d[2] + d[4] - 30)  # Print
        time.sleep(5.0)
    printed = os.path.join(home, "Documents", "output.pdf")
    pages = pdf_pages(printed) if os.path.exists(printed) else 0
    leftover = temp_pdfs(proc.pid)
    report("printed", pages > 0 and pages == expected and not leftover,
           f"{pages} pages printed, {expected} exported, temporary files left {leftover}")
    stop(proc)

    env, home = environment("cancel")
    proc = open_dialog(env, "cancel")
    d = dialog()
    if d is not None:
        x.click(1, d[1] + d[3] - 115, d[2] + d[4] - 30)  # Cancel
        time.sleep(2.0)
    leftover = temp_pdfs(proc.pid)
    report("cancel", d is not None and dialog() is None and not leftover,
           f"dialog {'closed' if dialog() is None else 'still open'}, temporary files left {leftover}")
    stop(proc)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("tinta")
    parser.add_argument("doc")
    parser.add_argument("--out", default="build/print-check")
    parser.add_argument("--inner", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    args.tinta = os.path.abspath(args.tinta)
    args.doc = os.path.abspath(args.doc)
    args.out = os.path.abspath(args.out)
    if args.inner:
        results = inner(args)
        print(f"{sum(results)}/{len(results)} checks passed")
        return 0 if results and all(results) else 1
    shutil.rmtree(args.out, ignore_errors=True)
    os.makedirs(args.out)
    env = dict(os.environ)
    for name in ("WAYLAND_DISPLAY", "DISPLAY", "DBUS_SESSION_BUS_ADDRESS"):
        env.pop(name, None)
    cmd = ["dbus-run-session", "--", shutil.which("xvfb-run"), "-a", "-s", "-screen 0 1600x1000x24",
           sys.executable, os.path.abspath(__file__), args.tinta, args.doc, "--out", args.out, "--inner"]
    return subprocess.run(cmd, env=env).returncode


if __name__ == "__main__":
    sys.exit(main())
