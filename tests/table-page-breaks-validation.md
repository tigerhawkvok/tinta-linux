# Tables across page breaks (PDF and print): validation

2026-10-29. Found while validating #1: in a PDF export, a table row at the
foot of page 1 left an empty slice of the row there, and its text sat on
page 2. The 4.0.12 release does the same.

## Causes

Two, both in `src/print.cpp`:
1. **The break fell inside a row.** `computePageBreaks` pulls a break above
   any text line, image, shape or scaled band it would cut, but it knew
   nothing of table rows. A row's cell padding lies outside its text lines,
   so the break landed in the padding: below the row's top border, above
   its text.
2. **Rules ran past the break into the margins.** `drawDocumentRange` drew
   every item overlapping the page's slice without clipping. A table's
   column rules span the whole table, so a table split even between rows ran
   them to the paper's edge on page 1, and from the top edge of page 2. A
   quote bar or callout tint crossing a break did the same.

## Fix

- **Row bands:** table layout records each row's extent
  (`App::tableRowBands`), with the header joined to the first body row, and
  page breaks treat them as atoms. A page never ends inside a row or on a
  lone header row. A row taller than 70% of a page is still sliced, as before.
- **Clip:** `drawDocumentRange` clips to the page's slice. A rule lying on
  the break draws on both pages (a 1.5 DIP edge), so a table split between
  rows is closed at the foot of one page and opens with a top border on the
  next.

## Checked

- **Fixture:** `tests/fixtures/table-page-breaks.md`. Tables with short and
  wrapping rows, aligned columns and a header-only table, between headings,
  text with a link and code, lists, a quote and a fenced block.
- **`wide_table_layout`** (`pageBreaksKeepRows`):
  - Lays the fixture out at 1050 and 560 px, then computes page breaks for
    page heights from 120 to 1400 px in 3.7 px steps.
  - Rows come from the drawn grid lines, independently of the fix. No break
    may fall inside a row, or between a header and its first row, unless that
    span is taller than 70% of the page.
  - **Mutation check:** without the row rule it reports 729 cuts at 1050 px
    and 724 at 560 px. The first is 8 px below a row's top, exactly the cell
    padding.
- **`linux_pdf_margins`** (new ctest): exports the fixture, rasterises every
  page at 72 dpi and requires each page's ink to stay inside the default
  19.05 mm margins.
  - The fix passes: ink rows 71..715 and 53..567 of 792, with 52..740 allowed.
  - The 4.0.12 AppImage fails it: page 1's ink runs to row 792, and page 2's
    starts at row 0.
- **Full suite:** ctest 66/66. The Windows cross-build compiles.
- **By eye, two PDFs:**
  - The fixture breaks between rows 5 and 6. Page 1's table is closed;
    page 2 opens with row 6 under a top border.
  - In the #1 fixture, the one-row table moves to page 2 with its header,
    and page 1 ends after the quote.

## Not covered

- **Repeating the header row** on the next page, as browsers do with
  `<thead>`, is not done.
- **Printing on Windows** goes through the same `computePageBreaks` and
  `drawDocumentRange`, but was not run there.
