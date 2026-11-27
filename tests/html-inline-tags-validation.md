# HTML sub, sup and ins tags (#263)

Validated on Windows with the Release build on 2026-10-16.

## Report

`<sub>`, `<sup>` and `<ins>` (the underline GitHub documents for its
Markdown) showed as literal tags. The reporter knows `^text^` and `~text~`
but asked for the HTML tags too, for compatibility.

## Change

**Inline HTML in paragraphs, headings, lists, quotes and table cells**

- These tags become the same elements as the Markdown spans:
  - `<sub>` and `<sup>`;
  - `<ins>` and `<u>` (a new underline style);
  - `<del>`, `<s>` and `<strike>`;
  - `<mark>`.
- Tags may carry attributes, use any case and wrap their attributes onto the
  next line.
- A close tag finds the innermost open span of its kind:
  - spans opened inside it close with it;
  - Markdown spans it cuts through open again after it, so
    `<ins>a *b</ins> c*` reads as a browser reads it.
- A stray close tag draws nothing, as on GitHub. A span left open ends with its
  paragraph, list item or cell.
- `~~`, `==`, `^`, `~` and `[[wiki|links]]` pair across these spans:
  `~~H<sub>2</sub>O~~` is struck-through H₂O.
- A comment spread over lines stays text, whatever tags it mentions.
- Code spans and fenced code keep the tags as text.

**HTML blocks** (`<p>…</p>` and the like)

- The same tags are read there too.
- One space stays beside inline tags. Before, `<p>a <b>b</b> c</p>` was drawn
  as "abc", and the same was true of the new tags.
- A close tag only closes its own element. A stray one used to end whatever
  element was open.
- A wrapper alone on its line (`<sub>` above some Markdown) no longer draws an
  empty line.
- Inside `<pre>` and `<code>`, the tags are dropped and their text kept, as
  before.
- A heading beside a linked logo or badge keeps its title and slug: a
  block drops the space left before its last inline tag. Markdown headings
  keep their slugs exactly as before.

**Drawing**

- Underlines sit just under the baseline of the text where the underline
  began:
  - one continuous rule runs across bold, italic, code, math and the spaces
    between them;
  - `<sup><ins>2</ins></sup>` underlines the raised 2;
  - a link inside `<ins>` shares the rule rather than adding a second one;
  - a ruby base is underlined under its own text.
- Superscripts and subscripts in a heading use a face in proportion to the
  heading, so `# H<sub>2</sub>O` and `# x^2^` look right. This also fixes the
  Markdown syntax, which used the body-size face and placed scripts above the
  heading's baseline.

**Exports**

- HTML writes `<ins>`, `<del>`, `<sub>`, `<sup>` and `<mark>`. Word writes
  single underline, strike-through, vertical alignment and highlighting.
- Image alt text keeps the words of nested spans in both exports.
- Fixed while in the same code: a `[[wiki]]` candidate skipped after a found
  link no longer drops the text before it (`[[a]] b [[]] c`).

## Fixtures and automated checks

`fixtures/html-inline-tags-263.md` mixes the tags into:

- a heading;
- body text with bold, italic, code, a mark, `==highlight==`, `^sup^` and
  `~sub~`;
- Chinese text;
- nested lists, an ordered list with a link, and a quote long enough to wrap;
- a table;
- a link and inline math inside `<ins>`;
- an HTML block with ruby, bold and italic;
- a lone wrapper line;
- an image whose alt text holds a subscript;
- code spans, a fence, a stray close tag and upper-case tags.

The new `html_spans` test:

- **Parser.** 39 small documents, each checked for the exact element tree,
  plus the slugs of two HTML headings. They cover nesting and misnesting with Markdown spans and links,
  same-kind nesting, unclosed tags, stray and self-closing tags, attributes,
  upper case, tags split over lines, quoted `>`, comments, code spans, the
  extension and wiki pairing, headings, tables, tight lists with sublists,
  quotes, and HTML blocks (spacing, closing, pruning, `<pre>`/`<code>`,
  anchors, ruby, heading slugs).
- **Layout.** It lays the fixture out in Paper and Midnight, at 1x and 1.5x,
  1050 and 650 px, and checks:
  - subscripts drop and superscripts rise in small type, in body text and in
    the heading (where they scale with it);
  - every `<ins>`/`<u>` run has a rule under its baseline, unbroken across
    each line, including the wrapped quote, Chinese text, the table cell, the
    nested list, the ruby base and the math;
  - the rule under math is level with the text's;
  - a link inside `<ins>` has exactly one rule;
  - `<sup><ins>` and `<ins>…<sup>` place their rules as intended;
  - text between two separate spans, `<del>` text, plain text and fenced code
    stay without a rule;
  - only the fenced `<ins>` and the code span keep literal tags, and the stray
    close tag draws nothing;
  - the HTML block keeps its spaces, and the lone wrapper takes no room.
- **Exports.** HTML has `<sub>`, `<sup>`, `<ins>` (also for `<u>`), `<del>`
  (also for `<s>`) and `<mark>`, the HTML block with its spaces, and the alt
  text. Word has underline, strike, subscript and superscript runs and the alt
  text.

`superscript_and_learn` now expects scripts in proportion to the text around
them (a heading's to the heading), on the right side of that text's
baseline, including inside emphasis in a heading.

Every part of the change was disabled in turn, and each made at least one
check fail:

| Change disabled | Failures |
| --- | --- |
| Inline tags not read | 280 |
| Underline not drawn | 196 |
| Spans not reopened after a misnested close | 3 |
| Sublist does not close a tight item's spans | 1 |
| Tags split over lines not joined | 3 |
| Post-passes stop at HTML spans | 2 |
| Wiki text before a skipped candidate lost | 1 |
| HTML-block spacing trimmed again | 13 |
| HTML-block close tags pop anything | 3 |
| Empty wrappers kept | 9 |
| Tags inside `<pre>`/`<code>` read | 2 |
| Text-less inline siblings not transparent | 1 |
| A block keeps the space before its last inline tag | 2 |
| A block left open at the end of the HTML keeps it | 1 |
| Escapes not read in raw HTML copies (footnotes) | 1 |
| Rule joins not reset between spans | 14 |
| Link keeps its own second rule | 8 |
| Rule under math at the box bottom | 8 |
| Rule placed from the paragraph baseline under `<sup><ins>` | 8 |
| Ruby base not underlined | 8 |
| Heading scripts in the body-size face | 16 (and 108 in `superscript_and_learn`) |
| Heading-italic script face | 36 in `superscript_and_learn` |

The full suite passed 36 of 36.

## Reviews

Three rounds of review, each finding verified independently before it was
acted on, shaped the change:

- **Round 1** found:
  - HTML-block spacing gluing words;
  - empty wrapper lines;
  - double rules under links;
  - stepped rules under math;
  - rule placement under superscripts and ruby;
  - an untested join reset.
- **Round 2** found:
  - text lost inside `<pre>`/`<code>`;
  - heading slugs broken by kept spaces;
  - post-pass pairing lost across spans;
  - multi-line comments read as tags;
  - alt text lost in exports.
- **Round 3** looked at the post-pass and raw-HTML rework. It found:
  - a heading-slug trim that reached plain Markdown headings (now only HTML
    blocks drop the space, where it ends a block);
  - escaped footnote brackets inside raw HTML read as references;
  - an HTML heading left unclosed keeping that space in its slug.

## Desktop check

The fixture was opened in the built app in Paper and Midnight, at 1000 and
650 px, at the top and scrolled down (`out/issue-263/` and
`out/issue-263/round2/`, ignored). It showed:

- H₂O and mc² in proportion in the heading;
- continuous underlines across bold, italic, code and math;
- the wrapped quote underlined on each line;
- a single rule under the link;
- the ruby base underlined;
- the HTML block with its spaces;
- the lone wrapper taking no room.

## Exports

The #260 commit (9b5b11a) and this build exported HTML, DOCX and print pages
for 12 control documents and the new fixture. The controls were:
markdown regression control, footnotes, inline math stress, mixed math,
superscript/subscript, wiki links, reading view, anchor links, frontmatter,
table cells, the #259 fixture and the #256 fixture.

- HTML and Word were byte-identical for every control.
- Print pages were identical except page 1 of `superscript-subscript`, where
  the heading's scripts now scale with it.

## Limits

- `<u>`, `<s>` and `<strike>` closed by an enclosing element are not reopened
  after it, as a browser would; only Markdown spans are.
- A close tag that would cut through an image, a code span or inline math is
  ignored, and its span runs on to the end of that element's paragraph.
- `~~`, `==`, `^` and `~` still do not pair across Markdown emphasis
  (`~~a *b* c~~`), as before.
- The spaces on both sides of a stray close tag stay, so it can leave two
  spaces.

## Draft reply (not posted)

### #263

Thanks for the request! The next release renders `<sub>`, `<sup>` and
`<ins>`, plus `<u>`, `<del>`, `<s>` and `<mark>`, in paragraphs, headings,
lists, tables, quotes and HTML blocks, mixed with bold, italic, code, links
and math. They are also kept in HTML and Word exports. `~~` and `==` work
across them too, so `~~H<sub>2</sub>O~~` comes out struck through, as on
GitHub.
