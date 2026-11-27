# Emacs keys

Pick **Settings → General → Keyboard shortcuts → Emacs**, then try the
chords below. In the reader, `C-n`/`C-p` scroll by a line, `C-v`/`M-v` by a
page and `M-<`/`M->` jump to either end. `C-s` searches; pressed again it
steps to the next match, `C-r` to the previous one, and `C-g` closes it.

## Reading

| Chord | In the reader |
|---|---|
| `C-n` / `C-p` | Scroll a line down / up |
| `C-v` / `M-v` | Scroll a page down / up |
| `M-<` / `M->` | Top / bottom |
| `C-s` / `C-r` | Search, then next / previous match |
| `C-x h`, `M-w` | Select everything, copy the selection |
| `C-x C-q` | Edit this document |
| `C-g` | Close the search, a panel or a menu; never the window |

> `C-x` waits for the next key, as in Emacs: the hint at the bottom right
> says `C-x-` until it arrives. An unknown follow-up is reported, not typed.

## Editing

Press `C-x C-q` here to edit, then:

1. Move with `C-f`, `C-b`, `C-n`, `C-p`, `C-a`, `C-e`, and by words with
   `M-f` and `M-b`.
2. Set the mark with `C-SPC` and move: the region grows from it.
   - `M-w` copies the region, `C-w` cuts it, `C-y` yanks it back.
   - `C-x C-x` swaps the caret and the mark.
3. `C-k` kills to the end of the line; a second `C-k` takes the line break
   too, and both land together in the clipboard.
4. `M-d` and `M-DEL` kill a word forward and back.
5. `C-d` deletes a character, `C-t` swaps two, `C-o` opens a line.
6. `C-/` undoes, `C-?` redoes, and so does `C-x u` (undo).
7. `C-x C-s` saves, `C-x C-w` saves under a new name, `C-x C-q` goes back
   to reading.

```text
Kill this line with C-k, then yank it back with C-y.
transpose: ab -> put the caret between a and b and press C-t
```

## Still Windows

Chords Emacs does not define keep their usual meaning: `Ctrl+C` copies,
`Ctrl+Z` undoes, `Ctrl+T` opens a tab, `Ctrl+Tab` cycles tabs. Single keys
(`F`, `J`, `K`, `E`, `?`) work in the reader as in the Windows profile.

---

Padding so the page scrolls: Lorem ipsum dolor sit amet, consectetur
adipiscing elit. Integer nec odio. Praesent libero. Sed cursus ante dapibus
diam. Sed nisi. Nulla quis sem at nibh elementum imperdiet. Duis sagittis
ipsum. Praesent mauris. Fusce nec tellus sed augue semper porta. Mauris
massa. Vestibulum lacinia arcu eget nulla.

Class aptent taciti sociosqu ad litora torquent per conubia nostra, per
inceptos himenaeos. Curabitur sodales ligula in libero. Sed dignissim
lacinia nunc. Curabitur tortor. Pellentesque nibh. Aenean quam. In
scelerisque sem at dolor. Maecenas mattis. Sed convallis tristique sem.
