#!/usr/bin/env python3
"""Linked scrolling in the split editor, on a private D-Bus session and Xvfb:
the wheel over each pane, wide and light, narrow and dark, wrapped and not.

  preview      the wheel over the preview scrolls the preview
  follows      the source follows it
  stays        nothing pulls the preview back a second later
  source       the wheel over the source scrolls both panes on
  top          the wheel back up over the preview returns both to the top

Each run has its own XDG_CONFIG_HOME under the output directory.

Usage: linked_scroll_check.py TINTA DOC [--out DIR]
"""
import argparse
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

RUNS = (("wide-light-wrap", 1100, 0, 1), ("narrow-dark-wrap", 700, 5, 1),
        ("wide-light-nowrap", 1100, 0, 0))


def inner(args):
    from gui_check import X, changed_fraction, isolated_env, settle

    x = X()
    results = []

    def report(name, ok, detail=""):
        results.append(ok)
        print(("PASS " if ok else "FAIL ") + name + ("  " + detail if detail else ""), flush=True)

    def wheel(button, px, py, notches):
        x.move(px, py)
        time.sleep(0.05)
        for _ in range(notches):
            x.click(button)
            time.sleep(0.04)

    for tag, width, theme, wrap in RUNS:
        env = isolated_env(os.path.join(args.out, ".home-" + tag), None, "")
        with open(os.path.join(env["XDG_CONFIG_HOME"], "Tinta", "settings.ini"), "a") as f:
            f.write("windowWidth=%d\nwindowHeight=800\nwindowMaximized=0\ncheckUpdates=0\n"
                    "hasAskedFileAssociation=1\nthemeIndex=%d\neditorWordWrap=%d\n" % (width, theme, wrap))
        env.pop("WAYLAND_DISPLAY", None)
        env["GDK_BACKEND"] = "x11"
        env.setdefault("GSK_RENDERER", "cairo")
        proc = subprocess.Popen([args.tinta, "--edit", args.doc], env=env,
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            start = time.monotonic()
            while time.monotonic() - start < 20 and not x.toplevels():
                time.sleep(0.05)
            time.sleep(2.0)
            if not x.toplevels():
                report(tag + ": the window opens", False)
                continue
            geo = max(x.toplevels(), key=lambda t: t[3] * t[4])
            _, gx, gy, gw, gh = geo

            def shot(name):
                return settle(x, geo, os.path.join(args.out, "%s-%s.png" % (tag, name)))

            # Inside each pane, clear of the tab strip and the bottom chips
            src = (int(gw * 0.05), 120, int(gw * 0.30), gh - 60)
            pre = (int(gw * 0.70), 120, int(gw * 0.95), gh - 60)
            over_preview = (gx + int(gw * 0.8), gy + gh // 2)
            over_source = (gx + int(gw * 0.25), gy + gh // 2)
            top = shot("0-top")
            wheel(5, *over_preview, 12)
            led = shot("1-preview-led")
            report(tag + ": the wheel over the preview scrolls the preview",
                   changed_fraction(top.crop(pre), led.crop(pre)) > 0.02)
            report(tag + ": the source follows the preview",
                   changed_fraction(top.crop(src), led.crop(src)) > 0.02)
            time.sleep(1.0)
            later = shot("2-preview-led-later")
            report(tag + ": nothing pulls the preview back afterwards",
                   changed_fraction(led.crop(pre), later.crop(pre)) == 0.0)
            wheel(5, *over_source, 6)
            source_led = shot("3-source-led")
            report(tag + ": the wheel over the source scrolls both panes on",
                   changed_fraction(later.crop(pre), source_led.crop(pre)) > 0.02 and
                   changed_fraction(later.crop(src), source_led.crop(src)) > 0.02)
            wheel(4, *over_preview, 40)
            back = shot("4-back-to-top")
            moved = changed_fraction(top.crop(pre), back.crop(pre))
            # The source's caret may blink between the two shots
            report(tag + ": the wheel back over the preview returns both panes to the top",
                   moved < 0.002 and changed_fraction(top.crop(src), back.crop(src)) < 0.01,
                   "preview changed %.4f" % moved)
        finally:
            proc.terminate()
            proc.wait(timeout=10)
            time.sleep(0.5)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("tinta")
    parser.add_argument("doc")
    parser.add_argument("--out", default="build/linked-scroll-check")
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
    cmd = ["dbus-run-session", "--", shutil.which("xvfb-run"), "-a", "-s", "-screen 0 1400x1000x24",
           sys.executable, os.path.abspath(__file__), args.tinta, args.doc, "--out", args.out, "--inner"]
    return subprocess.run(cmd, env=env).returncode


if __name__ == "__main__":
    sys.exit(main())
