# Editor zoom with word wrap (#266)

Open this file, press `:` to edit, turn on word wrap, then hold Ctrl and
turn the wheel over the source. No line may run into the next one at any
zoom level, in either direction.

滚轮放大、缩小字号时，编辑区的每一行都应该保持在自己的位置上，不会和下一行重叠。这一段故意写得很长，在窄窗口和大字号下会折成好几行，用来检查折行数在缩放之后是否重新计算，而不是沿用缩放前的行数。

A long English paragraph follows, long enough to wrap onto several rows at
every width once joined: the quick brown fox jumps over the lazy dog, again and again, while the editor lays out each logical line as one or more visual rows and must keep the rows it reserves for this line in step with the font size the line is drawn at, including after the zoom changes that size.

- A list item with **bold** and *italic* text that is also long enough to wrap when the window is narrow and the text is large.
- 第二个列表项：中文和 English mixed in one line, with `inline code` and a [link](https://example.com) that keeps going past the edge of the pane.

> A quote that wraps: zooming in makes every character wider, so this line needs more rows than it did before, and the line below it must move down to make room.

| Column | Text |
| --- | --- |
| short | 一个很长的表格单元格，里面的文字在编辑区会折成多行，缩放之后也一样 |

```text
A code line that is deliberately long, so the source wraps it too when word wrap is on and the zoom is high enough.
```

Inline math $e^{i\pi} + 1 = 0$ and a final short line.

Last line.
