#!/usr/bin/env python3
"""Tab drags, and the print dialog through the portal, on a live KDE
Plasma (Wayland) session: the same scenarios as tab_drag_check.py, carried
by KWin's drag and drop instead of XDND. Input comes from uinput,
window state from KWin scripting, captures from spectacle (desktop_check.py's
helpers).

Only run it with the user's go-ahead while they are away: the pointer
moves. It refuses a locked session or an open Tinta window. Every Tinta
uses its own configuration, and every Tinta it started, torn-off windows
included, is killed at the end.

Usage: desktop_tab_drag_check.py TINTA [--out DIR]
"""
import argparse
import os
import signal
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import desktop_check as dc  # noqa: E402
from render_fixtures import isolated_env  # noqa: E402

ALL = 'const t = workspace.windowList().filter(w => w.resourceClass == "tinta" && w.normalWindow);'
W, H = 640, 420
TAB2_X, TAB_Y = 300, 20  # the second tab, window-relative (as in tab_drag_check.py)


def windows():
    """Tinta windows: caption -> (x, y, w, h)."""
    lines = dc.kwin(ALL + """
for (const w of t) { const g = w.frameGeometry;
  out([Math.round(g.x), Math.round(g.y), Math.round(g.width), Math.round(g.height), w.caption].join("|")); }
out("END");""")
    found = {}
    for line in lines:
        f = line.split("|")
        found[f[4]] = tuple(int(v) for v in f[:4])
    return found


def place(word, x, y):
    dc.kwin(ALL + f"""
for (const w of t) if (w.caption.indexOf("{word}") >= 0) {{
  w.frameGeometry = {{x: {x}, y: {y}, width: {W}, height: {H}}}; workspace.activeWindow = w; }}
out("END");""")
    time.sleep(0.4)


def active_tinta():
    """The active window's frame, if it is Tinta's: (x, y, w, h)."""
    lines = dc.kwin("""const a = workspace.activeWindow;
if (a && a.resourceClass == "tinta") { const g = a.frameGeometry;
  out([Math.round(g.x), Math.round(g.y), Math.round(g.width), Math.round(g.height)].join("|")); }
out("END");""")
    return tuple(int(v) for v in lines[0].split("|")) if lines else None


def find(word):
    return next((g for c, g in windows().items() if word in c), None)


def run(args):
    out = os.path.abspath(args.out)
    work = os.path.join(out, "work")
    os.makedirs(work, exist_ok=True)
    dc.KWIN_DIR = out
    if dc.qdbus("org.freedesktop.ScreenSaver", "/ScreenSaver", "org.freedesktop.ScreenSaver.GetActive") != "false":
        raise SystemExit("the session is locked: not touching it")
    if windows():
        raise SystemExit("a Tinta window is already open: close it first")
    for name in ("alpha", "beta", "gamma", "delta", "epsilon"):
        with open(os.path.join(work, name + ".md"), "w") as f:
            f.write(f"# {name.capitalize()}\n\nThe {name} document.\n")
    env = isolated_env(os.path.join(out, ".home"), None, "")
    with open(os.path.join(env["XDG_CONFIG_HOME"], "Tinta", "settings.ini"), "a") as f:
        f.write(f"windowWidth={W}\nwindowHeight={H}\nwindowMaximized=0\ncheckUpdates=0\nrestoreSession=0\n")
    env = dc.session_env(env)
    env["TINTA_TRACE_GTK"] = "1"
    results = []

    def report(name, ok, detail):
        results.append(ok)
        print(f"{'PASS' if ok else 'FAIL'} {name}: {detail}", flush=True)

    def launch(*argv):
        log = open(os.path.join(out, f"tinta-{len(os.listdir(out))}.log"), "w")
        return subprocess.Popen([args.tinta, *argv], env=env, cwd=work, stdout=log, stderr=log)

    def wait(check, timeout=15):
        start = time.monotonic()
        while time.monotonic() - start < timeout:
            value = check()
            if value:
                return value
            time.sleep(0.3)
        return check()

    screen_w, screen_h = dc.screen_size()
    dc.KEYS.setdefault("p", 25)  # KEY_P, for Ctrl+P (before the keyboard device declares its keys)
    inp = dc.Input(screen_w, screen_h)
    empty = (screen_w - 120, screen_h - 120)  # no Tinta window there

    def drag(points, escape=False):
        """Press on the first point (which must be in the active Tinta window),
        move through the rest, then release."""
        x0, y0 = points[0]
        active = active_tinta()
        if not active or not (active[0] <= x0 < active[0] + active[2] and active[1] <= y0 < active[1] + active[3]):
            raise dc.Abort(f"press point {points[0]} is not in the active Tinta window ({active})")
        inp.move(x0, y0)
        time.sleep(0.15)
        inp.button(True)
        time.sleep(0.1)
        for (ax, ay), (bx, by) in zip(points, points[1:]):
            n = max(1, int(max(abs(bx - ax), abs(by - ay)) / 12))
            for i in range(1, n + 1):
                inp.move(ax + (bx - ax) * i / n, ay + (by - ay) * i / n)
                time.sleep(0.02)
        time.sleep(0.4)
        if escape:
            inp.key("esc")
            time.sleep(0.4)
        inp.button(False)
        time.sleep(2.0)

    def captions():
        return sorted(windows())

    try:
        launch(os.path.join(work, "alpha.md"))
        wait(lambda: find("alpha"))
        launch(os.path.join(work, "beta.md"))  # joins as a tab
        wait(lambda: find("beta"))
        launch("--cascade", os.path.join(work, "gamma.md"))
        wait(lambda: find("gamma"))
        report("backend", any("GdkWaylandDisplay" in open(os.path.join(out, f)).read()
                              for f in os.listdir(out) if f.endswith(".log")), "Wayland")
        place("gamma", 760, 120)
        place("beta", 60, 120)
        a, b = find("beta"), find("gamma")
        dc.screenshot(os.path.join(out, "0-start.png"))

        drag([(a[0] + TAB2_X, a[1] + TAB_Y), (a[0] + TAB2_X, a[1] + 180), (b[0] + 200, b[1] + 180),
              (b[0] + 300, b[1] + TAB_Y)])
        dc.screenshot(os.path.join(out, "1-moved.png"))
        c = captions()
        report("moved", len(c) == 2 and any("alpha" in s for s in c) and any("beta" in s for s in c), str(c))

        place("beta", 760, 120)
        b = find("beta")
        drag([(b[0] + TAB2_X, b[1] + TAB_Y), (b[0] + TAB2_X, b[1] + 220), empty])
        torn = wait(lambda: len(windows()) == 3)
        dc.screenshot(os.path.join(out, "2-torn-off.png"))
        c = captions()
        report("torn-off", bool(torn) and sum("beta" in s for s in c) == 1 and any("gamma" in s for s in c), str(c))

        launch(os.path.join(work, "delta.md"))  # joins the first window
        wait(lambda: find("delta"))
        place("delta", 60, 120)
        a = find("delta")
        drag([(a[0] + TAB2_X, a[1] + TAB_Y), (a[0] + TAB2_X, a[1] + 300), (a[0] + 200, a[1] + 320)])
        torn = wait(lambda: len(windows()) == 4)
        c = captions()
        report("own-window", bool(torn) and any("delta" in s for s in c) and any("alpha" in s for s in c), str(c))

        launch(os.path.join(work, "epsilon.md"))  # joins the first window
        wait(lambda: find("epsilon"))
        place("epsilon", 60, 120)
        a = find("epsilon")
        before = captions()
        drag([(a[0] + TAB2_X, a[1] + TAB_Y), (a[0] + TAB2_X, a[1] + 260), (a[0] + 120, a[1] + 260),
              (a[0] + 120, a[1] + TAB_Y)])
        after = captions()
        report("returned", after == before, f"before {before}, after {after}")

        # (Esc mid-drag is not checked: on KDE Wayland the source gets no keys
        # during a drag, and Esc and a drop on nothing arrive alike, so the
        # tab tears off; see tests/linux-tab-drag-validation.md)

        # Printing: GTK's own dialog (CUPS) opens with the exported PDF ready.
        # A real printer may be configured, so the dialog is only ever
        # cancelled: Esc goes to it once KWin says it is the active window.
        place("epsilon", 60, 120)
        if not active_tinta():
            raise dc.Abort("no Tinta window is active for Ctrl+P")
        inp.key("ctrl", "p")
        time.sleep(1.5)
        if not active_tinta():
            raise dc.Abort("the preview's window is not active for Enter")
        inp.key("enter")
        time.sleep(3.0)
        dc.screenshot(os.path.join(out, "5-print-dialog.png"))
        tmp = os.environ.get("TMPDIR", "/tmp")
        pending = [f for f in os.listdir(tmp) if f.startswith("tinta-print-")]
        caption = dc.kwin("const a = workspace.activeWindow; out(a ? a.resourceClass + \"|\" + a.caption : \"\"); out(\"END\");")
        dialog_up = bool(caption) and caption[0] == "tinta|Print"
        if dialog_up:
            inp.key("esc")
            time.sleep(2.0)
        left = [f for f in os.listdir(tmp) if f.startswith("tinta-print-")]
        report("print-dialog", dialog_up and bool(pending) and not left,
               f"active window {caption}; temporary PDF while up: {pending}; after Esc: {left}")
    except dc.Abort as e:
        report("guard", False, str(e))
    finally:
        inp.release_all()
        inp.close()
        # Every Tinta started here, torn-off windows included (they run our work files)
        for line in subprocess.run(["pgrep", "-af", "tinta"], capture_output=True, text=True).stdout.splitlines():
            pid, _, cmd = line.partition(" ")
            if work in cmd and args.tinta in cmd:
                try:
                    os.kill(int(pid), signal.SIGTERM)
                except ProcessLookupError:
                    pass
    print(f"{sum(results)}/{len(results)} checks passed")
    return 0 if results and all(results) else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("tinta")
    parser.add_argument("--out", default="build/desktop-tab-drag")
    args = parser.parse_args()
    args.tinta = os.path.abspath(args.tinta)
    args.out = os.path.abspath(args.out)
    return run(args)


if __name__ == "__main__":
    sys.exit(main())
