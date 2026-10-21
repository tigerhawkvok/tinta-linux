# Code block copy without a trailing line break (#269)

Validated on Windows with the Release build on 2026-10-21.

## Problem

A code block's Copy button put the block's text on the clipboard with the
line break that ends its last line, so a one-line block pasted as a line
plus an empty line. md4c ends every code block's text with that break.

## Change

- The Copy button copies the block without the break after its last line
  (`codeCopyText`, `src/utils.cpp`), as GitHub's copy button does. A blank
  line written before the closing fence is content and stays, as do
  trailing spaces and fences nested in a longer fence.
- The block's stored text is unchanged, so its height, search, selection
  and the diagram source are unaffected.
- The "Copied" pill now shows only when something was copied. An empty
  block, or a clipboard another program holds at that moment, no longer
  claims a copy that did not happen.

## Automated checks

`code_block_layout` (updated):

- Every existing layout case states its copied text: LF and CRLF fences, a
  closing fence at end of file, an unclosed fence, raw `.txt` and `.json`
  files, an intentional trailing blank line, empty blocks and an invalid
  Mermaid fence.
- New: `tests/fixtures/code-block-copy-269.md` is laid out in two themes, at
  100 % and 150 % and at 1050 and 650 px. Each of its seven blocks (one
  line, several lines, a kept blank line, a tilde fence, a block in a list,
  one in a quote, and a four-backtick fence holding a fenced block with
  trailing spaces) is clicked through the real mouse handlers in a hidden
  window, and the clipboard is read back.

Reverting `codeCopyText` fails the copied-text check in every layout case.
Reverting only the click handler's use of it fails the click test for the
blocks in all eight configurations.

Remote Desktop's clipboard sync can hold the clipboard for a moment after
each change; the click test repeats a click whose copy lost that race,
which would otherwise read as a stale clipboard rather than a wrong one.

## Limits

- Copying a selection with Ctrl+C is unchanged: it copies what is selected.
- Indented code blocks are off in Tinta (`MD_FLAG_NOINDENTEDCODEBLOCKS`),
  so the fixture has none.
