#!/usr/bin/env python3
"""Light/dark following on Linux, end to end, on a private bus and display.

Tinta asks the XDG desktop portal for org.freedesktop.appearance
color-scheme (as Windows' AppsUseLightTheme) and follows its SettingChanged
signal (as WM_SETTINGCHANGE "ImmersiveColorSet"). This runs a fake portal
on a private D-Bus session (dbus-run-session) and Tinta on a private Xvfb
server, with followSystemTheme=1, light = Paper and dark = Wombat:

  start-dark  the portal says dark at launch: Tinta opens in Wombat
  to-light    the portal switches to light: Tinta repaints in Paper
  to-dark     and back to dark: Wombat again

Usage: follow_system_check.py TINTA DOC [--out DIR]
Needs dbus-run-session, xvfb-run, Pillow and dbus-next (pip).
"""
import argparse
import asyncio
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

LIGHT, DARK = 0, 10  # Paper, Wombat
SAMPLE = (12, 400)   # document margin: the theme's background


# ---- The fake portal (--portal): scheme changes arrive on stdin ----------------------------------
def portal():
    from dbus_next import Variant
    from dbus_next.aio import MessageBus
    from dbus_next.errors import DBusError
    from dbus_next.service import ServiceInterface, method, signal

    class Settings(ServiceInterface):
        def __init__(self):
            super().__init__("org.freedesktop.portal.Settings")
            self.scheme = 1

        def value(self, namespace, key):
            if namespace == "org.freedesktop.appearance" and key == "color-scheme":
                return Variant("u", self.scheme)
            raise DBusError("org.freedesktop.portal.Error.NotFound", f"{namespace} {key}")

        @method()
        def ReadOne(self, namespace: "s", key: "s") -> "v":  # noqa: F821
            return self.value(namespace, key)

        @method()
        def Read(self, namespace: "s", key: "s") -> "v":  # noqa: F821
            return Variant("v", self.value(namespace, key))

        @signal()
        def SettingChanged(self) -> "ssv":  # noqa: F821
            return ["org.freedesktop.appearance", "color-scheme", Variant("u", self.scheme)]

    async def serve():
        bus = await MessageBus().connect()
        settings = Settings()
        bus.export("/org/freedesktop/portal/desktop", settings)
        await bus.request_name("org.freedesktop.portal.Desktop")
        print("ready", flush=True)
        loop = asyncio.get_running_loop()
        while True:
            line = await loop.run_in_executor(None, sys.stdin.readline)
            if not line:
                return
            settings.scheme = int(line)
            settings.SettingChanged()
            print("sent", settings.scheme, flush=True)

    asyncio.run(serve())


# ---- The checks (--inner, on the private bus and display) ------------------------------------------
def inner(args):
    from gui_check import X
    from render_fixtures import isolated_env, settings_ini  # noqa: F401
    from PIL import Image

    out = args.out
    results = []

    def report(name, ok, detail):
        results.append(ok)
        print(f"{'PASS' if ok else 'FAIL'} {name}: {detail}", flush=True)

    def expected(theme_index):
        # The background each theme paints, from a headless render
        env = isolated_env(os.path.join(out, ".home", f"ref{theme_index}"), None, "")
        with open(os.path.join(env["XDG_CONFIG_HOME"], "Tinta", "settings.ini"), "w") as f:
            f.write(f"checkUpdates=0\nhasAskedFileAssociation=1\nfollowSystemTheme=0\nthemeIndex={theme_index}\n")
        path = os.path.join(out, f"ref{theme_index}.png")
        subprocess.run([args.tinta, args.doc, f"--render-png={path}", "--size=1000x700"], env=env, cwd=out,
                       capture_output=True, timeout=120)
        return Image.open(path).convert("RGB").getpixel(SAMPLE)

    light, dark = expected(LIGHT), expected(DARK)
    print(f"background: Paper {light}, Wombat {dark}", flush=True)

    fake = subprocess.Popen([sys.executable, os.path.abspath(__file__), "--portal"], stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, text=True)
    if fake.stdout.readline().strip() != "ready":
        report("portal", False, "the fake portal did not start")
        return results

    env = isolated_env(os.path.join(out, ".home", "follow"), None, "")
    with open(os.path.join(env["XDG_CONFIG_HOME"], "Tinta", "settings.ini"), "w") as f:
        f.write(f"checkUpdates=0\nhasAskedFileAssociation=1\nfollowSystemTheme=1\nlightThemeIndex={LIGHT}\n"
                f"darkThemeIndex={DARK}\nwindowWidth=1000\nwindowHeight=700\nwindowMaximized=0\n")
    env.pop("WAYLAND_DISPLAY", None)
    env["GDK_BACKEND"] = "x11"  # never the user's Wayland session
    env.setdefault("GSK_RENDERER", "cairo")
    log = open(os.path.join(out, "tinta.log"), "w")
    proc = subprocess.Popen([args.tinta, args.doc], env=env, cwd=out, stdout=log, stderr=log)
    x = X()
    geometry = None
    for _ in range(400):
        windows = x.toplevels()
        if windows:
            geometry = max(windows, key=lambda g: g[3] * g[4])
            break
        time.sleep(0.05)
    if not geometry:
        report("start-dark", False, "no window")
        proc.kill()
        return results

    def background(wait=1.5):
        time.sleep(wait)
        return x.grab(geometry[1] + SAMPLE[0], geometry[2] + SAMPLE[1], 1, 1).getpixel((0, 0))

    def switch(scheme):
        fake.stdin.write(f"{scheme}\n")
        fake.stdin.flush()
        fake.stdout.readline()

    seen = background()
    report("start-dark", seen == dark, f"portal dark at launch: background {seen}, Wombat is {dark}")
    switch(2)
    seen = background()
    report("to-light", seen == light, f"portal switched to light: background {seen}, Paper is {light}")
    switch(1)
    seen = background()
    report("to-dark", seen == dark, f"portal switched to dark: background {seen}, Wombat is {dark}")

    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
    fake.stdin.close()
    fake.wait(timeout=5)
    log.close()
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("tinta", nargs="?")
    parser.add_argument("doc", nargs="?")
    parser.add_argument("--out", default="build/follow-system-check")
    parser.add_argument("--inner", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--portal", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.portal:
        portal()
        return 0
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
    cmd = ["dbus-run-session", "--", shutil.which("xvfb-run"), "-a", "-s", "-screen 0 1600x1200x24",
           sys.executable, os.path.abspath(__file__), args.tinta, args.doc, "--out", args.out, "--inner"]
    return subprocess.run(cmd, env=env).returncode


if __name__ == "__main__":
    sys.exit(main())
