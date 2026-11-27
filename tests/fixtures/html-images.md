# Images written in HTML

READMEs often place badges and logos with HTML rather than Markdown images.
Tinta Mono draws them at the size their `width` and `height` give, and a
badge inside a link opens the link.

## A badge inside a link, on its own line

<a href="https://example.com/redirect?r=app://add/%7B%22id%22%3A%22org.example.app%22%2C%22name%22%3A%22Example%22%7D"><img src="html-images.assets/badge.png" alt="Get it on Example" width="200" height="60"></a>

The paragraph after the badge, with **bold**, `code` and a
[Markdown link](https://example.com/markdown-link).

## An image in a sentence

Text before <img src="html-images.assets/badge.png" alt="small badge" height="24"> and after it.

A height alone keeps the image's proportions, and so does a width alone.

## A Markdown badge inside a link

[![Markdown badge](html-images.assets/badge.png)](https://example.com/markdown-badge)

## An HTML block

<p align="center">
  <img src="html-images.assets/badge.png" alt="A centred logo" width="120">
</p>

## Links written in HTML

An <a href="https://example.com/inline">inline HTML link</a> reads like a
[Markdown one](https://example.com/md), and an
<a id="target-here">anchor without an href</a> stays plain text.

- A list item with <a href="https://example.com/list">an HTML link</a>
- A list item with <img src="html-images.assets/badge.png" alt="list badge" width="100">

> A quote with <a href="https://example.com/quote">a link in HTML</a>.

| Column | Value |
|---|---|
| <img src="html-images.assets/badge.png" alt="cell badge" width="80"> | text |

## When the image is missing

<img src="html-images.assets/missing.png" alt="the project's badge">

The final paragraph.
