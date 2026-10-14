# Inline code on the baseline (#262)

Reported: `fit` or `eval`, per base. Inline code sat about half a letter
lower than the words around it.

## A heading with `code` in it

Plain text with `short` code, **bold `code` inside**, *italic `code` inside*,
and a [link with `code`](https://example.com) all share one baseline. Math
$x^2$ sits beside `x^2` without moving it, and x^2^ stays a superscript.

- A list item with `code`
  - A nested item with `nested code`
1. An ordered item: `1 + 1`

> A quote with `quoted code` and enough words to wrap at narrow widths, so
> that `the second line` keeps its baseline too.

| Column | Value |
|---|---|
| `cell code` | text and `more code` |
| plain | **bold** and `code` |

A span longer than a narrow column wraps:
`a_very_long_identifier_that_cannot_possibly_fit_on_one_line_in_a_narrow_window`
and the text after it continues.

```text
A fenced block is not inline code and keeps its own layout.
```

The final paragraph with `trailing code`.
