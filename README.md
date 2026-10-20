<div align="center">
  <img src="resources/tinta.ico" width="80">
  <h1>Tinta Mono</h1>
  <p><em>Markdown, distilled. On Windows and Linux, tuned for people who write code.</em></p>
  <p>A fast, native Markdown and Mermaid viewer and editor</p>

  <a href="https://github.com/tigerhawkvok/tinta-linux/releases/latest">
    <img src="https://img.shields.io/github/v/release/tigerhawkvok/tinta-linux?label=Download&style=for-the-badge&color=1a1a2e" alt="Download">
  </a>
  <a href="LICENSE">
    <img src="https://img.shields.io/badge/license-MIT-blue?style=for-the-badge" alt="MIT License">
  </a>
</div>

<br>

**Tinta Mono** is a fast, native Markdown and Mermaid viewer and editor for **Windows and
Linux**. It opens instantly: no Electron and no web engine, just Direct2D/DirectWrite on Windows
and GTK 4, Cairo and Pango on Linux.

## Lineage

Tinta Mono is a hard fork of **[Tinta](https://github.com/oipoistar/tinta)** by
[oipoistar](https://github.com/oipoistar), the Windows Markdown viewer at
[tinta.cc](https://tinta.cc). Everything that makes Tinta fast and pleasant comes from there: the
Direct2D renderer, the tabs and unified editor, annotations, the exports, and the themes. If you
only need Windows, upstream Tinta is signed and in the
[Microsoft Store](https://apps.microsoft.com/detail/9MZ5MZ3L9RKF).

The fork adds:

- **Linux**:
  - a compatibility layer that runs the same code on GTK 4, Cairo and Pango, natively on Wayland
    and X11;
  - an AppImage and a Flatpak;
  - single instance, tabs that tear off and move between windows, and printing through the system
    dialog.
- **Mermaid through [merman](https://github.com/Latias94/merman)**:
  - the Rust port of Mermaid draws every diagram family Mermaid's docs show, math labels included;
  - labels are sized in the face that draws them;
  - search reaches the diagram's source.
- **Programmer-friendly choices**:
  - Monaspace Argon as the default code face, bundled on both platforms;
  - Windows, Vim and Emacs keyboard profiles;
  - copying from the reader gives you the Markdown behind the selection;
  - pasted screenshots go to an assets folder or inline.

The name keeps "Tinta" for where it comes from, and "Mono" for the monospace and code-minded
defaults. Settings stay where Tinta keeps them (`%APPDATA%\Tinta`, or `~/.config/Tinta` on Linux),
so moving between the two keeps your setup.

## Install

Download from [the latest release](https://github.com/tigerhawkvok/tinta-linux/releases/latest):

| File | Platform |
|---|---|
| `tinta-mono-<version>-windows-x64.exe` | Windows 10/11, x64: a portable single exe. Put a `settings.ini` beside it to keep settings there. It is unsigned, so SmartScreen asks once ("More info", then "Run anyway"). |
| `Tinta-Mono-<version>-x86_64.AppImage` | Linux x86_64 (glibc 2.38 or newer): `chmod +x` and run; `--register` adds the desktop entry and file types. |

The Flatpak builds from this tree with `packaging/flatpak/build-flatpak.sh`.

## Why native?

Most markdown apps ship an entire browser to render text. Tinta uses the GPU-accelerated text stack already built into Windows:

|  | Tinta | Typora | Obsidian | VS Code |
|---|---|---|---|---|
| Startup | **<100 ms** | ~1.5 s | ~3 s | ~2 s |
| Install size | **~2.5 MB** | ~90 MB | ~250 MB | ~350 MB |
| Runtime | **Native Direct2D** | Electron | Electron | Electron |

It's a viewer first: perfect as the double-click default for `.md` and `.mmd` files, for reading documentation and diagrams — with an edit mode when you need it.

## Features

- **Lightning-fast startup** - Direct2D rendering, no web engine overhead
- **10 beautiful themes** - 5 light and 5 dark themes to choose from
- **Hardware-accelerated** - Smooth text rendering via DirectWrite
- **Word wrap** - Optional soft wrap in the editor (Ctrl+W)
- **Native LaTeX math** - `$inline$` and `$$display$$` equations with matrices, cases, aligned equations, fractions, limits, accents and custom macros. Uses an installed math font; no bundled fonts or web engine. See the [supported math syntax](docs/math-support.md).
- **Focused editing** - Hide the preview pane while writing (Ctrl+E)
- **Native Mermaid diagrams** - 22 diagram families, from flowcharts with subgraphs through sequence, class, state, ER, gantt, and pie to C4, sankey, kanban, and radar - rendered without a web engine
- **Export as HTML, DOCX, or PDF** - Right-click and pick "Export as..." for a self-contained web page, a Word document that opens natively, or a vector PDF; diagrams and math come along in every format
- **Pandoc export** - With [pandoc](https://pandoc.org) installed (or pointed at in settings), the editor offers EPUB, OpenDocument, PowerPoint, RTF and LaTeX too; rich formats convert from Tinta's own HTML export so diagrams and math survive as vector graphics
- **Start page** - Bare launches land on a launcher with recent files, quick actions labeled with your actual key bindings, and built-in guides; closing the last tab returns there, and Ctrl+T opens it as a new tab
- **Review annotations** - Select text and press `A` to attach a note, stored as an invisible HTML comment in the file itself. Annotated passages get a tint and a marker rail beside the scrollbar; hover previews, click edits, and a copy-for-agent button turns the whole review into a paste-ready task list for any coding agent - as the agent fixes items and deletes the comments, the marks vanish live
- **File references** - Plain-text paths like `docs/plan.md` become real links: live targets open as tabs, missing ones render as faded ghosts. Mouse back/forward (or Alt+arrows) walks your jumps with exact scroll restore
- **Link peek** - Rest the pointer on a local `.md` or wiki link and the target appears fully rendered in a panel beside it, no tab opened
- **Image lightbox** - Click any inline image to view it full size: wheel zooms, drag pans, Esc closes
- **Copy tables as TSV** - Hover a table for a copy button that pastes into Excel or Sheets as a real grid
- **Copy diagrams as images** - Hover a diagram for an Image button that puts a crisp 2x PNG on the clipboard
- **Paste screenshots** - Ctrl+V an image in edit mode and choose where it goes: the document's assets folder, inline in the Markdown, or anywhere with Save as. Tick Remember, or set it under Settings > Editor, to skip the question
- **Rich tables** - Tables with bold, italic, code, and clickable links in cells
- **Tabs** - Win11 Notepad-style tabs in the title bar: files opened from Explorer join the window as tabs, Ctrl+Tab cycles, Ctrl+W or Ctrl+F4 closes (Ctrl+F4 also while editing, where Ctrl+W is word wrap), middle-click closes, Ctrl+T opens the file browser into a new tab, unsaved buffers show a dot, resting the pointer on a tab shows its full name, its folder and what its dot means, and dragging reorders tabs. Right-click a tab for close operations (close, close all but this, close all to the left/right) plus copy path and reveal in Explorer. Pulling a tab out of the strip floats it as a card: drop it on open space for a new window (which keeps the tab row), or onto another Tinta window to move it there — and dropping a single-file window onto a tab strip merges it back. Single-file windows stay tabless; turn the Explorer behavior off with the "Open files in tabs" setting
- **Folder browser** - Press B to browse and open Markdown or Mermaid files (Ctrl+click a file to open it in a new tab)
- **Table of contents** - Press Tab to see document headings, click to jump; the panel follows your reading position, and typing filters the headings
- **Unified editor** - Press `:` to edit: your raw Markdown on the left with the rendered page docked beside it. A slim line-number gutter, a soft accent wash on the caret's line and its rendered block, a tool rail with formatting controls plus table-size and diagram-template pickers, and Markdown assists (lists continue on Enter, Tab indents, Ctrl+B/I wrap, and a quote, bracket or `* ~ ^ = :` typed over a selection wraps it). Alt+↑/↓ moves lines, and several carets (Alt+click, Ctrl+Alt+↑/↓, Ctrl+D) type, delete and move together, handy for lining up tables. A word and character count sits beside the Read button, for the whole document or the selection. Search and find-replace work in the editor too
- **Emoji shortcodes** - `:rocket:` and friends render as real emoji
- **Fit to width** - Wide tables and diagrams grow a Fit button that shrinks them to the reading column
- **Draft recovery** - Unsaved quick notes are stashed continuously and offered back on the next launch. With **Close without asking** (Settings → Editor), closing Tinta skips the save prompt and keeps unsaved edits as drafts for the next launch
- **Create on click** - Clicking a missing file reference offers to create the file and opens it ready to type
- **Text-file references** - `.txt`, `.json`, `.yaml` and friends link like Markdown files and open as highlighted documents
- **Update check** - Portable builds check GitHub once a day and offer a new release as a quiet, dismissible chip
- **Search** - Find text with F or Ctrl+F, cycle through matches with Enter; every match shows as a tick on the scrollbar. Ctrl+Shift+F, or the list button at the end of the search bar, opens a results panel beside the page: every match with a snippet, grouped under its heading, then the matches in your other open tabs and in the document's folder, by line. A click jumps to a match, or opens another file in its own tab at that match
- **Persistent settings** - Remembers your theme, zoom level, and window position
- **Portable mode** - Create a `settings.ini` next to `tinta.exe` (an empty file works) and the whole configuration — settings, custom themes, languages, drafts — lives beside the executable and travels with it; without one, everything stays in `%APPDATA%\Tinta`
- **Localized interface** - English, Simplified Chinese, Japanese, Korean, German, French, and Italian UI; follows Windows or a chosen language
- **Text selection & copy** - Select text and copy to clipboard
- **Zoom support** - Ctrl+scroll to zoom in/out
- **Drag & drop** - Drop any Markdown or Mermaid file to view it
- **Minimal footprint** - Small binary, minimal dependencies

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `B` | Toggle folder browser |
| `Tab` | Toggle table of contents (type to filter, Enter jumps) |
| `A` | Annotate the selected text |
| `F` / `Ctrl+F` | Open search |
| `Enter` | Next search match |
| `Ctrl+Shift+F` | Search results panel: every match beside the page |
| `ESC` | Close overlay / Quit |
| `T` | Open theme chooser |
| `S` | Toggle stats overlay |
| `Ctrl+C` | Copy the selection as Markdown — its emphasis, code, links, list markers and table header come along (or the whole document as written, if nothing is selected) |
| `Ctrl+A` | Select all text |
| `Ctrl+Scroll` | Zoom in/out |
| `Arrow keys` / `J/K` | Scroll |
| `Mouse back/forward` / `Alt+←/→` | Walk link jumps with scroll restore |
| `Page Up/Down` | Page scroll |
| `Home/End` | Jump to start/end |
| `:` | Enter edit mode |
| `ESC` `ESC` | Exit edit mode |
| `Ctrl+N` | New quick note — a fresh window, start typing right away; `Ctrl+S` names it via the save dialog. While the note is still empty, an **Open a file** button (`Ctrl+O`) in the preview pane picks an existing file instead |
| `Ctrl+S` | Save (in edit mode) |
| `Ctrl+Shift+S` | Save As: the editor writes the buffer to a new path, the viewer saves a copy, and the document continues under the new name |
| `Ctrl+H` | Find and replace (in edit mode): Tab switches fields, Enter replaces, Ctrl+Enter replaces all |
| `Ctrl+P` | Print / export to PDF; the preview picks the paper, orientation and margins |
| `Ctrl+E` | Show/hide preview pane (in edit mode) |
| `Ctrl+Shift+E` | Read the unsaved edits at full width; `Esc` returns to the source (in edit mode) |
| `Ctrl+W` | Toggle word wrap (in edit mode) |
| `Alt+↑` / `Alt+↓` | Move the current line, or the selected lines, up or down (in edit mode) |
| `Ctrl+Alt+↑` / `Ctrl+Alt+↓` / `Alt+Click` | Add a caret above, below or at the click; `Esc` returns to one caret (in edit mode) |
| `Ctrl+D` | Select the word, then add its next occurrence as another caret (in edit mode) |
| `Ctrl+F4` | Close the current tab (viewer and edit mode) |
| `Q` | Quit |

## Customization

**Shortcut profiles** (Settings → Keyboard shortcuts): pick **Windows**
(`E` edits, `F1` opens help, `F` searches — the default), **Vim** (`/`
searches, `:` edits, `?` helps), **Emacs**, or **Custom**. Emacs adds its
chords in the reader and the source editor: `C-n`/`C-p`/`C-f`/`C-b`,
`C-a`/`C-e`, `M-f`/`M-b`, `C-v`/`M-v`, `M-<`/`M->`, `C-s`/`C-r`, the mark
(`C-SPC`) with `C-w`/`M-w`/`C-y`, `C-k`/`M-d` (consecutive kills append),
`C-/`, `C-t`, `C-o`, `M-%`, `C-g`, and `C-x` with `C-f`, `C-s`, `C-w`,
`C-q` (edit/read), `C-c`, `k`, `b`, `h`, `u` and `C-x`. The clipboard is the
kill ring; chords Emacs leaves undefined keep their Windows meaning, and
Help (`C-h`) lists them all. The Edit button opens a
shortcut editor: click an action, press its new key. Raw `:` and `?`
always work regardless of profile.

Settings live in a plain INI file at `%APPDATA%\Tinta\settings.ini` (theme,
zoom, window placement, word wrap, and more — written on exit). The `[Keys]`
section holds the Custom profile's bindings if you prefer editing by hand:
change a value and restart Tinta. Accepted values are letters, digits,
`Tab`, `Space`, `F1`–`F12`, or any single character.

```ini
[Keys]
search=F
browse=B
toc=Tab
theme=T
stats=S
quit=Q
newfile=N
zen=F11
scrollup=K
scrolldown=J
edit=:
help=?
```

Ctrl combos (save, print, copy, word wrap…) are fixed. The help overlay and
context menu show whatever keys are currently bound.

Other settings of note: `browserFocusPath=1` makes `B` open the file browser
with the path box focused and selected, so paste + Enter jumps anywhere
(also in Settings → General). `tinta --new` starts an untitled quick note
from the command line. The print preview offers 12.7, 19.05 and 25.4 mm page
margins; `printMarginMm=` takes any value from 5 to 50 for printing and PDF.

The interface language is available in Settings → General. It follows the
system display language by default, or can be changed explicitly. Transient
notifications and native error dialogs use the same language. Additional
translations can be added through `languages.ini`; the app remains a native,
single-file Windows viewer with no web runtime.

## Building

Requires Windows with Visual Studio 2019+ and CMake 3.15+.

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

The executable will be at `build/Release/tinta.exe`.

### Both release downloads, from Linux

```bash
packaging/build-release.sh 4.0.11
```

This builds, for the version given:
- the portable Windows exe, cross-compiled with clang-cl;
- the Linux AppImage.

It then checks both. Each must carry the version, and each must export a
Mermaid document to HTML headless; the exe runs under Wine when Wine is
installed. The results land in `build/release-4.0.11/` with `SHA256SUMS`
and a `BUILD-INFO.txt` naming the commit and any uncommitted changes. The
version applies to that build only; `CMakeLists.txt` is not edited. Nothing
is committed, tagged or published. The script's header lists the
first-run requirements.

## Usage

```bash
# Open a Markdown file
tinta.exe document.md

# Open a Mermaid flowchart
tinta.exe diagram.mmd

# Open with light theme
tinta.exe -l document.md

# Show stats on startup
tinta.exe -s document.md

# Register for Markdown and Mermaid files
tinta.exe /register
```

Or simply drag and drop a `.md` or `.mmd` file onto the window.

To copy a document's full path, right-click the document or its tab (the filename
in the title bar when only one file is open) and choose
**Copy file path**, beside **Show in folder**. The tab menu copies the clicked
tab's path, even if another tab is active. Untitled notes have no path until saved.

## File Association

On first launch, Tinta will ask if you want to set it as the default viewer for Markdown and Mermaid files. If you choose "No", you won't be asked again.

To register Tinta as the default viewer later, run:
```bash
tinta.exe /register
```

This registers `.md`, `.markdown`, and `.mmd` so you can select Tinta as their default app in Windows Settings.

## Mermaid Support

Tinta natively renders twenty-two Mermaid diagram families in `.mmd` files and fenced `mermaid` code blocks — no web engine involved:

- **Flowcharts** (`flowchart` / `graph`) - TB/TD, BT, LR, RL layouts, `subgraph` groups, common node shapes, directed and labeled edges, `classDef`/`class`/`style` styling, and graceful handling of Mermaid 11 edge IDs and node configs
- **Sequence diagrams** - participants and actors with aliases and `<br/>` breaks, every arrow family, activations, notes, `autonumber`, and `loop`/`alt`/`opt`/`par`/`critical`/`break` frames
- **Class diagrams** - member blocks, stereotypes, `~T~` generics, all relation kinds with multiplicities, `note` and `note for`
- **State diagrams** - `[*]` start/end, choice/fork/join, descriptions, directions, inline notes
- **ER diagrams** - attribute tables with keys, full crow's-foot cardinality notation
- **C4 diagrams** (`C4Context`/`C4Container`/`C4Component`/`C4Dynamic`/`C4Deployment`) - persons, systems, containers, and components in the standard C4 palette, nested boundaries, labeled relations
- **Requirement diagrams** - typed requirement and element boxes with labeled relations
- **Architecture diagrams** (`architecture-beta`) - services with line-art icons, groups, junctions, and side-anchored edges
- **Block diagrams** (`block-beta`) - column grids, spans, spaces, nested blocks, block arrows, labeled edges
- **Sankey diagrams** (`sankey-beta`) - ranked nodes with smooth flow ribbons
- **Packet diagrams** (`packet-beta`) - 32-bit rows with labeled bit-range fields
- **Kanban boards** - columns and cards with ticket, assignee, and priority metadata
- **Treemaps** (`treemap-beta`) and **radar charts** (`radar-beta`)
- **Pie charts**, **git graphs**, **gantt charts** (including the numeric `dateFormat X` axis), **mindmaps**, **timelines**, **user journeys**, **quadrant charts**, and **XY charts** (`xychart-beta`)

Diagrams follow the active theme, print through Ctrl+P, export as SVG in HTML and as crisp images in DOCX, and their text is selectable and searchable like any other content. Anything a native renderer does not cover yet (composite states, zenuml) falls back to readable source code.

## Themes

Press `T` to open the theme chooser:

**Light Themes:**
- Paper - Warm sepia, literary feel
- Sakura - Soft pink elegance
- Arctic - Cool blue-white
- Meadow - Fresh green
- Dusk - Warm gray twilight

**Dark Themes:**
- Midnight - Deep blue-black
- Dracula - Purple-tinted dark
- Forest - Deep green
- Ember - Warm charcoal
- Abyss - Pure black (OLED-friendly)

Custom themes live in `themes.ini` beside the portable executable, or in
`%APPDATA%\Tinta`. Add `inlinecode` to a `[theme]` section to color inline code
independently of fenced blocks:

```ini
code=334455
inlinecode=B00020
```

Here inline code is red and plain fenced code stays neutral. Syntax highlighting
keeps its own colors. Without `inlinecode`, inline code uses `code` as before.
The override is preserved when saving a theme from the editor and applies to
HTML and DOCX exports. Native printing keeps Tinta's light print palette, set in
the theme's fonts.

For supported code-fence languages and aliases, including SQL, PowerShell, Java,
PHP, HTML/XML, CSS, YAML and Markdown, see [Syntax highlighting](docs/syntax-highlighting.md).
The [Frontmatter settings](docs/frontmatter.md) page controls property display and optional created/updated dates.

Named, repeated and multiline [footnotes](docs/footnotes.md) include native navigation and HTML/DOCX export.

Individual H1-H6 and marked-text overrides are described in [Heading and highlight colours](docs/theme-colours.md).

## Dependencies

- [MD4C](https://github.com/mity/md4c) - Fast markdown parser (fetched automatically by CMake)
- Windows Direct2D/DirectWrite (system libraries)

## License

MIT
