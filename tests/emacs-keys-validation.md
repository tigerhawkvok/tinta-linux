# Emacs shortcut profile: validation

Requested 2026-10-15 ("let's also add Emacs style"). Philip chose the
deepest of three options: Emacs chords in the reader **and** the source
editor, not just a renamed set of single keys like the vim profile.

## Design

- **The profile.** `emacs` joins `windows` and `vim` in `KEY_PROFILES`.
  Its single keys are the Windows ones (`F`, `J`, `K`, `E`…); its chords
  live in `src/emacs_keys.cpp`.
- **Where keys enter.** `handleKeyDown` asks `emacsKeyDown` first (Ctrl
  chords and the `C-x` prefix). The window procedure asks
  `emacsSysKeyDown` on `WM_SYSKEYDOWN` (Meta chords).
- **Re-sending.** Most chords are re-sent as the Windows key they stand
  for, with exactly the modifiers it needs, through `handleKeyDown`. Every
  caret, the search bar, the preview, paste-as-table and undo then behave
  as they already do. While the mark is set, motions are re-sent with
  Shift.
  - `emacsDispatching` keeps a re-sent key from being read as Emacs again.
  - Each modifier gets its real state back afterwards, unless a modal loop
    ran meanwhile.
  - Commands that open dialogs (open, save as) are called directly, not
    re-sent.
- **Emacs's own commands**, in `editor.cpp`:
  - kills (`C-k`, `M-d`, `M-DEL`): the clipboard is the kill ring's one
    slot, and a kill made where the previous one left the caret, with
    nothing typed since, appends (or prepends, backwards);
  - the mark, `C-x C-x`, `C-t` (one undo step), `C-o`.
- **`C-g` is keyboard-quit.** It closes the search, a panel, a menu or a
  dialog, or drops the region. Unlike Esc it never closes the window or
  leaves the editor.
- **`C-x` waits for the next key**, with a `C-x-` hint. An unknown
  follow-up is reported ("C-x z is undefined"), not typed.
- **Guards.**
  - Ctrl+Alt (AltGr) is never a chord.
  - A handled Meta chord's `WM_SYSCHAR` is swallowed, so Windows does not
    beep.
  - Dialogs, menus and text fields keep their keys; only `C-g` reaches
    them, as Esc.
- **Windows meanings kept.** Chords Emacs leaves undefined keep them:
  Ctrl+C, Ctrl+Z, Ctrl+T, Ctrl+Tab, Ctrl+1…9, Ctrl+, and the rest.
- **What Emacs takes over**, editor and reader:
  - Ctrl+A select all, Ctrl+N new note, Ctrl+P print, Ctrl+F find, Ctrl+V
    paste, Ctrl+W close tab and word wrap, Ctrl+D next occurrence, Ctrl+E
    preview, Ctrl+H replace.
  - Where each now lives: `C-x h`, `C-x C-f`, `C-s`, `C-y`, `C-x k`, `M-%`,
    the Settings toggle for word wrap, and the context menu for print.

## Bindings

| | Reader | Source editor |
|---|---|---|
| `C-n` `C-p` | line down / up | next / previous line |
| `C-f` `C-b` | scroll sideways | character forward / back |
| `M-f` `M-b` | | word forward / back |
| `C-a` `C-e` | (Windows) | line start / end |
| `C-v` `M-v` | page down / up | page down / up |
| `M-<` `M->` | top / bottom | buffer start / end |
| `C-s` `C-r` | search; then next / previous match | the same |
| `M-%` | replace (opens the editor) | replace |
| `C-SPC` `C-@` | | set the mark |
| `C-w` `M-w` | copy the selection (read-only) | kill / copy the region |
| `C-y` | | yank |
| `C-k` `M-d` `M-DEL` | | kill line / word / word back |
| `C-d` `C-t` `C-o` `C-j` | | delete, transpose, open line, newline |
| `C-/` `C-_` `C-?` | | undo, undo, redo |
| `C-h` | help | help |
| `C-g` | quit | quit |
| `C-x C-f` `C-s` `C-w` | open, save, save as | the same |
| `C-x C-q` | edit | back to reading |
| `C-x C-c` | close the window | the same (unsaved tabs ask) |
| `C-x k` `b` `←` `→` | close tab, next / previous tab | the same |
| `C-x h` `u` `C-x` | select all | select all, undo, swap point and mark |

## UI

- Settings → Keyboard shortcuts lists Windows, Vim, **Emacs**, Custom.
  The hint reads "Windows, vim or Emacs style, or your own bindings" in all
  seven languages.
- Help (`C-h`), in the Emacs profile:
  - shows the Emacs chords beside the single keys;
  - drops the rows whose Windows chord Emacs takes over;
  - adds an EMACS section;
  - widens the key column.
- The echo hints are localized: "Quit", "Mark set", "… is undefined".

## Checked

- **`emacs_keys`** (`tests/emacs_keys_tests.cpp`, ctest): the real
  handlers, with modifier state set as a keyboard sets it.
  - **Reader:** C-n, C-p, C-v, M-v, M->, M-<; C-s opens the search and C-g
    closes it; C-x h then M-w copies the document; C-x z is consumed.
  - **C-g never closes the window.** The test first shows that a bare Esc
    does post `WM_CLOSE`, so the check itself works.
  - **Editor motions:** C-f, C-b, C-e, C-a, C-n, C-p, M-f, M-b.
  - **The mark:** C-SPC with C-f grows a region; M-w copies it and drops
    the mark; C-w kills; C-y yanks; C-x C-x swaps; C-g clears the region
    and stays in the editor.
  - **Kills:** C-k twice gathers "beta gamma\n"; C-/ undoes both; M-d;
    M-DEL twice prepends; C-x u.
  - **Small edits:** C-d; C-t, with a one-step undo; C-o; C-x h.
  - **Search and modes:** C-s and C-g in the editor; C-x C-q into and out
    of the editor.
  - **Guards:** AltGr is left alone, and the windows profile ignores the
    chords.
- **Mutations**, each failing the test:
  - the dispatcher switched off: the reader motions fail;
  - motions ignoring the mark: 2 checks;
  - kills never appending: 2 checks;
  - C-g acting as Esc: the window-close check.
- **Linux GUI** under Xvfb (own D-Bus session, X11):
  - Help with the Emacs labels;
  - the `C-x-` hint, then "C-x z is undefined";
  - the shortcuts dropdown listing four profiles with Emacs selected.
- **Windows exe under Wine:**
  - C-h opens Help and C-g closes it ("Quit");
  - M-> (`WM_SYSKEYDOWN`) reaches the end;
  - C-x C-q enters the editor;
  - C-e then typing lands at the end of the logical line, with no stray
    control character.
- **Fixture:** `tests/fixtures/emacs-keys.md`, a walkthrough of the chords
  over a table, lists, a quote and code.
- ctest 50/50.

## Not done

- Not implemented: the full kill ring (`M-y`), prefix arguments (`C-u`),
  `M-x`, case commands (`M-u`, `M-l`, `M-c`), `C-l`, rectangles, and the
  ESC-as-Meta prefix.
- The reader has no point: `C-a`/`C-e` keep their Windows meaning there,
  and `C-w` copies, as a kill in a read-only buffer does.
