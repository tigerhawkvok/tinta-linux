# Theme editor fonts apply on save (#265)

Open the theme editor (**T**, then *Edit* on a custom theme), pick another
**Text**, **Headings** or **Code** font, and press *Save*. This page should
switch to the new fonts at once: no reopening, no Print/PDF round trip.

## Body text and emphasis

Plain body text, *italic*, **bold**, ***bold italic***, a [link](https://example.com)
and `inline code` set in the code font. 中文正文也使用主题字体。

### A third-level heading with *emphasis* and `code`

> A quoted paragraph in the text font, with `code` inside.

1. A numbered item
2. Another, with **bold** text
   - a nested bullet

| Column | Font |
| --- | --- |
| Body | Text font |
| `code` | Code font |

```python
def greet(name):
    return f"Hello, {name}"
```

Inline math $a^2 + b^2 = c^2$ stays in its own font.

#### Fourth level

##### Fifth level

###### Sixth level
