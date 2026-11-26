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

## Images in the line (later on 2026-11-26)

Philip: "Make images inline, fix the gap". Every image, Markdown or HTML,
now sits in the text as a browser sets it.

### How it lays out

- **Placement:** the image's bottom sits on the baseline, and it wraps like
  a word when it doesn't fit. A badge row stays a row, and a badge stays
  beside its heading's words.
- **A taller image grows its line** (`layoutInlineContent` in
  `src/render.cpp`). What the line already holds moves down by the
  difference, found by position (bottom below the line top), so its text
  keeps the image's baseline. `shiftLineDown` also re-buckets the moved
  text rects for selection and hit testing.
- **List markers follow:** the bullet, number or task checkbox of an item
  whose first line grew moves with it (`App::grownFirstLines`).
- **A missing image** keeps its `[image: alt]` text inside the sentence, in
  faded italics, as one word.
- **The gap is gone:** a paragraph holding only an image ends one line's
  descent below it, like any paragraph. Before, it had the image's 12 px
  margin and an empty line on top of the paragraph gap, about 60 px.
- **The old block layout** (`layoutImage`) is gone; `imageBoxSize` keeps
  its sizing rule.

### Checked

- **Fixture** `tests/fixtures/inline-images.md`, alongside
  `html-images.md`. It covers:
  - a row of three linked and unlinked badges;
  - badges in a Markdown and an HTML heading;
  - an 80 px image mid-sentence, with bold, code and a link after it;
  - an image alone in a paragraph;
  - badges in a bullet, task and numbered item;
  - a quote; two badges in one table cell;
  - a missing image in a sentence.
- **`image_cache_and_decode`** (`inlineImages`): Paper and Midnight, 1050
  and 560 px, 1x and 1.5x.
  - The row shares one baseline, left to right, and its links cover the
    badges.
  - Both headings' badges sit beside their words, on the words' baseline.
  - The tall image is either beside its words on their baseline, or (at
    560 px and 1.5x, where it doesn't fit) wrapped to the next line. The
    grown line starts below the block above.
  - The image alone leaves less than 30 px (scaled) before the next
    paragraph.
  - The bullet, number and checkbox share their grown first line's baseline.
  - The cell's two badges share a line inside the table.
  - The missing image's alt text stays in its sentence.
  - Every text rect lies in exactly one line bucket at its top.
  - **Mutation checks:**
    - Without line growth, "the grown line starts below the block above"
      fails in all eight combinations.
    - Without the marker shift, the bullet, number and checkbox checks fail
      in all eight.
- `htmlImages` and every other test pass unchanged: ctest 66/66, the Xvfb
  tests included once xvfb was installed.
- **By eye:**
  - the fixture in Paper at 1000 px and Midnight at 560 px;
  - the README badge now has a paragraph's gap under it;
  - headings with badges;
  - a PDF export of the fixture, within its margins on both pages
    (`pdf_margin_check.py`).

## Not covered / known

- **Not honoured:** `align="center"` and other layout attributes; images
  stay at the left margin, or wherever the line puts them.
- **`vertical-align` is not read:** images sit on the baseline.
- **Word export:** a linked image is not a hyperlink, and images are
  written as inline pictures, as before.
- `html_spans` snapshots show `Link[Image]` in HTML headings; the headings'
  slugs and titles are unchanged, and checked.
