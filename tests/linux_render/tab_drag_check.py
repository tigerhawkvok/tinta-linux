#!/usr/bin/env python3
"""Tab drags between Tinta windows, through the desktop's drag and drop, on
a private D-Bus session and Xvfb (XDND here; Wayland carries the same GDK
drag). Window A opens alpha and beta (beta joins it as a tab); window B
opens gamma with --cascade, a process of its own.

  moved       beta, pulled clear of A's strip and dropped on B's, opens in
              B and leaves A
  torn-off    beta, released over the empty screen, tears off into a third
              window
  own-window  delta (joined to A), released over A's own document, tears off
  returned    epsilon, pulled clear and dropped back on A's strip, stays
  last-tab    a window's only tab dropped on another window: that window
              takes it and the empty one closes

Positions assume Tinta's tab metrics at scale 1 in 640-wide windows: the
second tab spans about x 230-420 at y 20.

Usage: tab_drag_check.py TINTA [--out DIR]
"""
import argparse
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
TAB_X, TAB_Y = 300, 20  # A's second tab, window-relative


def inner(args):
    from gui_check import X
    from render_fixtures import isolated_env

    out = args.out
    work = os.path.join(out, "work")
    os.makedirs(work, exist_ok=True)
    for name in ("alpha", "beta", "gamma", "delta", "epsilon"):
        with open(os.path.join(work, name + ".md"), "w") as f:
            f.write(f"# {name.capitalize()}\n\nThe {name} document.\n")
    env = isolated_env(os.path.join(out, ".home"), None, "")
    with open(os.path.join(env["XDG_CONFIG_HOME"], "Tinta", "settings.ini"), "a") as f:
        f.write("windowWidth=640\nwindowHeight=420\nwindowMaximized=0\ncheckUpdates=0\nrestoreSession=0\n")
    env.pop("WAYLAND_DISPLAY", None)
    env["GDK_BACKEND"] = "x11"
    env.setdefault("GSK_RENDERER", "cairo")
    x = X()
    results, procs = [], []

    def report(name, ok, detail):
        results.append(ok)
        print(f"{'PASS' if ok else 'FAIL'} {name}: {detail}", flush=True)

    def windows():
        return sorted(x.toplevels(), key=lambda t: t[0])

    def titles():
        return [x.title(w[0]) for w in windows()]

    def by_title(word):
        return next((w for w in windows() if word in x.title(w[0])), None)

    def wait_count(n, timeout=15):
        start = time.monotonic()
        while time.monotonic() - start < timeout:
            if len(windows()) == n:
                time.sleep(1.2)
                return True
            time.sleep(0.1)
        return False

    def launch(*argv):
        p = subprocess.Popen([args.tinta, *argv], env=env, cwd=work, stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL)
        procs.append(p)
        return p

    def shot(name):
        x.grab(0, 0, 1600, 1000).save(os.path.join(out, name + ".png"))

    def place(window, left, top):
        x.lib.XMoveWindow(x.d, window, left, top)
        x.sync()

    def layout(spots):
        for word, (left, top) in spots:
            w = by_title(word)
            if w is not None:
                place(w[0], left, top)
        time.sleep(0.8)

    def drag(path, step=12):
        x.move(*path[0])
        time.sleep(0.15)
        x.tst.XTestFakeButtonEvent(x.d, 1, 1, 0)
        x.sync()
        time.sleep(0.1)
        for (ax, ay), (bx, by) in zip(path, path[1:]):
            n = max(1, int(max(abs(bx - ax), abs(by - ay)) / step))
            for i in range(1, n + 1):
                x.move(int(ax + (bx - ax) * i / n), int(ay + (by - ay) * i / n))
                time.sleep(0.04)
        time.sleep(0.4)
        x.tst.XTestFakeButtonEvent(x.d, 1, 0, 0)
        x.sync()
        time.sleep(1.5)

    empty = (1500, 960)  # the bottom-right corner stays clear of windows
    try:
        launch(os.path.join(work, "alpha.md"))
        wait_count(1)
        launch(os.path.join(work, "beta.md"))  # joins A as a tab
        time.sleep(2.0)
        launch("--cascade", os.path.join(work, "gamma.md"))
        wait_count(2)
        layout((("beta", (0, 0)), ("gamma", (760, 0))))
        a, b = by_title("beta"), by_title("gamma")
        shot("0-start")

        drag([(a[1] + TAB_X, a[2] + TAB_Y), (a[1] + TAB_X, a[2] + 160), (b[1] + 200, b[2] + 160),
              (b[1] + 300, b[2] + TAB_Y)])
        shot("1-moved")
        t = titles()
        report("moved", len(t) == 2 and any("alpha" in s for s in t) and any("beta" in s for s in t), str(t))

        b = by_title("beta")
        drag([(b[1] + TAB_X, b[2] + TAB_Y), (b[1] + TAB_X, b[2] + 200), empty])
        torn = wait_count(3)
        shot("2-torn-off")
        t = titles()
        report("torn-off", torn and sum("beta" in s for s in t) == 1 and any("gamma" in s for s in t), str(t))

        launch(os.path.join(work, "delta.md"))  # joins A
        time.sleep(2.5)
        layout((("delta", (0, 0)), ("gamma", (760, 0)), ("beta", (0, 480))))
        a = by_title("delta")
        if a is not None:
            drag([(a[1] + TAB_X, a[2] + TAB_Y), (a[1] + TAB_X, a[2] + 300), (a[1] + 200, a[2] + 320)])
        torn = wait_count(4)
        shot("3-own-window")
        t = titles()
        report("own-window", torn and any("delta" in s for s in t) and any("alpha" in s for s in t), str(t))

        launch(os.path.join(work, "epsilon.md"))  # joins A
        time.sleep(2.5)
        layout((("epsilon", (0, 0)), ("gamma", (760, 0)), ("beta", (0, 480)), ("delta", (700, 480))))
        a = by_title("epsilon")
        before = sorted(titles())
        if a is not None:
            drag([(a[1] + TAB_X, a[2] + TAB_Y), (a[1] + TAB_X, a[2] + 250), (a[1] + 120, a[2] + 250),
                  (a[1] + 120, a[2] + TAB_Y)])
            time.sleep(1.0)
        shot("4-returned")
        after = sorted(titles())
        report("returned", len(after) == 4 and after == before, f"before {before}, after {after}")

        # beta's window has one tab, at the strip's start
        layout((("gamma", (760, 0)), ("beta", (760, 480)), ("epsilon", (0, 0)), ("delta", (0, 480))))
        b, c = by_title("gamma"), by_title("beta")
        if b is not None and c is not None:
            drag([(c[1] + 70, c[2] + TAB_Y), (c[1] + 70, c[2] - 60), (b[1] + 300, b[2] + 160),
                  (b[1] + 300, b[2] + TAB_Y)])
            time.sleep(1.5)
        shot("5-last-tab")
        t = titles()
        # gamma's window now shows beta; beta's own window is gone
        report("last-tab", len(t) == 3 and all(any(w in s for s in t) for w in ("beta", "delta", "epsilon")),
               str(t))
    finally:
        for p in procs:
            if p.poll() is None:
                p.terminate()
                try:
                    p.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    p.kill()
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("tinta")
    parser.add_argument("--out", default="build/tab-drag-check")
    parser.add_argument("--inner", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    args.tinta = os.path.abspath(args.tinta)
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
           sys.executable, os.path.abspath(__file__), args.tinta, "--out", args.out, "--inner"]
    return subprocess.run(cmd, env=env).returncode


if __name__ == "__main__":
    sys.exit(main())
