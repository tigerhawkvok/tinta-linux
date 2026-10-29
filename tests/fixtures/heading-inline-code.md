# `Tinta` in a first-level heading

Inline code keeps its proportion to the text around it: 14 px beside 16 px
body text, and 7/8 of a heading's size inside a heading (#1).

## `git`

A heading made only of code, the reported case, is as large as its level.

### Install with `make install`

#### Level four `code`

##### Level five `code`

###### Level six `code`

## *Emphasis and `code`* in a heading

Body text with `body code`, **bold `code` inside**, *italic `code` inside*,
and a [link with `code`](https://example.com) keeps the body size.

- A list item with `item code`
  - A nested item with `nested code`
1. An ordered item: `1 + 1`

> A quote with `quoted code` that wraps at narrow widths, so that
> `the second line` keeps the body size too.

| Column | Value |
|---|---|
| `cell code` | text and `more code` |

:::tip[Run `make test` first]
A callout body with `tip code`.
:::

```sh
make test   # a fenced block keeps its own size
```

The final paragraph with `trailing code`.
