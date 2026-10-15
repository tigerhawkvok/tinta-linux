#!/usr/bin/env python3
"""Interactive-window check for the Linux build, on a private X server.

Starts Tinta in a GTK window under Xvfb (never on the user's desktop), then
drives it with real X input (XTest) and reads the results back from the
screen and the X clipboard:

  map       the window appears; time from launch to a viewable window
  frame     its first settled frame matches the headless --render-png capture
  wheel     wheel notches scroll the document (the frame changes)
  keys      Ctrl+F and typing open the find bar with text (the frame changes)
  clipboard Ctrl+C with no selection puts the document text on CLIPBOARD
  resize    an X resize reaches WM_SIZE: the frame fills the new size
  close     a click on Tinta's own caption close button exits with status 0

Usage: gui_check.py TINTA DOC [--out DIR] [--size WxH] [--marker TEXT]
TINTA is the binary or an AppImage. Needs Pillow, Xvfb (xvfb-run) and
libXtst. Exit status is 1 when any check fails.
"""
import argparse
import ctypes
import ctypes.util
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from render_fixtures import isolated_env  # noqa: E402

from PIL import Image, ImageChops  # noqa: E402


# ---- Xlib through ctypes (no python-xlib dependency) ---------------------------------
class XWindowAttributes(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int), ("y", ctypes.c_int), ("width", ctypes.c_int), ("height", ctypes.c_int),
                ("border_width", ctypes.c_int), ("depth", ctypes.c_int), ("visual", ctypes.c_void_p),
                ("root", ctypes.c_ulong), ("class_", ctypes.c_int), ("bit_gravity", ctypes.c_int),
                ("win_gravity", ctypes.c_int), ("backing_store", ctypes.c_int),
                ("backing_planes", ctypes.c_ulong), ("backing_pixel", ctypes.c_ulong),
                ("save_under", ctypes.c_int), ("colormap", ctypes.c_ulong), ("map_installed", ctypes.c_int),
                ("map_state", ctypes.c_int), ("all_event_masks", ctypes.c_long),
                ("your_event_mask", ctypes.c_long), ("do_not_propagate_mask", ctypes.c_long),
                ("override_redirect", ctypes.c_int), ("screen", ctypes.c_void_p)]


class XImage(ctypes.Structure):  # the leading fields only
    _fields_ = [("width", ctypes.c_int), ("height", ctypes.c_int), ("xoffset", ctypes.c_int),
                ("format", ctypes.c_int), ("data", ctypes.c_void_p), ("byte_order", ctypes.c_int),
                ("bitmap_unit", ctypes.c_int), ("bitmap_bit_order", ctypes.c_int), ("bitmap_pad", ctypes.c_int),
                ("depth", ctypes.c_int), ("bytes_per_line", ctypes.c_int), ("bits_per_pixel", ctypes.c_int)]


class X:
    def __init__(self):
        self.lib = ctypes.CDLL(ctypes.util.find_library("X11") or "libX11.so.6")
        self.tst = ctypes.CDLL(ctypes.util.find_library("Xtst") or "libXtst.so.6")
        lib, tst = self.lib, self.tst
        lib.XOpenDisplay.restype = ctypes.c_void_p
        lib.XOpenDisplay.argtypes = [ctypes.c_char_p]
        lib.XDefaultRootWindow.restype = ctypes.c_ulong
        lib.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
        lib.XQueryTree.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong),
                                   ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.POINTER(ctypes.c_ulong)),
                                   ctypes.POINTER(ctypes.c_uint)]
        lib.XGetWindowAttributes.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(XWindowAttributes)]
        lib.XTranslateCoordinates.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_int,
                                              ctypes.c_int, ctypes.POINTER(ctypes.c_int),
                                              ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_ulong)]
        lib.XResizeWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_uint, ctypes.c_uint]
        lib.XStringToKeysym.restype = ctypes.c_ulong
        lib.XStringToKeysym.argtypes = [ctypes.c_char_p]
        lib.XKeysymToKeycode.restype = ctypes.c_ubyte
        lib.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        lib.XInternAtom.restype = ctypes.c_ulong
        lib.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
        lib.XCreateSimpleWindow.restype = ctypes.c_ulong
        lib.XCreateSimpleWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong] + [ctypes.c_int] * 2 + \
            [ctypes.c_uint] * 3 + [ctypes.c_ulong] * 2
        lib.XConvertSelection.argtypes = [ctypes.c_void_p] + [ctypes.c_ulong] * 5
        lib.XCheckTypedWindowEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_void_p]
        lib.XGetWindowProperty.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_long,
                                           ctypes.c_long, ctypes.c_int, ctypes.c_ulong,
                                           ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_int),
                                           ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_ulong),
                                           ctypes.POINTER(ctypes.c_void_p)]
        lib.XFree.argtypes = [ctypes.c_void_p]
        lib.XGetImage.restype = ctypes.POINTER(XImage)
        lib.XGetImage.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_int, ctypes.c_uint,
                                  ctypes.c_uint, ctypes.c_ulong, ctypes.c_int]
        lib.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
        tst.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
        tst.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
        tst.XTestFakeMotionEvent.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                             ctypes.c_ulong]
        self.d = lib.XOpenDisplay(None)
        if not self.d:
            raise SystemExit("cannot open the X display")
        self.root = lib.XDefaultRootWindow(self.d)

    def sync(self):
        self.lib.XSync(self.d, 0)

    def title(self, window):
        """_NET_WM_NAME (UTF-8), as GTK sets it."""
        lib, d = self.lib, self.d
        kind, fmt = ctypes.c_ulong(), ctypes.c_int()
        count, after, data = ctypes.c_ulong(), ctypes.c_ulong(), ctypes.c_void_p()
        lib.XGetWindowProperty(d, window, lib.XInternAtom(d, b"_NET_WM_NAME", 0), 0, 4096, 0,
                               lib.XInternAtom(d, b"UTF8_STRING", 0), ctypes.byref(kind), ctypes.byref(fmt),
                               ctypes.byref(count), ctypes.byref(after), ctypes.byref(data))
        if not data.value:
            return ""
        text = ctypes.string_at(data.value, count.value).decode("utf-8", "replace")
        lib.XFree(data)
        return text

    def toplevels(self):
        """Viewable children of the root: (window, x, y, width, height)."""
        root, parent = ctypes.c_ulong(), ctypes.c_ulong()
        children, n = ctypes.POINTER(ctypes.c_ulong)(), ctypes.c_uint()
        if not self.lib.XQueryTree(self.d, self.root, ctypes.byref(root), ctypes.byref(parent),
                                   ctypes.byref(children), ctypes.byref(n)):
            return []
        found = []
        for i in range(n.value):
            w = children[i]
            attrs = XWindowAttributes()
            if not self.lib.XGetWindowAttributes(self.d, w, ctypes.byref(attrs)):
                continue
            if attrs.map_state != 2 or attrs.override_redirect or attrs.width < 100:  # IsViewable
                continue
            x, y, child = ctypes.c_int(), ctypes.c_int(), ctypes.c_ulong()
            self.lib.XTranslateCoordinates(self.d, w, self.root, 0, 0, ctypes.byref(x), ctypes.byref(y),
                                           ctypes.byref(child))
            found.append((w, x.value, y.value, attrs.width, attrs.height))
        if n.value:
            self.lib.XFree(children)
        return found

    def grab(self, x, y, width, height):
        """The root window's pixels (24-bit TrueColor, 32 bits a pixel)."""
        image = self.lib.XGetImage(self.d, self.root, x, y, width, height, 0xFFFFFFFF, 2)  # ZPixmap
        if not image:
            raise RuntimeError("XGetImage failed")
        img = image.contents
        size = img.bytes_per_line * img.height
        pixels = Image.frombuffer("RGB", (img.width, img.height), ctypes.string_at(img.data, size), "raw", "BGRX",
                                  img.bytes_per_line, 1).copy()
        self.lib.XFree(img.data)
        self.lib.XFree(image)
        return pixels

    def resize(self, window, width, height):
        self.lib.XResizeWindow(self.d, window, width, height)
        self.sync()

    def move(self, x, y):
        self.tst.XTestFakeMotionEvent(self.d, -1, x, y, 0)
        self.sync()

    def click(self, button, x=None, y=None):
        if x is not None:
            self.move(x, y)
            time.sleep(0.05)
        self.tst.XTestFakeButtonEvent(self.d, button, 1, 0)
        self.sync()
        time.sleep(0.03)
        self.tst.XTestFakeButtonEvent(self.d, button, 0, 0)
        self.sync()

    def keycode(self, name):
        return self.lib.XKeysymToKeycode(self.d, self.lib.XStringToKeysym(name.encode()))

    def key(self, *names):
        """Presses the keys in order and releases them in reverse (a chord)."""
        codes = [self.keycode(n) for n in names]
        for c in codes:
            self.tst.XTestFakeKeyEvent(self.d, c, 1, 0)
            self.sync()
            time.sleep(0.01)
        for c in reversed(codes):
            self.tst.XTestFakeKeyEvent(self.d, c, 0, 0)
            self.sync()
            time.sleep(0.01)

    def clipboard_text(self, timeout=3.0):
        """CLIPBOARD as UTF8_STRING, or None when no owner answers."""
        lib, d = self.lib, self.d
        window = lib.XCreateSimpleWindow(d, self.root, 0, 0, 1, 1, 0, 0, 0)
        clipboard = lib.XInternAtom(d, b"CLIPBOARD", 0)
        utf8 = lib.XInternAtom(d, b"UTF8_STRING", 0)
        prop = lib.XInternAtom(d, b"TINTA_GUI_CHECK", 0)
        lib.XConvertSelection(d, clipboard, utf8, prop, window, 0)
        self.sync()
        event = ctypes.create_string_buffer(256)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if lib.XCheckTypedWindowEvent(d, window, 31, event):  # SelectionNotify
                break
            time.sleep(0.02)
        else:
            return None
        kind, fmt = ctypes.c_ulong(), ctypes.c_int()
        count, after, data = ctypes.c_ulong(), ctypes.c_ulong(), ctypes.c_void_p()
        lib.XGetWindowProperty(d, window, prop, 0, 1 << 24, 1, 0, ctypes.byref(kind), ctypes.byref(fmt),
                               ctypes.byref(count), ctypes.byref(after), ctypes.byref(data))
        if not data.value or count.value == 0:
            return None
        text = ctypes.string_at(data.value, count.value).decode("utf-8", "replace")
        lib.XFree(data)
        return text


# ---- Screen captures --------------------------------------------------------------------
def capture(x, geometry, path):
    _, gx, gy, w, h = geometry
    frame = x.grab(gx, gy, w, h)
    frame.save(path)
    return frame


def changed_fraction(a, b):
    if a.size != b.size:
        return 1.0
    diff = ImageChops.difference(a, b).convert("L").point(lambda v: 255 if v > 24 else 0)
    hist = diff.histogram()
    return hist[255] / float(a.size[0] * a.size[1])


def settle(x, geometry, path, quiet_ms=400, timeout=8.0):
    """Captures until two frames quiet_ms apart agree; returns the last."""
    last = capture(x, geometry, path)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        time.sleep(quiet_ms / 1000.0)
        frame = capture(x, geometry, path)
        if changed_fraction(frame, last) == 0.0:
            return frame
        last = frame
    return last


DROP_DOC = os.path.join(HERE, "..", "fixtures", "markdown-regression-control.md")


def drop_check(x, geometry, out):
    """(ok, detail) for dragging DROP_DOC from another process onto Tinta."""
    helper = os.path.join(out, "drag_source")
    built = subprocess.run(f'cc "{os.path.join(HERE, "drag_source.c")}" $(pkg-config --cflags --libs gtk4) '
                           f'-o "{helper}"', shell=True, capture_output=True, text=True)
    if built.returncode:
        return False, f"drag_source.c did not build: {built.stderr.strip()[:200]}"
    env = dict(os.environ)
    env.pop("WAYLAND_DISPLAY", None)
    env["GDK_BACKEND"] = "x11"
    env.setdefault("GSK_RENDERER", "cairo")
    known = {g[0] for g in x.toplevels()}
    source = subprocess.Popen([helper, os.path.abspath(DROP_DOC)], env=env, stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL)
    try:
        src = None
        for _ in range(200):
            src = next((g for g in x.toplevels() if g[0] not in known), None)
            if src:
                break
            time.sleep(0.05)
        if not src:
            return False, "the drag source window did not appear"
        time.sleep(0.5)
        before = x.title(geometry[0])
        sx, sy = src[1] + src[3] // 2, src[2] + src[4] // 2
        tx, ty = geometry[1] + geometry[3] * 3 // 5, geometry[2] + geometry[4] * 3 // 5
        x.move(sx, sy)
        time.sleep(0.1)
        x.tst.XTestFakeButtonEvent(x.d, 1, 1, 0)
        x.sync()
        for step in range(1, 21):
            x.move(sx + (tx - sx) * step // 20, sy + (ty - sy) * step // 20)
            time.sleep(0.04)
        time.sleep(0.3)
        x.tst.XTestFakeButtonEvent(x.d, 1, 0, 0)
        x.sync()
        after = before
        for _ in range(60):
            time.sleep(0.05)
            after = x.title(geometry[0])
            if "markdown-regression-control" in after:
                break
        ok = "markdown-regression-control" in after and after != before
        return ok, f"title '{before}' -> '{after}'"
    finally:
        source.terminate()
        source.wait(timeout=5)
        time.sleep(0.3)


# ---- The run ------------------------------------------------------------------------------
def run(args):
    out = os.path.abspath(args.out)
    os.makedirs(out, exist_ok=True)
    width, height = (int(v) for v in args.size.split("x"))
    results = []

    def report(name, ok, detail):
        results.append(ok)
        print(f"{'PASS' if ok else 'FAIL'} {name}: {detail}", flush=True)

    # Isolated settings with the window size; the same for the headless reference
    def env_for(tag):
        env = isolated_env(os.path.join(out, ".home", tag), None, "")
        with open(os.path.join(env["XDG_CONFIG_HOME"], "Tinta", "settings.ini"), "a") as f:
            f.write(f"windowWidth={width}\nwindowHeight={height}\nwindowMaximized=0\n")
        env.pop("WAYLAND_DISPLAY", None)
        # Always X11: without WAYLAND_DISPLAY, GTK would still try the default
        # Wayland socket, which is the user's live session
        env["GDK_BACKEND"] = "x11"
        env.setdefault("GSK_RENDERER", "cairo")  # Vulkan is software under Xvfb
        env["GDK_SCALE"] = str(args.scale)  # X11 takes whole-number scales
        return env

    reference = os.path.join(out, "headless.png")
    subprocess.run([args.tinta, args.doc, f"--render-png={reference}", f"--size={width}x{height}",
                    f"--scale={args.scale}"],
                   env=env_for("headless"), cwd=out, capture_output=True, timeout=120)

    x = X()
    log = open(os.path.join(out, "tinta.log"), "w")
    start = time.monotonic()
    proc = subprocess.Popen([args.tinta, args.doc], env=env_for("gui"), cwd=out, stdout=log, stderr=log)
    geometry = None
    while time.monotonic() - start < 20 and proc.poll() is None:
        windows = x.toplevels()
        if windows:
            geometry = max(windows, key=lambda g: g[3] * g[4])
            break
        time.sleep(0.01)
    mapped_ms = (time.monotonic() - start) * 1000
    report("map", geometry is not None,
           f"{geometry[3]}x{geometry[4]} at +{geometry[1]}+{geometry[2]} after {mapped_ms:.0f} ms"
           if geometry else f"no window (exit {proc.poll()})")
    if not geometry:
        proc.kill()
        return results

    first = settle(x, geometry, os.path.join(out, "1-initial.png"))
    ref = Image.open(reference).convert("RGB") if os.path.exists(reference) else None
    if ref is None:
        report("frame", False, "headless reference missing")
    else:
        frac = changed_fraction(first, ref)
        report("frame", frac < 0.02, f"{frac * 100:.2f}% of pixels differ from the headless capture "
                                     f"({first.size[0]}x{first.size[1]} vs {ref.size[0]}x{ref.size[1]})")

    cx, cy = geometry[1] + geometry[3] // 2, geometry[2] + geometry[4] // 2
    x.move(cx, cy)
    for _ in range(5):
        x.click(5)  # wheel down
        time.sleep(0.03)
    scrolled = settle(x, geometry, os.path.join(out, "2-wheel.png"))
    frac = changed_fraction(scrolled, first)
    report("wheel", frac > 0.05, f"{frac * 100:.1f}% of pixels changed after 5 notches")

    # A shortcut (WM_KEYDOWN) opens the find bar; typing (WM_CHAR from
    # TranslateMessage) must then change it on its own
    x.key("Control_L", "f")
    opened = settle(x, geometry, os.path.join(out, "3-find-open.png"))
    shortcut = changed_fraction(opened, scrolled)
    for name in ("z", "q", "x"):
        x.key(name)
    found = settle(x, geometry, os.path.join(out, "3-find.png"))
    typed = changed_fraction(found, opened)
    report("keys", shortcut > 0.0005 and typed > 0.0002,
           f"Ctrl+F changed {shortcut * 100:.2f}% of pixels, typing 'zqx' {typed * 100:.3f}%")
    x.key("Escape")
    # One Escape closes the find bar; a second would close Tinta (upstream behaviour)
    time.sleep(0.3)

    x.key("Control_L", "c")
    time.sleep(0.3)
    text = x.clipboard_text()
    report("clipboard", bool(text) and args.marker in text,
           f"{len(text)} chars, marker {'found' if args.marker in text else 'missing'}" if text
           else "no CLIPBOARD owner answered")
    whole = len(text or "")

    # Client-area mouse: a drag selects part of the document, which Ctrl+C
    # then copies instead of everything
    left, top = geometry[1], geometry[2]
    x.move(left + 60, top + 160)
    time.sleep(0.05)
    x.tst.XTestFakeButtonEvent(x.d, 1, 1, 0)
    x.sync()
    for step in range(1, 11):
        x.move(left + 60 + step * 40, top + 160 + step * 12)
        time.sleep(0.02)
    x.tst.XTestFakeButtonEvent(x.d, 1, 0, 0)
    x.sync()
    time.sleep(0.2)
    x.key("Control_L", "c")
    time.sleep(0.3)
    part = x.clipboard_text() or ""
    report("select", 0 < len(part) < whole, f"drag copied {len(part)} of {whole} chars")

    # Drag and drop from another application (drag_source.c stands in for a
    # file manager): Tinta opens the file in a tab, and the title names it
    report("drop", *drop_check(x, geometry, out))

    before = settle(x, geometry, os.path.join(out, "4-before-resize.png"))
    small = (geometry[0], geometry[1], geometry[2], 700, 500)
    x.resize(geometry[0], 700, 500)
    time.sleep(0.5)
    now = [g for g in x.toplevels() if g[0] == geometry[0]]
    if now:
        small = now[0]
    resized = settle(x, small, os.path.join(out, "4-resize.png"))
    # The frame must follow: drawn to the bottom-right corner, and laid out
    # again (not the old frame cropped, as without WM_SIZE)
    corner = resized.crop((resized.size[0] - 24, resized.size[1] - 24, resized.size[0], resized.size[1]))
    lit = corner.size[0] * corner.size[1] - corner.convert("L").histogram()[0]  # not black
    relaid = changed_fraction(resized, before.crop((0, 0) + resized.size))
    report("resize", small[3:] == (700, 500) and lit > 24 * 24 * 0.9 and relaid > 0.01,
           f"window {small[3]}x{small[4]}, {lit}/{24 * 24} corner pixels drawn, "
           f"{relaid * 100:.1f}% differs from the old frame cropped")

    # Tinta's caption buttons sit at the top right; close is the last
    x.move(small[1] + small[3] - 60, small[2] + 15)
    time.sleep(0.1)
    x.click(1, small[1] + small[3] - 20, small[2] + 15)
    try:
        code = proc.wait(timeout=8)
        report("close", code == 0, f"exited with {code} after the caption close click")
    except subprocess.TimeoutExpired:
        proc.kill()
        report("close", False, "still running 8 s after the caption close click")
    log.close()
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("tinta")
    parser.add_argument("doc")
    parser.add_argument("--out", default="build/gui-check")
    parser.add_argument("--size", default="1000x700", help="window size in device pixels")
    parser.add_argument("--scale", type=int, default=1, help="GDK_SCALE (HiDPI)")
    parser.add_argument("--marker", default="tinta-gtk-7Q4")
    parser.add_argument("--inner", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    args.tinta = os.path.abspath(args.tinta)
    args.doc = os.path.abspath(args.doc)
    if not args.inner:
        # A private X server: never the user's session
        xvfb = shutil.which("xvfb-run")
        if not xvfb:
            raise SystemExit("xvfb-run not found")
        env = dict(os.environ)
        env.pop("WAYLAND_DISPLAY", None)
        env.pop("DISPLAY", None)
        # A private session bus too: Tinta is single-instance over D-Bus, and
        # must neither join nor host the user's own Tinta
        env.pop("DBUS_SESSION_BUS_ADDRESS", None)
        cmd = ["dbus-run-session", "--", xvfb, "-a", "-s", "-screen 0 1600x1200x24", sys.executable, os.path.abspath(__file__),
               args.tinta, args.doc, "--out", args.out, "--size", args.size, "--scale", str(args.scale), "--marker", args.marker, "--inner"]
        return subprocess.run(cmd, env=env).returncode
    results = run(args)
    print(f"{sum(results)}/{len(results)} checks passed")
    return 0 if results and all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
