# The default code face: Monaspace Argon

Code now uses **Monaspace Argon**, an OFL font Tinta ships, instead of
Consolas, which only Windows has. Inline `code`, **bold `code`**, *italic
`code`* and ***bold italic `code`*** all use it, as does a
[link with `code`](https://monaspace.githubnext.com/).

## Columns line up `|` in a heading

```text
mmmm  iiii  WWWW  ....  ____
1lIi  0Oo   rn m  cl d  ,.;:
=> -> != == === >= <= :: !== ===
|----|----|----|
| ab | cd | ef |
```

Ligature-prone operators stay as separate glyphs: `=>`, `->`, `!=`, `===`.

## Highlighted code

```python
def column(width: int, text: str) -> str:
    """Pad to a fixed column; every glyph has the same advance."""
    return f"{text:<{width}}|"  # 'mmm' and 'iii' end at the same column
```

## Characters Argon does not have

```text
Box:   ┌──┬──┐  └──┴──┘
CJK:   中文 日本語 한국어
Emoji: 😀 ✓
```

| Face | Sample |
|---|---|
| default | `fn main() { println!("hi"); }` |
| **bold** | **`let x = 42;`** |

> A quote with `inline code` and a long enough line to wrap at narrow widths
> so that `the second line` keeps the code face and its baseline.

The final paragraph with `trailing code`.
