#!/usr/bin/env python3
"""A private desktop session for the checks: their own D-Bus bus, their own
X or Wayland server, and their own XDG_RUNTIME_DIR.

dbus-run-session gives a private bus but keeps the user's runtime dir,
/run/user/UID, and whatever starts on that bus lives there as if it were
the user's. On 2026-11-26 a check's file drop (GTK 4.24, Ubuntu 26.04)
started the document portal, the KDE portal and kwallet on a private bus.
The document portal mounted, and on exit unmounted, the user's real
/run/user/UID/doc. Every Flatpak app then failed to start ("bwrap: Can't
find source path .../doc/by-app/..."), and the daemons outlived their bus.

So every nested session the checks start goes through run(), or through
the command line below for ctest and the release script:
- a fresh 0700 runtime dir per session;
- afterwards, whatever still runs with that dir is stopped and whatever is
  mounted in it is unmounted;
- then the dir is removed, never through a mount.
GTK is also told to ask no portals (GDK_DEBUG=no-portals).

  private_session.py [--dbus] -- COMMAND...    --dbus: inside dbus-run-session
"""
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time

PREFIX = "tinta-session-"


def _mounts_under(path):
    """Mount points at or under path, deepest first."""
    found = []
    try:
        with open("/proc/self/mountinfo", encoding="utf-8") as f:
            for line in f:
                point = line.split()[4].encode().decode("unicode_escape")
                if point == path or point.startswith(path + "/"):
                    found.append(point)
    except OSError:
        pass
    return sorted(found, key=len, reverse=True)


def _processes_with(runtime_dir):
    """Our processes that inherited this runtime dir (the session's leftovers)."""
    needle = ("XDG_RUNTIME_DIR=" + runtime_dir).encode()
    pids = []
    for name in os.listdir("/proc"):
        if not name.isdigit() or int(name) == os.getpid():
            continue
        try:
            with open(f"/proc/{name}/environ", "rb") as f:
                if needle in f.read().split(b"\0"):
                    pids.append(int(name))
        except OSError:
            continue  # gone, or not ours
    return pids


def cleanup(runtime_dir):
    """Stops the session's leftovers, unmounts what they mounted in its
    runtime dir, then removes the dir. With a mount still there, the dir
    stays: deleting through a portal mount would delete the files it shows."""
    pids = _processes_with(runtime_dir)
    for pid in pids:
        try:
            os.kill(pid, signal.SIGTERM)
        except OSError:
            pass
    deadline = time.monotonic() + 3.0
    while pids and time.monotonic() < deadline:
        pids = [pid for pid in pids if os.path.exists(f"/proc/{pid}")]
        time.sleep(0.1)
    for pid in pids:
        try:
            os.kill(pid, signal.SIGKILL)
        except OSError:
            pass
    for point in _mounts_under(runtime_dir):
        for tool in ("fusermount3", "fusermount"):
            if shutil.which(tool):
                subprocess.run([tool, "-u", "-z", point], capture_output=True)
                break
    left = _mounts_under(runtime_dir)
    if left:
        print(f"private_session: {left} still mounted; {runtime_dir} kept", file=sys.stderr)
        return
    if os.path.basename(runtime_dir).startswith(PREFIX):
        shutil.rmtree(runtime_dir, ignore_errors=True)


def run(cmd, env=None, **kwargs):
    """subprocess.run(cmd) in a session of its own (see the module doc):
    the outer display and bus are dropped, and XDG_RUNTIME_DIR is private."""
    env = dict(os.environ if env is None else env)
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "DBUS_SESSION_BUS_ADDRESS"):
        env.pop(name, None)
    env["GDK_DEBUG"] = ",".join(filter(None, [env.get("GDK_DEBUG", ""), "no-portals"]))
    runtime_dir = tempfile.mkdtemp(prefix=PREFIX)  # 0700, ours
    env["XDG_RUNTIME_DIR"] = runtime_dir
    try:
        return subprocess.run(cmd, env=env, **kwargs)
    finally:
        cleanup(runtime_dir)


def main(argv):
    if "--" not in argv:
        print(__doc__.strip().splitlines()[-1].strip(), file=sys.stderr)
        return 2
    split = argv.index("--")
    options, cmd = argv[:split], argv[split + 1:]
    if not cmd:
        return 2
    if "--dbus" in options:
        cmd = ["dbus-run-session", "--"] + cmd
    return run(cmd).returncode


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
