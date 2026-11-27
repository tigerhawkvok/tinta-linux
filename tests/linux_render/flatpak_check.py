#!/usr/bin/env python3
"""The built Flatpak, run from its build directory (not installed): a
headless render in the sandbox, then the window on Xvfb. X11 only: the
Wayland socket is withheld, so nothing reaches a live session. Its own
XDG_CONFIG_HOME: flatpak build has no per-app directories, and with
--filesystem=host Tinta would read and rewrite the real settings.

  python3 flatpak_check.py build-flatpak/build-dir tests/fixtures/mermaid-label-fit.md build/flatpak-check

It starts its own Xvfb and D-Bus session with a runtime dir of its own
(private_session.py): flatpak and the portals it may start live in
XDG_RUNTIME_DIR, and the user's real one must not be touched.

Prints the render's size, the bundled fonts and metainfo in /app, and the
window; window.png and headless.png land in OUT.
"""
import os
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import private_session  # noqa: E402

build_dir, doc, out = (os.path.abspath(a) for a in sys.argv[1:4])
os.makedirs(out, exist_ok=True)
if '--inner' not in sys.argv:
    # The session first: Xvfb, a private bus and a private runtime dir
    sys.exit(private_session.run(
        ['dbus-run-session', '--', shutil.which('xvfb-run'), '-a', '-s', '-screen 0 1600x1200x24',
         sys.executable, os.path.abspath(__file__), build_dir, doc, out, '--inner']).returncode)

from gui_check import X  # noqa: E402
env = dict(os.environ)
env.pop('WAYLAND_DISPLAY', None)
# Its own configuration: flatpak build has no per-app directories, and with
# --filesystem=host Tinta would otherwise read and rewrite the real
# ~/.config/Tinta/settings.ini
config = os.path.join(out, 'config')
os.makedirs(os.path.join(config, 'Tinta'), exist_ok=True)
with open(os.path.join(config, 'Tinta', 'settings.ini'), 'w') as f:
    f.write('[Settings]\nthemeIndex=0\nfollowSystemTheme=0\nwindowWidth=1000\nwindowHeight=760\n'
            'windowMaximized=0\ncheckUpdates=0\nhasAskedFileAssociation=1\n')
base = ['flatpak', 'build', '--nosocket=wayland', '--socket=x11', '--share=ipc', '--filesystem=host',
        '--env=GDK_BACKEND=x11', '--env=GSK_RENDERER=cairo', '--env=XDG_CONFIG_HOME=' + config,
        '--env=XDG_CACHE_HOME=' + os.path.join(out, 'cache'), build_dir]

png = os.path.join(out, 'headless.png')
r = subprocess.run(base + ['tinta', doc, '--render-png=' + png, '--size=1000x1600'], env=env, capture_output=True,
                   text=True, timeout=120)
print('headless render: exit', r.returncode, os.path.getsize(png) if os.path.exists(png) else 'no file')
version = subprocess.run(base + ['sh', '-c', 'ls /app/share/tinta/fonts /app/share/metainfo; '
                                 'true'],
                         env=env, capture_output=True, text=True, timeout=60)
print(version.stdout.strip())

x = X()
log = open(os.path.join(out, 'window.log'), 'w')
proc = subprocess.Popen(base + ['tinta', doc], env=env, stdout=log, stderr=log)
start = time.monotonic()
while time.monotonic() - start < 30 and not x.toplevels():
    time.sleep(0.1)
time.sleep(3.0)
wins = x.toplevels()
print('windows:', [(w[3], w[4], x.title(w[0])) for w in wins])
x.grab(0, 0, 1400, 1000).save(os.path.join(out, 'window.png'))
proc.terminate()
proc.wait(timeout=10)
print('done')
