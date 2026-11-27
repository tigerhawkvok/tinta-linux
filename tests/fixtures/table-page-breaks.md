# Tables across page breaks

Printed and exported pages keep each table row whole: a page ends between
rows, never inside one, and a header row stays with the first body row. A
row taller than most of a page is still sliced.

## A short table

| Column | Value |
|---|---|
| `cell code` | text and `more code` |
| plain | **bold** and *italic* |

Some text between the tables, with a [link](https://example.com) and
`inline code`, long enough to wrap onto a second line at narrow widths so
that the tables after it start at different heights.

## Rows of different heights

| Step | What happens | Notes |
|---|---|---|
| 1 | A single line | - |
| 2 | A cell long enough to wrap onto two or three lines in a narrow column, which makes this row taller than its neighbours | wraps |
| 3 | Short again | `code` |
| 4 | Another tall cell: the row's padding sits above and below its text, so a cut there would leave an empty slice of the row on the page above | wraps |
| 5 | Short | - |
| 6 | Short | - |
| 7 | A third tall cell, so tall rows fall at many page positions as the page height varies in the test | wraps |
| 8 | The last row | end |

- A list between tables
  - with a nested item
1. and an ordered one

> A quote with `quoted code`.

| Left | Centre | Right |
|:---|:---:|---:|
| a | b | c |
| d | e | f |
| g | h | i |
| j | k | l |

```text
A fenced block, which keeps its own breaking rules.
```

| Only | A | Header |
|---|---|---|

The final paragraph.
