# The application menu button's icon on Linux: validation

2026-10-23. Philip: the application menu at the top left highlights on
hover, but shows no icon, so nothing hints that it is there.

## Cause

- **The design:** the menu button is the app icon, as in a Windows caption.
  `src/tabs.cpp` loads resource `IDI_ICON1` with `LoadImageW`, converts it
  with `CreateBitmapFromHICON`, and draws it at 16 DIP in the 40 DIP cell.
- **Windows:** the exe carries the icon (`llvm-readobj --coff-resources`
  lists `IDI_ICON1` as `GROUP_ICON`).
- **Linux:** the compat layer stubbed both calls, so the bitmap stayed null
  and only the hover highlight drew.

## Fix

- **`LoadImageW`** (`platform/linux/src/win32_window.cpp`) returns the app
  icon for `IDI_ICON1` as an `HICON` holding its pixels. It uses the smallest
  installed size that covers the request. The PNG comes from:
  - the hicolor copy beside the binary (`../share/icons/hicolor/NxN/apps/tinta.png`,
    as the AppImage, Flatpak and install prefixes lay it out);
  - or, from a build, the source tree's `platform/linux/data/icons/tinta-N.png`.
- **`CreateBitmapFromHICON`** (`wic_imaging.cpp`) wraps those pixels.
- **`DestroyIcon`** frees them.
- Other icon names still return null.

## Checked

- **Regression test:** `compat_d2d_wic` (`testAppIcon`) follows the path
  `tabs.cpp` takes. It checks that:
  - an unknown name loads nothing;
  - `IDI_ICON1` loads at 32x32 and converts to PBGRA;
  - it becomes a D2D bitmap, and drawn on white it marks 100 or more of the
    1024 pixels.

  The old stubs fail its first check.
- **Full suite:** ctest 65/65.
- **By eye:** `gui_check.py` under Xvfb (window at 1000x640), then the
  headless renderer for themes and scale.
  - The ink drop shows in the menu cell in Paper, Dusk, Midnight, Dracula,
    Abyss and Wombat, and at 2x.
  - In light themes it is clear. In dark themes the dark-navy artwork shows
    mainly through its light outline: visible, but faint. It is the same
    artwork as on Windows.

## Not covered

- **The Windows caption by eye:** under Wine without a window manager, the
  window's top strip falls off-screen. The Windows path is unchanged, and
  calls the real `LoadImageW` on a resource the exe carries.
