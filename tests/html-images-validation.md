# Images and links written in HTML: validation

2026-11-26. Philip: a README's Obtainium badge,
`<a href="https://apps.obtainium.imranr.dev/redirect?r=obtainium://app/%7B…%7D"><img src="docs/images/badge_obtainium.png" alt="Get it on Obtainium" width="161" height="48"></a>`
alone on its line, showed in the preview as its raw tags.

## Cause

- **It is inline HTML.** The opening tag doesn't stand alone on its line,
  so by CommonMark's rules the line is a paragraph of inline raw HTML, not
  an HTML block.
- **Inline raw HTML** became elements only for `<br>`, font tags, `<a id>`
  and `<a name>` anchors, and the `<sub>`/`<sup>`/`<ins>`/`<u>`/`<del>`/`<s>`/`<mark>`
  spans. `<a href>` and `<img>` stayed literal text.
- **HTML blocks** (`<p align="center"><img …></p>`) dropped `<img>`
  silently: there was no image element anywhere for HTML.
- **No size:** images had no width or height; they drew at their own pixels.
- **No click:** an image inside a link (Markdown's `[![badge](img)](url)`
  too) registered no link area, so it did not open its link.

## Fix

- **Parser** (`src/markdown.cpp`):
  - A shared `readInlineTag` reads a raw tag's name and attributes (quoted
    or bare, entities decoded). `anchorTag` now uses it.
  - Inline `<img src>` becomes an `Image` element with its alt text and
    `width`/`height` in CSS px (`161`, `161px`; a percentage is no size).
    An `<img>` without a `src` draws nothing.
  - Inline `<a href>` opens a `Link` around what follows. `</a>` closes it,
    and a link left open ends with its paragraph.
  - `<a id>` anchors and a stray `</a>` behave as before.
  - HTML blocks turn `<img>` into an `Image` the same way.
- **Layout** (`src/render.cpp`):
  - An image with an HTML size draws at it, scaled with the text; the
    missing side comes from the picture's proportions
    (`imageDisplaySize` in `include/markdown.h`).
  - It fits the column as before, and Markdown images are unchanged.
  - An image inside a link registers that link over the picture (no
    underline), and links win over the lightbox on click.
- **Exports:** HTML writes `width`/`height` on `<img>`, and a link around
  an image stays `<a><img></a>`. Word sizes the picture with the same
  helper; the image is not a hyperlink there.

## Checked

- **Fixture** `tests/fixtures/html-images.md`, with its own 400x120
  `html-images.assets/badge.png`. It covers:
  - the reported case, a long `%`-escaped href around a sized image;
  - an image in a sentence (height only);
  - a Markdown badge in a link;
  - an HTML block image (width only);
  - inline HTML links in a paragraph, a list and a quote;
  - images in a list item and a table cell;
  - a missing image whose alt text has an apostrophe;
  - headings, emphasis and code around them.
- **`document_types`** (`htmlImageTests`): the tree each case parses to.
  - Link around image; src, size and alt with entities decoded; no raw tag
    left as text.
  - Bare, px and percentage sizes; an `<img>` without a src.
  - An inline link's text; `<a id>` and a stray `</a>`; an unclosed link
    ending with its paragraph.
  - The HTML-block image; a Markdown image with no HTML size.
  - The whole fixture: seven images, two of them in links, no raw `<img>`
    or `<a href>` left.
  - **Mutation check:** with the inline `<img>` and `<a href>` branches
    disabled, 15 checks fail.
- **`image_cache_and_decode`** (`htmlImages`): lays the fixture out in
  Paper and Midnight, at 1050 and 560 px and 1x and 1.5x.
  - Six images draw at their expected sizes, and the missing one shows its
    alt text.
  - The HTML badge and the Markdown badge are links the size of the picture.
  - The real mouse-move handler over the badge reports its link.
  - HTML export writes `<a href="…"><img … width="200" height="60"/></a>`;
    Word draws the badge 1905000x571500 EMU (200x60 px).
  - **Mutation check:** without the HTML size and the image link rect, the
    size and link checks fail.
- **By eye:** headless renders of the fixture in Paper at 1000 px and
  Midnight at 560 px. Philip's own README renders its badge at 161x48 where
  the raw tags were (render kept in `build/`, not committed).

## Not covered / known

- **An image mid-sentence** still breaks the line into a block of its own,
  as Markdown images always have. A row of badges stacks vertically rather
  than sitting side by side.
  - In an HTML heading, `<h1><a><img></a> MyProject</h1>` draws the logo
    above the title, and `Installation <a><img></a>` draws the badge under
    its heading. Before this change those images were dropped.
  - The text after such an image keeps its leading space.
  - `html_spans` snapshots now show `Link[Image]` there. The headings' slugs
    and titles are unchanged, and checked.
- **A paragraph holding only an image** keeps about 60 px of space below
  it (the image's margin, an empty line, then the paragraph gap). This
  predates the change.
- **Not honoured:** `align="center"` and other layout attributes; images
  stay at the left margin.
- **Word export:** a linked image is not a hyperlink.
- **The five Xvfb-driven ctests** (`linux_gtk_window`, `linux_tab_drag`,
  `linux_print`, `linux_linked_scroll`, `compat_clipboard_bridge`) could
  not run on 2026-11-26. The dist upgrade left no `xvfb-run` or `Xvfb` on
  the machine (Linuxbrew dropped it, and apt's `xvfb` is not installed).
  They fail the same on the unchanged v4.0.13 code, so this change is not
  the cause.
