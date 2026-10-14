#!/usr/bin/env python3
"""Render Markdown fixtures with the headless Linux build.

Each document opens in its own Tinta process with a private configuration
folder (XDG_CONFIG_HOME, XDG_CACHE_HOME), so user settings never leak in,
and the window is saved as a PNG once startup settles. Output goes to
<build>/linux-render/ (ignored by git).

  render_fixtures.py BUILD_DIR [fixture.md ...] [--size WxH] [--scale N]
                     [--theme NAME] [--out DIR]

Without fixtures, every tests/fixtures/*.md is rendered. Prints one line
per document with the exit code, the settle time Tinta reported and the
PNG size, and exits non-zero if any render failed.
"""
import argparse
import os
import shutil
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


# Built-in themes in THEMES order (src/themes.cpp)
THEMES = ["Paper", "Sakura", "Arctic", "Meadow", "Dusk", "Midnight", "Dracula", "Forest", "Ember", "Abyss"]


def custom_theme_names(themes_ini):
    names = []
    if themes_ini:
        for line in open(themes_ini, encoding="utf-8"):
            if line.strip().lower().startswith("name="):
                names.append(line.split("=", 1)[1].strip())
    return names


# Custom themes (themes.ini) follow the built-ins in index order
def settings_ini(theme, themes_ini=""):
    names = THEMES + custom_theme_names(themes_ini)
    lines = ["checkUpdates=0", "hasAskedFileAssociation=1", "followSystemTheme=0", "openInTabs=1"]
    lines.append(f"themeIndex={names.index(theme) if theme else 0}")
    return "\n".join(lines) + "\n"


def export_pdf(tinta, doc, out_pdf, theme, themes_ini=""):
    """Tinta's own --exportpdf path; returns (exit code, wall ms, stderr)."""
    home = os.path.join(os.path.dirname(out_pdf), ".home", os.path.basename(out_pdf))
    env = isolated_env(home, theme, themes_ini)
    start = time.monotonic()
    proc = subprocess.run([tinta, doc, "--exportpdf", out_pdf], env=env, cwd=home, capture_output=True, text=True,
                          timeout=120)
    return proc.returncode, (time.monotonic() - start) * 1000, proc.stderr.strip()


def isolated_env(home, theme, themes_ini):
    shutil.rmtree(home, ignore_errors=True)
    config = os.path.join(home, "config", "Tinta")
    os.makedirs(config)
    with open(os.path.join(config, "settings.ini"), "w") as f:
        f.write(settings_ini(theme, themes_ini))
    if themes_ini:
        shutil.copyfile(themes_ini, os.path.join(config, "themes.ini"))
    env = dict(os.environ)
    env["XDG_CONFIG_HOME"] = os.path.join(home, "config")
    env["XDG_CACHE_HOME"] = os.path.join(home, "cache")
    env["XDG_DATA_HOME"] = os.path.join(home, "data")
    return env


def render(tinta, doc, out_png, size, scale, theme, settle_ms, trace=False, repaint=0, env_extra=None,
           themes_ini=""):
    home = os.path.join(os.path.dirname(out_png), ".home", os.path.splitext(os.path.basename(out_png))[0])
    env = isolated_env(home, theme, themes_ini)
    if trace:
        env["TINTA_TRACE_WIC"] = "1"
    env.update(env_extra or {})
    args = [tinta, doc, f"--render-png={out_png}", f"--size={size}", f"--scale={scale}",
            f"--settle-ms={settle_ms}", f"--repaint={repaint}"]
    start = time.monotonic()
    proc = subprocess.run(args, env=env, cwd=home, capture_output=True, text=True, timeout=120)
    wall = (time.monotonic() - start) * 1000
    return proc.returncode, wall, proc.stderr.strip()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("build")
    parser.add_argument("docs", nargs="*")
    parser.add_argument("--size", default="1000x1400")
    parser.add_argument("--scale", default="1")
    parser.add_argument("--theme", default="")
    parser.add_argument("--settle-ms", default="400")
    parser.add_argument("--out", default="")
    parser.add_argument("--trace", action="store_true", help="log image decodes, print all stderr")
    parser.add_argument("--repaint", type=int, default=0, help="time N repaints after settling")
    parser.add_argument("--env", action="append", default=[], help="NAME=VALUE for Tinta")
    parser.add_argument("--themes-ini", default="", help="custom themes; --theme may name one")
    parser.add_argument("--pdf", action="store_true", help="also export each document with --exportpdf")
    args = parser.parse_args()

    tinta = os.path.join(os.path.abspath(args.build), "tinta")
    out_dir = os.path.abspath(args.out or os.path.join(args.build, "linux-render"))
    os.makedirs(out_dir, exist_ok=True)
    docs = args.docs or sorted(
        os.path.join(REPO, "tests", "fixtures", f)
        for f in os.listdir(os.path.join(REPO, "tests", "fixtures")) if f.endswith(".md"))

    failures = 0
    for doc in docs:
        name = os.path.splitext(os.path.basename(doc))[0]
        suffix = f"-{args.theme.replace(' ', '_')}" if args.theme else ""
        suffix += f"-{args.size}" + (f"@{args.scale}x" if args.scale != "1" else "")
        out_png = os.path.join(out_dir, f"{name}{suffix}.png")
        if os.path.exists(out_png):
            os.remove(out_png)
        code, wall, stderr = render(tinta, os.path.abspath(doc), out_png, args.size, args.scale, args.theme,
                                    args.settle_ms, args.trace, args.repaint,
                                    dict(e.split("=", 1) for e in args.env), os.path.abspath(args.themes_ini)
                                    if args.themes_ini else "")
        ok = code == 0 and os.path.exists(out_png)
        failures += not ok
        settle = next((l for l in stderr.splitlines() if l.startswith("render-png: settled")), "")
        frames = next((l for l in stderr.splitlines() if "repaints" in l), "")
        if frames:
            settle += "; " + frames.replace("render-png: ", "")
        other = [l for l in stderr.splitlines() if not l.startswith("render-png:")]
        size = os.path.getsize(out_png) if os.path.exists(out_png) else 0
        print(f"{'ok  ' if ok else 'FAIL'} {name}: exit {code}, wall {wall:.0f} ms, "
              f"{settle.replace('render-png: ', '')}, {size} bytes")
        for line in other if args.trace else other[:5]:
            print(f"     {line}")
        if args.pdf:
            out_pdf = os.path.join(out_dir, f"{name}{suffix}.pdf")
            if os.path.exists(out_pdf):
                os.remove(out_pdf)
            code, wall, stderr = export_pdf(tinta, os.path.abspath(doc), out_pdf, args.theme,
                                            os.path.abspath(args.themes_ini) if args.themes_ini else "")
            head = open(out_pdf, "rb").read(5) if os.path.exists(out_pdf) else b""
            pdf_ok = code == 0 and head == b"%PDF-"
            failures += not pdf_ok
            size = os.path.getsize(out_pdf) if os.path.exists(out_pdf) else 0
            print(f"{'ok  ' if pdf_ok else 'FAIL'} {name}.pdf: exit {code}, wall {wall:.0f} ms, {size} bytes")
    print(f"{len(docs) - failures}/{len(docs)} rendered into {out_dir}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
