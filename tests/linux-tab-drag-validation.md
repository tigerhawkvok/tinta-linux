# Linux: tab tear-off and tabs dragged between windows: validation

Approved by Philip on 2026-10-15 ("Approved").

## Why it needed its own design

On Windows, a tab pulled clear of the strip follows the cursor as a layered
ghost popup. On release:

- `WindowFromPoint` finds a Tinta window under the cursor, and the tab moves
  there through `WM_COPYDATA`;
- anywhere else, `ShellExecute(tinta --cascade --tabbed --pos x y)` opens it
  in a new window at the drop point.

None of that carries to Wayland, where no client knows where its windows
or the pointer are, or can place a new window. The compat layer's versions
answered only for their own process. The GDI ghost never drew.

## Design: the desktop's drag and drop

- **Shared code** (`tabs.cpp`, `#ifndef _WIN32`): when a detachable tab first
  leaves the strip, it calls `tintaBeginTabDrag` (Linux `windows.h`) instead
  of drawing a ghost. Tinta's own drag ends there, because the press now
  belongs to the desktop. The outcome comes back as `WM_APP_TAB_DRAG_DONE`,
  and `tabDragDone` finds the tab again by its path:
  - **MOVED:** the tab closes locally, and the window too if it was its only
    tab, as on Windows;
  - **NO_TARGET:** with more than one tab, it tears off into
    `tinta --cascade --tabbed path` (`--cascade` skips the single-instance
    hand-off; the desktop places the window);
  - **CANCELLED:** nothing.
- **The drag** (`gtk_window.cpp`): `gdk_drag_begin` with an
  `application/x-tinta-tab` payload (the path and the source pid).
  - The icon is the tab card drawn with Cairo and Pango at the window's
    scale: strip colour, border, title ellipsized.
  - `drop-performed` and `dnd-finished` report MOVED.
  - Cancelled with no target reports NO_TARGET, without the fly-back
    animation; any other cancel reports CANCELLED.
- **Every Tinta window is a drop target** (`GtkDropTargetAsync`).
  - It reads the payload asynchronously. A blocking read deadlocked: on X11
    the bytes arrive through the same loop's events.
  - It hands the tab to the window as `tabSendDrop` does on Windows
    (`WM_COPYDATA`, the drop point as a screen point, so the insertion slot
    follows the pointer), then presents the window.
- **The source window** takes its own tab back only over its strip. The tab
  stays put, standing in for Windows' "return to the strip and it
  re-attaches". Released anywhere else on its own window, the tab tears
  off, as on Windows.

## Checked

`linux_tab_drag` (`tests/linux_render/tab_drag_check.py`, ctest when
`TINTA_GUI_PYTHON` is set) runs two Tinta processes on a private D-Bus
session and Xvfb, with XTest input and XDND. 5/5 checks pass:

| Check | What happens |
|---|---|
| moved | beta, dropped on B's strip, opens there (active, at the drop slot) and leaves A |
| torn-off | released over the empty screen: a third window with beta |
| own-window | delta, released over A's own document: a window of its own |
| returned | epsilon, dropped back on A's strip: no window comes or goes, and A keeps both tabs (screenshot) |
| last-tab | a window's only tab dropped on another window: that window takes it and the empty one closes |

- **Mutation:** with the drop target refusing everything, moved, returned
  and last-tab fail (3 checks); the tear-offs still pass, as they should.
- **Found and fixed on the way:** the synchronous payload read deadlocked
  (the drop showed `reading DND as application/x-tinta-tab now` and never
  finished).
- ctest 57/57. The Windows exe builds; its tab drag is unchanged, and the
  new message is a no-op there.

## Live on KDE Plasma Wayland (2026-10-16)

Philip let me drive his session while he was away.
`tests/linux_render/desktop_tab_drag_check.py` uses uinput input, KWin
scripting and spectacle. It guards every press so it lands in the active
Tinta window, and it kills every Tinta it started. 6/6 passed:

| Check | Result |
|---|---|
| backend | GdkWaylandDisplay |
| moved | beta, dropped on another window's strip, opens there |
| torn-off | released over another application: a new window |
| own-window | released over its own document: a new window |
| returned | dropped back on its own strip: nothing moves |
| print-dialog | see `linux-print-validation.md` |

The first live runs found three things the Xvfb runs couldn't:

- **A tab could vanish into another application.** Creality Print, which
  is Chromium-based, accepted the drop and reported a move, so Tinta closed
  the tab, and it opened nowhere.
  - Fix: the payload carries a receipt path in `XDG_RUNTIME_DIR`, and only
    a Tinta window that took the tab creates the file. The source treats the
    tab as moved only with the receipt; without one, the tab tears off.
  - The source polls for the receipt for up to 2 s after the drop is
    performed. It doesn't wait for the other application's "finished",
    which Creality Print sent many seconds late.
- **No tear-off on Wayland.** GDK reports a drop on nothing as
  `GDK_DRAG_CANCEL_ERROR`, not `NO_TARGET`. That reason now tears off,
  unless this process saw Esc during the drag.
- **Esc can't be told apart.** On KWin the source gets no key events during
  a drag, so Esc and a drop on nothing arrive alike, and the tab tears off.
  It is not lost: dropping the new window's tab back moves it home. Under
  XDND, where the source does see keys, Esc is swallowed and keeps the tab.

## Not checked

- **Esc during a drag.** Under Xvfb with no window manager, an XTest Esc
  went to the focused window (where Tinta's Esc closes it), not to the drag.
  On a real desktop the drag protocol owns Esc, and the CANCELLED path
  changes nothing.
- **The card's rounded corners** draw black under Xvfb: there is no
  compositor, so ARGB windows can't be transparent. A Wayland compositor
  blends them.
