# Editor search with word wrap (#260)

Validated on Windows with the Release build on 2026-10-16.

## Report

Searching in the editor with word wrap on could leave the current match
offscreen. The reporter's document has long paragraphs and a table. Searching
it for `latin`, match 5 of 12 was not visible, while every match showed with
word wrap off.

## Cause

Stepping to a match scrolled the editor to `line × line height`, the match's
logical line, as if no line wrapped. With word wrap on, each long line above
the match takes several rows, so the view stopped short of the match by
those extra rows. A match deep in a long paragraph sat further down still,
on a later row of its own line. The editor's caret-following code already
counted wrapped rows, but the search step did not use it.

## Change

- The caret-following code's row calculation is now a helper: the wrapped
  row a line starts on, plus the caret's row inside that line's layout.
  Unwrapped text keeps `line × line height`.
- Stepping to a match uses it to centre the match's row in the editor below
  the tab strip and toolbar (it used the whole window height), clamped to
  the document.
- It then runs the caret-following code, so with wrap off a match far along
  a long line also scrolls into view sideways.

## Fixtures and automated checks

`fixtures/search-word-wrap-260.md` puts `latin` deep inside long paragraphs
(17 matches), with a table cell, a list item, a quote, a Chinese paragraph and
a long code line among them.

`wrapped_cursor_navigation` now also opens the fixture in the editor in Paper
and Midnight. It runs at 1x and 1.5x, 650 and 1050 px, with the preview on
and off, and word wrap on and off. It opens the search, types `latin`, and
steps through every match with Enter. After each step it checks that the
caret, which sits on the match, is inside the source pane vertically and
horizontally.

With the old scrolling restored, the test fails 449 checks. The full suite
passed 36 of 36 after a clean rebuild.

## Desktop check

The reporter's file was opened in the editor with word wrap on, at
1000 px in Paper, in the master build (c58981a) and in this one. The script
pressed Ctrl+F, typed `latin` and pressed Enter (`out/issue-260b/`, ignored).

- Master: at 6 of 12 the view stayed near the top, with only matches 1 to 3
  on screen.
- This build: every step from 2 to 6 of 12 shows the current match, centred.

The new fixture at 650 px in Midnight shows match 10 of 17, inside the long
table row, in view.

## Limits

- A match centres in the editor pane. With the docked preview on, the preview
  keeps following the caret by its own rules, which this change leaves alone.

## Draft reply (not posted)

### #260

Thanks for the clear report and the file, Derek. With word wrap on, stepping
to a match scrolled to the match's line number as if no line wrapped. Every
long line above it pushed the match further below the view. The next release
counts the wrapped rows and centres the current match. Your document now keeps
every `latin` match on screen, in tables and long paragraphs too.
