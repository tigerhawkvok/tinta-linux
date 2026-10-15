#!/usr/bin/env python3
"""Single instance across processes, on a private D-Bus session and Xvfb.

A plain `tinta file.md` joins the running window as a new tab, as on
Windows: the first interactive Tinta owns org.tinta.Instance on the session
bus, and FindWindow in a later one returns a stand-in whose WM_COPYDATA
travels there. With one isolated configuration shared by every launch:

  joins       B (another file) exits 0 without a window; A's title names B's file
  headless    a --render-png run while A is up renders its own PNG, not routed
  cascade     --cascade still opens a window of its own
  takeover    after A exits, the next launch owns the name and opens its window

Usage: instance_check.py TINTA [--out DIR]
"""
import argparse
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
FIXTURES = os.path.join(HERE, "..", "fixtures")
DOC_A = os.path.abspath(os.path.join(FIXTURES, "gtk-window.md"))
DOC_B = os.path.abspath(os.path.join(FIXTURES, "markdown-regression-control.md"))
DOC_C = os.path.abspath(os.path.join(FIXTURES, "wombat-theme.md"))


def inner(args):
    from gui_check import X
    from render_fixtures import isolated_env

    out = args.out
    results = []

    def report(name, ok, detail):
        results.append(ok)
        print(f"{'PASS' if ok else 'FAIL'} {name}: {detail}", flush=True)

    env = isolated_env(os.path.join(out, ".home", "shared"), None, "")
    with open(os.path.join(env["XDG_CONFIG_HOME"], "Tinta", "settings.ini"), "a") as f:
        f.write("windowWidth=900\nwindowHeight=600\nwindowMaximized=0\nopenInTabs=1\n")
    env.pop("WAYLAND_DISPLAY", None)
    env["GDK_BACKEND"] = "x11"
    env.setdefault("GSK_RENDERER", "cairo")
    x = X()
    procs = []

    def launch(*argv, name):
        log = open(os.path.join(out, f"{name}.log"), "w")
        p = subprocess.Popen([args.tinta, *argv], env=env, cwd=out, stdout=log, stderr=log)
        procs.append(p)
        return p

    def windows():
        return {g[0]: x.title(g[0]) for g in x.toplevels()}

    def wait_for(predicate, timeout=15):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            value = predicate()
            if value:
                return value
            time.sleep(0.05)
        return None

    try:
        a = launch(DOC_A, name="a")
        first = wait_for(lambda: windows() or None)
        if not first:
            report("joins", False, "the first Tinta never showed a window")
            return results
        win_a = next(iter(first))
        time.sleep(1.0)  # its name request completes

        start = time.monotonic()
        b = launch(DOC_B, name="b")
        try:
            code = b.wait(timeout=15)
        except subprocess.TimeoutExpired:
            code = None
        took = (time.monotonic() - start) * 1000
        title = wait_for(lambda: "markdown-regression-control" in x.title(win_a) and x.title(win_a), 5) or x.title(win_a)
        count = len(windows())
        report("joins", code == 0 and count == 1 and "markdown-regression-control" in title,
               f"second launch exited {code} after {took:.0f} ms; {count} window(s); first window now '{title}'")

        png = os.path.join(out, "headless.png")
        r = subprocess.run([args.tinta, DOC_C, f"--render-png={png}", "--size=600x400"], env=env, cwd=out,
                           capture_output=True, text=True, timeout=60)
        report("headless", r.returncode == 0 and os.path.exists(png) and "wombat-theme.md" in r.stderr,
               f"exit {r.returncode}, PNG {'written' if os.path.exists(png) else 'missing'}; "
               f"{next((l for l in r.stderr.splitlines() if 'window' in l), 'no window line')}")

        c = launch("--cascade", DOC_C, name="c")
        two = wait_for(lambda: len(windows()) == 2 and windows())
        report("cascade", bool(two) and any("wombat-theme" in t for t in (two or {}).values()),
               f"windows: {list((two or windows()).values())}")
        c.terminate()
        c.wait(timeout=10)
        wait_for(lambda: len(windows()) == 1, 5)

        a.terminate()
        a.wait(timeout=10)
        wait_for(lambda: len(windows()) == 0, 5)
        time.sleep(0.5)
        d = launch(DOC_B, name="d")
        own = wait_for(lambda: windows() or None)
        alive = d.poll() is None
        report("takeover", bool(own) and alive,
               f"after the first instance exited: {list((own or {}).values())}, process "
               f"{'running' if alive else f'exited {d.returncode}'}")
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
    parser.add_argument("--out", default="build/instance-check")
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
    cmd = ["dbus-run-session", "--", shutil.which("xvfb-run"), "-a", "-s", "-screen 0 1600x1200x24",
           sys.executable, os.path.abspath(__file__), args.tinta, "--out", args.out, "--inner"]
    return subprocess.run(cmd, env=env).returncode


if __name__ == "__main__":
    sys.exit(main())
