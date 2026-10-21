# Code block copy (#269)

Each block's **Copy** button copies its lines *without* the line break that
ends the last one, as [GitHub's copy button](https://github.com) does. Paste
each into a text editor: the caret stays at the end of the last line.

## Single line (the report)

```text
single line
```

## Several lines

```cpp
int main() {
    return 0;
}
```

## A blank line before the closing fence stays

```text
line

```

## Tilde fence

~~~python
print("tilde")
~~~

## Inside a list

1. First step
   ```bash
   echo "in a list"
   ```
2. Second step, with `inline code` after the block

## Inside a quote

> A quoted block:
>
> ```json
> {"quoted": true}
> ```

## Four-backtick fence: inner fences and trailing spaces stay

````markdown
```js
let x = 1;   
```
````

| Language | Lines | Copied text ends with |
| --- | --- | --- |
| text | 1 | `line` |
| cpp | 3 | `}` |
| text, blank line kept | 2 | an empty line |

Inline math such as $x^2 + y^2$ and ~~struck~~ text follow the blocks.

## Done
