# Linux printing: validation

Philip ranked it "Lowest priority" on 2026-10-15. It was the last item on
his list.

## Before

On Linux, Print did nothing: `PrintDlgW` is a compat stub that returns
FALSE, so `printDocument` stopped at the printer picker. PDF export already
worked, because the compat `ID2D1PrintControl` writes the pages Tinta
renders into a Cairo PDF surface.

## Design

`printDocument` (`print.cpp`, `#ifndef _WIN32`):

1. Exports the document with `exportPdfFile` to a temporary PDF
   (`tinta-print-<pid>-<n>.pdf` in the temp folder). The paper and
   orientation are the print preview's, and edits not yet saved are
   included, as in the preview.
2. Hands the file to `tintaPrintPdf`, which calls GTK 4.14+'s
   `gtk_print_dialog_print_file`.
   - The system dialog chooses the printer, copies and pages.
   - In a sandbox the print portal shows it, so the Flatpak needs no CUPS
     access.
   - It returns at once. When the job has the file, or the dialog is
     dismissed, the file is deleted.

Print and PDF therefore come out of the same pages, so they can't drift
apart. `GtkPrintOperation`, the planned route, would have needed a second
drawing path through GTK's print context.

## Checked

`linux_print` (`tests/linux_render/print_check.py`, ctest when
`TINTA_GUI_PYTHON` is set) runs Ctrl+P then Enter on
`tests/fixtures/paste-image.md`, on a private D-Bus session and Xvfb. It
uses GTK's in-process dialog (`GDK_DEBUG=no-portals`), and HOME and
Documents point into the output directory. 3/3 pass:

| Check | Result |
|---|---|
| dialog | the GTK print dialog opens, 625 × 516 |
| printed | "Print to File" writes `Documents/output.pdf`: 2 pages, like `--exportpdf` of the same document; no temporary PDF left |
| cancel | Cancel closes the dialog ("Dismissed by user") and removes the temporary PDF |

- **Page 1 of the printed PDF** (`pdftoppm`) is Tinta's print layout:
  Letter, the print palette, the table, list, quote and pasted image.
- **Mutation:** skipping the dialog fails all 3 checks.
- **Found on the way:** with no Documents folder, "Print to File" defaults
  to the working directory, which is `/` for Tinta, and fails with
  "Permission denied". A desktop session has `~/Documents`. The check
  creates one.

## Live on KDE Plasma Wayland (2026-10-16)

- **Outside a sandbox, GTK took the print portal anyway, and KDE's portal
  failed** ("Operation failed", with no dialog shown).
  - Fix: unsandboxed builds use GTK's own dialog and a `GtkPrintJob` over
    CUPS (`printWithUnixDialog`).
  - Live, it opened with the exported PDF ready (Philip's CUPS printer is
    listed). Esc cancelled it and removed the temporary file. Nothing was
    printed: the check only ever cancels.
- **In the Flatpak the portal is the only way out, and it failed the same
  way.** `dbus-monitor` showed why: given no setup, GTK calls the portal's
  `Print` directly without `PreparePrint`, and KDE's portal answers 2
  (error) within a millisecond. The Flatpak path now calls
  `gtk_print_dialog_setup` first (PreparePrint shows KDE's dialog), then
  prints with that setup.

## Not checked

- Printing on paper: every check cancels or prints to a file. The CUPS
  dialog and the Flatpak's portal dialog were both opened live and
  cancelled.
- A Tinta process killed while its dialog is open leaves its temporary PDF
  behind.
