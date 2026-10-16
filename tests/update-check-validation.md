# Update check on demand, and OS-neutral wording: validation

Philip, on the 4.0.2 Windows build (2026-10-15):
- "I couldn't force an update check".
- "UI refers to 'Windows theme' instead of agnostic 'OS'".

## Check now

The automatic check runs at most once a day (`lastUpdateCheck`). His 4.0.1
had checked before 4.0.2 was released, so it would not look again until
the next day, and nothing could make it.

What changed:
- **Settings > General > Updates** (new, last card):
  - the daily check's toggle (`checkUpdates`, which was settings.ini only);
  - a **Check now** chip. It posts `WM_APP_UPDATE_CHECK_NOW` and starts a
    worker whatever the day; the chip stays lit while the worker is out.
- A manual check answers every outcome:
  - a newer release brings the update chip back, even for a version
    dismissed before;
  - otherwise a success chip says "Tinta X.Y.Z is the latest version";
  - a failure says "Couldn't check for updates".
- The automatic check is unchanged.
- **Linux:** WinHTTP was stubbed, so every check failed, by design while it
  asked upstream's Windows releases. The fork's releases carry the
  AppImage too, so the compat layer now implements the six WinHTTP calls
  Tinta uses over libcurl: one HTTPS GET, read back from memory.
- The settings panel grew from 540 to 590 px, so seven General cards
  clear the footer.

Checked (one-off drivers in the job scratch space, not in ctest: they
click by XTEST and the success case needs the network):
- **Linux, `build/tinta` under Xvfb** with its own D-Bus session and
  settings, `checkUpdates=0`: Ctrl+, then click Check now.
  - The card renders.
  - Within 0.4 s: "✓ Tinta 4.0.2 is the latest version" (a real request
    to the fork's `releases/latest`).
  - With `https_proxy` pointing at a dead port: "⚠ Couldn't check for
    updates".
- **Windows exe under Wine** (real WinHTTP): the same click answers
  "Tinta 4.0.2 is the latest version".
  - Wine's client area is offset from the toplevel, so the click
    coordinates were measured from its screenshot.
- Not exercised: the "newer release" branch. It needs a release newer
  than the running build; its only change is that a manual check ignores a
  dismissed version.

## Wording

| Before | After | Where |
|---|---|---|
| Follow Windows theme | Follow system theme | Settings and the theme chooser, all seven languages |
| Auto follows the Windows display language | Auto follows the system display language | Settings; true on Linux too (`LANGUAGE`/`LANG`) |
| Reveal in Explorer | Show in folder | Context menus |
| Files opened from Explorer join this window… | Files opened from your file manager join this window… | Settings |

- The README, the `copy-file-path` fixture and a test message follow the
  new label.
- Still says Windows, deliberately:
  - the default-viewer prompt ("Windows will open Settings…"), a
    Windows-only flow;
  - the shortcut profile named "Windows" (Ctrl-based bindings, beside vim).

ctest 49/49.
