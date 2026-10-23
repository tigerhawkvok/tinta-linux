# Admonitions in every dialect

Validated on Linux (Xvfb, X11 backend) on 2026-10-23. Requested by Philip:
"add all the 'standard' flavors of admonitions", rendered like Docusaurus,
nesting included.

## Dialects

| Dialect | Syntax | Titles | Folding | Nesting |
|---|---|---|---|---|
| GitHub, GitLab alerts | `> [!NOTE]` | text after the marker | no | `> >` |
| Obsidian callouts | `> [!faq]- Title` | after the marker, inline Markdown | `+` open, `-` folded | `> >` |
| Docusaurus | `:::tip[Title]`, `:::tip Title` | brackets (Markdown) or the rest of the line | no | more colons outside |
| VitePress | `::: warning Title`, `::: details` | rest of the line | `details` folded | more colons |
| MyST | `:::{note}`, ```` ```{note} ````, `:class: dropdown` | `{admonition} Title` | `dropdown` | colons, longer fences |
| MkDocs (Material) | `!!! note "Title"`, `???`, `???+` | quoted; `""` hides the bar | `???` folded, `???+` open | indentation |
| reStructuredText | `.. note::`, `.. admonition:: Title` | `admonition` only | no | indentation |
| Hugo shortcodes | `{{< alert "warning" >}}`, `{{% alert title= color= %}}`, `{{< tip >}}` | `title=` or a quoted argument | `open=` | by tag name |
| HTML | `<div class="admonition note">`, `<p class="admonition-title">` | the title paragraph | no | no |

The kinds, with their aliases, are those of Obsidian and Material plus
GitHub's: note (seealso), abstract (summary, tldr), info, todo, tip (hint),
important, success (check, done), question (help, faq), warning (warn,
attention), caution, failure (fail, missing), danger (error), bug, example,
quote (cite) and details (dropdown). An unknown type after `[!...]` or `!!!`
is a note titled after the type, as in Obsidian and Material. An unknown `:::`
type, a reStructuredText directive that is not an admonition, and any marker
inside fenced code stay as written.

Notion has no admonition syntax (an emoji and bold text), so there is nothing
to parse.

## Change

- **Parsing** (`markdown.cpp`, `include/admonition.h`). For every dialect but
  the blockquote callouts, a pre-pass finds the markers outside fenced code
  and blanks them in the copy md4c reads. The copy has the same length, so
  every source offset (scroll sync, task checkboxes, copy as Markdown) stays
  exact. The blocks of each body are then gathered into a blockquote with
  the admonition fields set: kind, keyword, a title of inline Markdown,
  fold state, and no-title.
- **Callouts:** `> [!type]` blockquotes take a fold mark and a title, and
  every type. Behaviour change: `> [!NOTE] text` is now a callout titled
  "text", as in Obsidian and GitLab; github.com would show a plain quote.
- **Viewer** (`render.cpp`): one look for all dialects, after Docusaurus: a box
  tinted with its kind's accent, a 4 px accent bar, and a title bar with the
  kind's icon and the title (or the type's label). Nested boxes sit inside
  their parents. A foldable box shows ▸ or ▾ and folds when its title bar
  is clicked; the state is kept per file and offset. On paper every box is
  open.
- **Exports:** HTML writes `<div class="admonition KIND">`, or a
  `<details>`/`<summary>` (open as written) for a foldable box, with the
  viewer's palette for the theme. Word writes the accent bar and an
  icon-and-title line, with every box open.

## Checked

- `document_types` (ctest) checks 26 cases, one or more per dialect, for
  kind, keyword, title, fold and body. It also checks:
  - the box's offset, and that the body's and title's offsets point into
    the source;
  - nesting in each style, and a container inside a list item;
  - markers in fenced code, unknown `:::` types, other directives and
    mid-line `:::` all staying text;
  - empty and unclosed containers, title-only callouts and labels.
- `blockquote_layout` (ctest), for four dialects:
  - the box ends 0.75 of its padding below its last line, with the bar down
    its left edge;
  - all text sits inside the box, and the next block keeps its 14 px gap;
  - a nested box sits inside its parent.
- `admonitions` (ctest) runs on `tests/fixtures/admonitions.md`: every
  dialect among headings, lists, a table, code, links, emphasis and a plain
  quote, 26 boxes. In two themes, two scales and two widths it checks:
  - every box draws, and text stays inside its box;
  - titles reach the searchable text, and folded bodies do not;
  - there are four foldable title bars; unfolding shows a body and folding
    hides one;
  - print opens every box.

  In the HTML: every box, a closed and an open `<details>`, a Markdown
  title, no title for `""`, labels, every kind in the stylesheet, and a
  plain quote staying `<blockquote>`. In Word: titles, folded bodies
  included, and labels.

  Mutations caught: ignoring the fold state, dropping the tint, and a body
  as wide as the box.
- **The built app on Xvfb:** the fixture page by page at 1100 px in the
  light theme and 760 px in the dark theme. A real click on the folded
  "Why are some of these folded?" title unfolds it: ▸ becomes ▾ and the
  body appears. The screenshots are in `build/admonition-gui/`; they led
  to one fix, a custom title spacing its body a line further than a label.
- Full ctest: 65/65.

## Limitations

- Not yet run on Windows.
- The exported HTML was not opened in a browser; its markup is checked.
- Collapsed bodies are not searched (as in Obsidian); unfold to find text
  inside.
- Docusaurus `:::` containers inside a `>` blockquote, and HTML admonitions
  with nested `<div>`s, are not recognised.
