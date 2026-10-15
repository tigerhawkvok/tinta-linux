#!/usr/bin/env python3
"""Headed check of the GTK window on a live KDE Plasma (Wayland) session.

Where gui_check.py runs on a private Xvfb server with no window manager,
this runs Tinta natively on KWin. It covers what only a real compositor does:
caption moves, edge resizes, maximize and minimize. Input comes from
virtual uinput devices (an absolute pointer and a keyboard, which KWin
treats as hardware), window state from KWin scripting, and captures from
spectacle.

It runs on a user's own session, so it guards that session: every key and
every click first checks that KWin's active window is Tinta and that the
point lies inside Tinta's frame. Otherwise it releases everything and
stops. Only run it with the user's go-ahead, while they are away from the
desktop: the session must be unlocked, and the pointer moves.

Usage: desktop_check.py TINTA DOC [--out DIR] [--size WxH]
Needs: Plasma 6 Wayland, write access to /dev/uinput (the input group),
qdbus6, spectacle, journalctl, and Pillow.
"""
import argparse
import ctypes
import fcntl
import os
import re
import struct
import subprocess
import sys
import time
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from render_fixtures import isolated_env  # noqa: E402

from PIL import Image, ImageChops  # noqa: E402

SESSION = {
    "XDG_RUNTIME_DIR": f"/run/user/{os.getuid()}",
    "WAYLAND_DISPLAY": "wayland-0",
    "DBUS_SESSION_BUS_ADDRESS": f"unix:path=/run/user/{os.getuid()}/bus",
    "XDG_SESSION_TYPE": "wayland",
    "XDG_CURRENT_DESKTOP": "KDE",
    "QT_QPA_PLATFORM": "wayland",
}


def session_env(base=None):
    env = dict(base if base is not None else os.environ)
    env.update(SESSION)
    env.pop("DISPLAY", None)
    return env


class Abort(Exception):
    pass


# ---- KWin ------------------------------------------------------------------------------------
def qdbus(*args):
    r = subprocess.run(["qdbus6", *args], env=session_env(), capture_output=True, text=True, timeout=20)
    return r.stdout.strip()


def kwin(script_body):
    """Runs a KWin script; returns the lines it logs with out(...)."""
    token = "TINTA" + uuid.uuid4().hex[:10]
    path = os.path.join(KWIN_DIR, token + ".js")  # KWin reads it: not a private /tmp
    with open(path, "w") as f:
        f.write(f'function out(s) {{ console.info("{token} " + s); }}\n{script_body}\n')
    since = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(time.time() - 1))
    try:
        sid = qdbus("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting.loadScript", path, token)
        qdbus("org.kde.KWin", f"/Scripting/Script{sid}", "org.kde.kwin.Script.run")
        lines = []
        for _ in range(20):
            time.sleep(0.1)
            log = subprocess.run(["journalctl", "--user", "--since", since, "--no-pager", "-o", "cat"],
                                 capture_output=True, text=True).stdout
            lines = [line.split(token + " ", 1)[1] for line in log.splitlines() if token + " " in line]
            if lines and lines[-1] == "END":
                return lines[:-1]
        return [line for line in lines if line != "END"]
    finally:
        qdbus("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting.unloadScript", token)
        os.unlink(path)


KWIN_DIR = os.getcwd()

FIND = 'const t = workspace.windowList().filter(w => w.resourceClass == "tinta" && w.normalWindow);'


def tinta_state():
    """Tinta's window: geometry, flags and whether it is the active window."""
    lines = kwin(FIND + """
const a = workspace.activeWindow;
for (const w of t) {
    const g = w.frameGeometry, m = workspace.clientArea(KWin.MaximizeArea, w);
    out([Math.round(g.x), Math.round(g.y), Math.round(g.width), Math.round(g.height), w.minimized ? 1 : 0,
         w === a ? 1 : 0, Math.round(m.x), Math.round(m.y), Math.round(m.width), Math.round(m.height),
         w.caption].join("|"));
}
out("END");""")
    if not lines:
        return None
    f = lines[0].split("|")
    x, y, w, h, minimized, active, mx, my, mw, mh = (int(v) for v in f[:10])
    return {"x": x, "y": y, "w": w, "h": h, "minimized": bool(minimized), "active": bool(active),
            "area": (mx, my, mw, mh), "caption": f[10]}


def activate_tinta():
    kwin(FIND + "if (t.length) { t[0].minimized = false; workspace.activeWindow = t[0]; } out('END');")
    time.sleep(0.3)


# ---- uinput ------------------------------------------------------------------------------------
UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_RELBIT, UI_SET_ABSBIT = 0x40045564, 0x40045565, 0x40045566, 0x40045567
UI_DEV_SETUP, UI_ABS_SETUP, UI_DEV_CREATE, UI_DEV_DESTROY = 0x405C5503, 0x401C5504, 0x5501, 0x5502
EV_SYN, EV_KEY, EV_REL, EV_ABS = 0, 1, 2, 3
BTN_LEFT, BTN_RIGHT, BTN_MIDDLE = 0x110, 0x111, 0x112
REL_WHEEL = 8
KEYS = {"esc": 1, "ctrl": 29, "f": 33, "c": 46, "z": 44, "q": 16, "x": 45, "shift": 42, "alt": 56,
        "e": 18, "v": 47, "a": 30, "enter": 28}


class Device:
    def __init__(self, name, setup):
        self.fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
        setup(self.fd)
        fcntl.ioctl(self.fd, UI_DEV_SETUP, struct.pack("HHHH80sI", 0x06, 0x1234, 0x5678, 1, name.encode(), 0))
        fcntl.ioctl(self.fd, UI_DEV_CREATE)

    def emit(self, *events):
        data = b"".join(struct.pack("llHHi", 0, 0, t, c, v) for t, c, v in events)
        os.write(self.fd, data + struct.pack("llHHi", 0, 0, EV_SYN, 0, 0))

    def close(self):
        fcntl.ioctl(self.fd, UI_DEV_DESTROY)
        os.close(self.fd)


class Input:
    """An absolute pointer over the whole workspace, and a keyboard."""

    def __init__(self, width, height):
        self.width, self.height = width, height
        self.held_buttons, self.held_keys = set(), set()

        def pointer(fd):
            for ev in (EV_KEY, EV_ABS, EV_REL):
                fcntl.ioctl(fd, UI_SET_EVBIT, ev)
            for b in (BTN_LEFT, BTN_RIGHT, BTN_MIDDLE):
                fcntl.ioctl(fd, UI_SET_KEYBIT, b)
            fcntl.ioctl(fd, UI_SET_RELBIT, REL_WHEEL)
            for axis, size in ((0, width), (1, height)):
                fcntl.ioctl(fd, UI_SET_ABSBIT, axis)
                fcntl.ioctl(fd, UI_ABS_SETUP, struct.pack("Hxxiiiiii", axis, 0, 0, size - 1, 0, 0, 0))

        def keyboard(fd):
            fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
            for code in range(1, 120):
                fcntl.ioctl(fd, UI_SET_KEYBIT, code)

        self.pointer = Device("tinta-check pointer", pointer)
        self.keyboard = Device("tinta-check keyboard", keyboard)
        time.sleep(1.0)  # libinput and KWin pick the devices up

    def move(self, x, y):
        self.pointer.emit((EV_ABS, 0, int(x)), (EV_ABS, 1, int(y)))
        time.sleep(0.02)

    def button(self, down, button=BTN_LEFT):
        self.pointer.emit((EV_KEY, button, 1 if down else 0))
        (self.held_buttons.add if down else self.held_buttons.discard)(button)
        time.sleep(0.03)

    def wheel(self, notches):
        self.pointer.emit((EV_REL, REL_WHEEL, notches))
        time.sleep(0.03)

    def key(self, *names):
        codes = [KEYS[n] for n in names]
        for c in codes:
            self.keyboard.emit((EV_KEY, c, 1))
            self.held_keys.add(c)
            time.sleep(0.02)
        for c in reversed(codes):
            self.keyboard.emit((EV_KEY, c, 0))
            self.held_keys.discard(c)
            time.sleep(0.02)

    def release_all(self):
        for b in list(self.held_buttons):
            self.button(False, b)
        for c in list(self.held_keys):
            self.keyboard.emit((EV_KEY, c, 0))
        self.held_keys.clear()

    def close(self):
        self.release_all()
        self.pointer.close()
        self.keyboard.close()


# ---- Captures ------------------------------------------------------------------------------------
def screenshot(path):
    subprocess.run(["spectacle", "-b", "-n", "-f", "-o", path], env=session_env(), capture_output=True, timeout=30)
    return Image.open(path).convert("RGB")


def window_shot(state, path):
    full = screenshot(path + ".full.png")
    crop = full.crop((state["x"], state["y"], state["x"] + state["w"], state["y"] + state["h"]))
    crop.save(path)
    os.unlink(path + ".full.png")
    return crop


def changed_fraction(a, b):
    if a.size != b.size:
        return 1.0
    diff = ImageChops.difference(a, b).convert("L").point(lambda v: 255 if v > 24 else 0)
    return diff.histogram()[255] / float(a.size[0] * a.size[1])


def screen_size():
    out = subprocess.run(["kscreen-doctor", "-o"], env=session_env(), capture_output=True, text=True).stdout
    out = re.sub(r"\x1b\[[0-9;]*m", "", out)  # colours even into a pipe
    right = bottom = 0
    for line in out.splitlines():
        if "Geometry:" in line:
            pos, size = line.split("Geometry:")[1].split()
            x, y = (int(v) for v in pos.split(","))
            w, h = (int(v) for v in size.split("x"))
            right, bottom = max(right, x + w), max(bottom, y + h)
    return right, bottom


# ---- The run ------------------------------------------------------------------------------------
def run(args):
    global KWIN_DIR
    out = os.path.abspath(args.out)
    os.makedirs(out, exist_ok=True)
    KWIN_DIR = out
    width, height = (int(v) for v in args.size.split("x"))
    results = []

    def report(name, ok, detail):
        results.append(ok)
        print(f"{'PASS' if ok else 'FAIL'} {name}: {detail}", flush=True)

    if qdbus("org.freedesktop.ScreenSaver", "/ScreenSaver", "org.freedesktop.ScreenSaver.GetActive") != "false":
        raise SystemExit("the session is locked (or the screensaver is up): not touching it")
    if tinta_state():
        raise SystemExit("a Tinta window is already open: close it first")

    def env_for(tag, base):
        env = isolated_env(os.path.join(out, ".home", tag), None, "")
        with open(os.path.join(env["XDG_CONFIG_HOME"], "Tinta", "settings.ini"), "a") as f:
            f.write(f"windowWidth={width}\nwindowHeight={height}\nwindowMaximized=0\n")
        return base(env)

    reference = os.path.join(out, "headless.png")
    subprocess.run([args.tinta, args.doc, f"--render-png={reference}", f"--size={width}x{height}"],
                   env=env_for("headless", lambda e: e), cwd=out, capture_output=True, timeout=120)
    ref = Image.open(reference).convert("RGB")

    screen_w, screen_h = screen_size()
    # The checks copy and paste: the user's clipboard comes back afterwards
    saved_clipboard = qdbus("org.kde.klipper", "/klipper", "org.kde.klipper.klipper.getClipboardContents")
    inp = Input(screen_w, screen_h)
    log = open(os.path.join(out, "tinta.log"), "w")
    start = time.monotonic()
    proc = subprocess.Popen([args.tinta, args.doc], env=env_for("desktop", lambda e: {**session_env(e), "TINTA_TRACE_GTK": "1"}), cwd=out,
                            stdout=log, stderr=log)
    try:
        state = None
        while time.monotonic() - start < 20 and proc.poll() is None and not state:
            state = tinta_state()
        report("map", state is not None,
               f"{state['w']}x{state['h']} at +{state['x']}+{state['y']} after "
               f"{(time.monotonic() - start) * 1000:.0f} ms (includes KWin queries), caption '{state['caption']}'"
               if state else f"no window (exit {proc.poll()})")
        if not state:
            return results
        log.flush()
        trace = [line for line in open(log.name).read().splitlines() if line.startswith("tinta-gtk:")]
        display = next((line for line in trace if " display " in line), "no TINTA_TRACE_GTK output")
        report("backend", "GdkWaylandDisplay" in display, display.replace("tinta-gtk: ", ""))

        def guard(x=None, y=None):
            """Tinta active, and (x, y) inside its frame; else stop everything."""
            s = tinta_state()
            if not s or not s["active"] or s["minimized"]:
                activate_tinta()
                s = tinta_state()
            if not s or not s["active"] or s["minimized"]:
                raise Abort("Tinta is not the active window")
            if x is not None and not (s["x"] <= x < s["x"] + s["w"] and s["y"] <= y < s["y"] + s["h"]):
                raise Abort(f"({x}, {y}) is outside Tinta's frame {s}")
            return s

        def click(x, y, button=BTN_LEFT):
            guard(x, y)
            inp.move(x, y)
            inp.button(True, button)
            inp.button(False, button)

        def drag(x0, y0, x1, y1, steps=12):
            guard(x0, y0)
            inp.move(x0, y0)
            inp.button(True)
            for i in range(1, steps + 1):
                inp.move(x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps)
                time.sleep(0.02)
            inp.button(False)
            time.sleep(0.5)

        def double_click(x, y):
            guard(x, y)  # once: a KWin query takes longer than a double-click
            inp.move(x, y)
            for _ in range(2):
                inp.button(True)
                inp.button(False)
                time.sleep(0.05)

        activate_tinta()
        state = guard()
        time.sleep(1.0)
        first = window_shot(state, os.path.join(out, "1-initial.png"))
        frac = changed_fraction(first, ref)
        report("frame", state["w"] == width and state["h"] == height and frac < 0.02,
               f"{state['w']}x{state['h']}, {frac * 100:.2f}% of pixels differ from the headless capture")

        # Caption move: press on empty caption space (left of the caption buttons)
        x0, y0 = state["x"] + state["w"] - 260, state["y"] + 12
        drag(x0, y0, x0 - 200, y0 + 120)
        moved = guard()
        dx, dy = moved["x"] - state["x"], moved["y"] - state["y"]
        report("move", abs(dx + 200) <= 4 and abs(dy - 120) <= 4 and (moved["w"], moved["h"]) == (width, height),
               f"dragged by (-200, +120), window moved by ({dx:+d}, {dy:+d})")
        state = moved

        # Edge resize: the right border is Tinta's own hit test (6 px inside)
        x0, y0 = state["x"] + state["w"] - 3, state["y"] + state["h"] // 2
        drag(x0, y0, x0 + 150, y0)
        grown = guard()
        report("resize-edge", abs(grown["w"] - (state["w"] + 150)) <= 4 and grown["h"] == state["h"],
               f"dragged the right edge by +150: {state['w']}x{state['h']} -> {grown['w']}x{grown['h']}")
        time.sleep(0.6)
        wide = window_shot(grown, os.path.join(out, "2-resized.png"))
        # Laid out again for the new width: not the old frame with a margin
        relaid = changed_fraction(wide.crop((0, 0) + first.size), first)
        report("resize-frame", relaid > 0.003,  # 0 without WM_SIZE; ~1% at +150 px
               f"{relaid * 100:.1f}% of the old area differs after the relayout")
        state = grown

        # Caption double-click maximizes; again restores
        double_click(state["x"] + state["w"] - 260, state["y"] + 12)
        time.sleep(0.8)
        big = guard()
        area = big["area"]
        report("maximize", (big["x"], big["y"], big["w"], big["h"]) == area,
               f"after a caption double-click: {big['w']}x{big['h']} at +{big['x']}+{big['y']}, work area {area}")
        window_shot(big, os.path.join(out, "3-maximized.png"))
        double_click(big["x"] + big["w"] - 260, big["y"] + 12)
        time.sleep(0.8)
        back = guard()
        report("restore", (back["w"], back["h"]) == (state["w"], state["h"]),
               f"second double-click: {back['w']}x{back['h']} (was {state['w']}x{state['h']})")
        state = back

        # Wheel and keys (Ctrl+F, then typing) inside the document
        cx, cy = state["x"] + state["w"] // 2, state["y"] + state["h"] // 2
        guard(cx, cy)
        inp.move(cx, cy)
        before = window_shot(state, os.path.join(out, "4-before-wheel.png"))
        for _ in range(5):
            guard(cx, cy)
            inp.wheel(-1)
        time.sleep(0.6)
        after = window_shot(guard(), os.path.join(out, "4-wheel.png"))
        frac = changed_fraction(after, before)
        report("wheel", frac > 0.05, f"{frac * 100:.1f}% of pixels changed after 5 notches")

        guard()
        inp.key("ctrl", "f")
        time.sleep(0.5)
        opened = window_shot(guard(), os.path.join(out, "5-find-open.png"))
        for name in ("z", "q", "x"):
            guard()
            inp.key(name)
        time.sleep(0.5)
        typed = window_shot(guard(), os.path.join(out, "5-find.png"))
        a, b = changed_fraction(opened, after), changed_fraction(typed, opened)
        report("keys", a > 0.0005 and b > 0.0002, f"Ctrl+F changed {a * 100:.2f}%, typing 'zqx' {b * 100:.3f}%")
        guard()
        inp.key("esc")  # closes the find bar; a second Escape would close Tinta
        time.sleep(0.4)

        # Copy to the desktop clipboard (Klipper reads it back)
        guard()
        inp.key("ctrl", "c")
        time.sleep(0.6)
        text = qdbus("org.kde.klipper", "/klipper", "org.kde.klipper.klipper.getClipboardContents")
        report("clipboard", args.marker in text, f"Klipper holds {len(text)} chars, marker "
                                                  f"{'found' if args.marker in text else 'missing'}")

        # Paste from the desktop: Klipper's text into the find bar
        qdbus("org.kde.klipper", "/klipper", "org.kde.klipper.klipper.setClipboardContents", "zqxpaste")
        time.sleep(0.3)
        guard()
        inp.key("ctrl", "f")
        time.sleep(0.4)
        empty = window_shot(guard(), os.path.join(out, "6-find-empty.png"))
        guard()
        inp.key("ctrl", "v")
        time.sleep(0.5)
        pasted = window_shot(guard(), os.path.join(out, "6-find-pasted.png"))
        frac = changed_fraction(pasted, empty)
        report("paste", frac > 0.0002, f"Ctrl+V into the find bar changed {frac * 100:.3f}% of pixels")
        guard()
        inp.key("esc")
        time.sleep(0.4)

        # Minimize from Tinta's caption button (left of maximize and close)
        state = guard()
        click(state["x"] + state["w"] - 115, state["y"] + 15)
        time.sleep(0.8)
        mini = tinta_state()
        report("minimize", bool(mini and mini["minimized"]), f"minimized={mini and mini['minimized']}")
        activate_tinta()

        # Close from Tinta's caption close button
        state = guard()
        click(state["x"] + state["w"] - 20, state["y"] + 15)
        try:
            code = proc.wait(timeout=8)
            report("close", code == 0, f"exited with {code} after the caption close click")
        except subprocess.TimeoutExpired:
            report("close", False, "still running 8 s after the caption close click")
    except Abort as e:
        report("guard", False, f"stopped: {e}")
    finally:
        inp.close()
        qdbus("org.kde.klipper", "/klipper", "org.kde.klipper.klipper.setClipboardContents", saved_clipboard)
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
        log.close()
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("tinta")
    parser.add_argument("doc")
    parser.add_argument("--out", default="build/desktop-check")
    parser.add_argument("--size", default="1000x700")
    parser.add_argument("--marker", default="tinta-gtk-7Q4")
    args = parser.parse_args()
    args.tinta = os.path.abspath(args.tinta)
    args.doc = os.path.abspath(args.doc)
    results = run(args)
    print(f"{sum(results)}/{len(results)} checks passed")
    return 0 if results and all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
