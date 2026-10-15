# List item spacing

A README's flag list used to run together: items sat one body line apart,
so a wrapped description looked like the next flag. Items now stand at
least half a line apart (one-line items advance 1.5 lines); an item's own
wrapped lines keep the paragraph pitch.

## Mandatory Flags

- `--input <path>`: the Markdown file to read. Relative paths resolve against the current working directory, and `-` reads standard input instead of a file.
- `--output <path>`: where to write the result.
- `--format html|docx|pdf`: the export format. **Required** whenever `--output` names a file without a recognised extension.
- `--theme <name>`: one of the built-in themes, such as *Paper* or *Wombat*, or a name from [themes.ini](#themes).
- `--strict`

## Ordered and nested

1. Configure the build with `cmake -S . -B build`.
2. Build it. This step compiles every source file and can take a while on a cold cache, so it is worth running in the background.
   - Nested bullets keep the same rule: a gap between items.
   - Their wrapped lines, like this one which goes on long enough to wrap at narrow widths, stay tight.
3. Run the tests.

## Tasks

- [x] Space list items apart
- [ ] Check the README on Windows, where the list item that wraps onto a second line should still read as one item
- [ ] Ship it

## Loose list (blank lines between items)

- A loose item is a paragraph, which already leaves space below it.

- So it is not spaced twice.

- Last loose item.

## Themes

> A list in a quote:
>
> - first quoted item
> - second quoted item, long enough to wrap onto another line at the narrow width used in the checks
> - third

| Flag | Required |
|---|---|
| `--input` | yes |
| `--strict` | no |

```sh
tinta README.md --exportpdf README.pdf
```

Last line.
