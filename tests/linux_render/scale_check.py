#!/usr/bin/env python3
"""Fractional-scale check of the GTK window in a nested KWin.

Starts a private KWin (virtual output at --scale, its own Wayland socket and
D-Bus session; the user's display settings are never touched), runs Tinta
in it natively on Wayland, captures the output with spectacle and finds the
window. It passes when:
  backend  Tinta is on GdkWaylandDisplay
  scale    GTK reports the output's scale (fractional-scale-v1)
  frame    the window equals a headless --render-png at that scale, pixel
           for pixel: Tinta draws at the true scale and nothing blurs it

Usage: scale_check.py TINTA DOC [--scale 1.5] [--size WxH] [--out DIR]
Needs kwin_wayland (Plasma 6), dbus-run-session, spectacle and Pillow.
"""
import argparse
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from render_fixtures import isolated_env  # noqa: E402

from PIL import Image, ImageChops  # noqa: E402


def find_frame(screen, ref):
    """Top-left of `ref` inside `screen` (exact match), or None."""
    sw, sh = screen.size
    rw, rh = ref.size
    rows = [r for r in range(rh // 4, rh, max(1, rh // 16))]
    sbytes = screen.tobytes()
    rbytes = ref.tobytes()
    for r in rows:  # a row with detail; the caption and margins are flat
        row = rbytes[r * rw * 3:(r + 1) * rw * 3]
        if len(set(row)) < 8:
            continue
        for y in range(r, sh - (rh - r) + 1):
            line = sbytes[y * sw * 3:(y + 1) * sw * 3]
            x = line.find(row)
            while x != -1:
                if x % 3 == 0:
                    pos = (x // 3, y - r)
                    if ImageChops.difference(screen.crop(pos + (pos[0] + rw, pos[1] + rh)), ref).getbbox() is None:
                        return pos
                x = line.find(row, x + 1)
        return None
    return None


def inner(args):
    """Runs inside the nested session."""
    out = args.out
    width, height = (int(v) for v in args.size.split("x"))
    env = isolated_env(os.path.join(out, ".home", "nested"), None, "")
    with open(os.path.join(env["XDG_CONFIG_HOME"], "Tinta", "settings.ini"), "a") as f:
        f.write(f"windowWidth={width}\nwindowHeight={height}\nwindowMaximized=0\n")
    env["TINTA_TRACE_GTK"] = "1"
    env.pop("DISPLAY", None)
    env["GDK_BACKEND"] = "wayland"
    env.setdefault("GSK_RENDERER", "cairo")  # the virtual output has no GPU
    # The virtual backend ignores --scale: set it the way System Settings
    # would (this KWin's config is private, see outer())
    with open(os.path.join(out, "kscreen.log"), "w") as klog:
        subprocess.run(["kscreen-doctor", "-o"], stdout=klog, stderr=klog, timeout=30)
        subprocess.run(["kscreen-doctor", f"output.Virtual-0.scale.{args.scale:g}"], stdout=klog, stderr=klog,
                       timeout=30)
        time.sleep(0.5)
        subprocess.run(["kscreen-doctor", "-o"], stdout=klog, stderr=klog, timeout=30)
    log = open(os.path.join(out, "tinta.log"), "w")
    proc = subprocess.Popen([args.tinta, args.doc], env=env, cwd=out, stdout=log, stderr=log)
    time.sleep(args.wait)
    shot = os.path.join(out, "screen.png")
    with open(os.path.join(out, "spectacle.log"), "w") as slog:
        slog.write(f"WAYLAND_DISPLAY={os.environ.get('WAYLAND_DISPLAY')} "
                   f"DBUS_SESSION_BUS_ADDRESS={os.environ.get('DBUS_SESSION_BUS_ADDRESS')}\n")
        slog.flush()
        subprocess.run(["spectacle", "-b", "-n", "-f", "-o", shot], stdout=slog, stderr=slog, timeout=60)
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
    log.close()


def outer(args):
    out = os.path.abspath(args.out)
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    width, height = (int(v) for v in args.size.split("x"))
    results = []

    def report(name, ok, detail):
        results.append(ok)
        print(f"{'PASS' if ok else 'FAIL'} {name}: {detail}", flush=True)

    # The output in physical pixels; KWin's logical size is this / scale
    phys_w, phys_h = int(width * 1.6), int(height * 1.6)
    env = dict(os.environ)
    for name in ("WAYLAND_DISPLAY", "DISPLAY", "DBUS_SESSION_BUS_ADDRESS"):
        env.pop(name, None)
    env.setdefault("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}")
    # KWin saves output and tiling state in its config: a nested KWin
    # sharing the user's would write into their real kwinoutputconfig.json
    # and kwinrc (and apply their stored scale over --scale)
    for name in ("XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_STATE_HOME", "XDG_CACHE_HOME"):
        env[name] = os.path.join(out, ".kwin-home", name.lower())
        os.makedirs(env[name], exist_ok=True)
    # A private compositor: spectacle may capture it without a desktop-file grant
    env["KWIN_SCREENSHOT_NO_PERMISSION_CHECKS"] = "1"
    cmd = ["dbus-run-session", "--", "kwin_wayland", "--virtual", "--no-lockscreen",
           "--width", str(phys_w), "--height", str(phys_h), "--scale", str(args.scale),
           "--socket", f"tinta-scale-{os.getpid()}",
           "--exit-with-session", " ".join([sys.executable, os.path.abspath(__file__), args.tinta, args.doc,
                                            "--out", out, "--size", args.size, "--scale", str(args.scale),
                                            "--wait", str(args.wait), "--inner"])]
    with open(os.path.join(out, "kwin.log"), "w") as log:
        subprocess.run(cmd, env=env, stdout=log, stderr=log, timeout=180)

    trace = open(os.path.join(out, "tinta.log")).read().splitlines() if os.path.exists(
        os.path.join(out, "tinta.log")) else []
    display = next((l for l in trace if l.startswith("tinta-gtk: display")), "no trace")
    report("backend", "GdkWaylandDisplay" in display, display.replace("tinta-gtk: ", ""))
    sizes = [l for l in trace if l.startswith("tinta-gtk: resize")]
    last = sizes[-1].replace("tinta-gtk: ", "") if sizes else "no resize traced"
    report("scale", f"scale {args.scale:.3f}" in last, last)

    shot = os.path.join(out, "screen.png")
    if not os.path.exists(shot):
        report("frame", False, "no capture (see kwin.log)")
        return results
    # The reference at the window's own pixel size: a whole number of logical
    # pixels times a fractional scale need not give the size asked for
    px = last.split(", ")[-1].split()[0] if sizes else args.size
    reference = os.path.join(out, "headless.png")
    env = isolated_env(os.path.join(out, ".home", "headless"), None, "")
    subprocess.run([args.tinta, args.doc, f"--render-png={reference}", f"--size={px}", f"--scale={args.scale}"],
                   env=env, cwd=out, capture_output=True, timeout=120)
    ref = Image.open(reference).convert("RGB")
    screen = Image.open(shot).convert("RGB")
    pos = find_frame(screen, ref)
    if pos:
        report("frame", True, f"found pixel-identical to the headless {args.scale}x render at +{pos[0]}+{pos[1]} "
                              f"in a {screen.size[0]}x{screen.size[1]} capture")
    else:
        report("frame", False, f"no exact {ref.size[0]}x{ref.size[1]} match in the "
                               f"{screen.size[0]}x{screen.size[1]} capture")
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("tinta")
    parser.add_argument("doc")
    parser.add_argument("--scale", type=float, default=1.5)
    parser.add_argument("--size", default="1000x700", help="window size in device pixels")
    parser.add_argument("--out", default="build/scale-check")
    parser.add_argument("--wait", type=float, default=4.0, help="seconds before the capture")
    parser.add_argument("--inner", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    args.tinta = os.path.abspath(args.tinta)
    args.doc = os.path.abspath(args.doc)
    if args.inner:
        inner(args)
        return 0
    results = outer(args)
    print(f"{sum(results)}/{len(results)} checks passed")
    return 0 if results and all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
