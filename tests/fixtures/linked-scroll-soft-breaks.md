# Linked scrolling across soft line breaks

Paragraphs here are wrapped by hand, so one paragraph spans several source
lines joined by soft line breaks. Many blocks open with a link, emphasis or
inline code that runs to the end of its first line; others are code blocks
that open with a blank or an indented line. Scroll either pane: the other
follows all the way to the end of the document.

## 1. Merging between branches

[The merge script](https://example.com/tools/merge.py)
does the merge in one pass, resolving tips since the last shared tag, and
leaves a local branch behind for inspection. A local copy sits at the root of
the repository, but the linked version is canonical and may be newer.

**The hooks run after every merge,**
so a clean merge may still leave uncommitted files behind. That is expected:
amend the merge with them, or commit them on their own, and carry on.

`git status`
should then be clean. If it is not, a hook rewrote a file based on the branch
it came from; add the change and amend, or commit it separately.

*This line ends with a hard break*\
and carries on below it, on a line of its own, before the paragraph ends with
a [plain link](https://example.com/plain/1) in the middle of the text.

- [First item link](https://example.com/items/1)
  continues on a second source line, inside the same tight list item.
- A plain second item, on one line.
- `third_item()`
  also wraps onto a second source line.

> [A quoted link](https://example.com/quotes/1)
> opens this quote, and the quote carries on for a second and a third line
> before it ends.

```text

The blank line above opens this block.
entry 01-1: no change, wait
entry 01-2: no change, wait
```

## 2. Keeping a field log

[The field notes](https://example.com/notes/2)
collect years of entries, and the pattern only shows once the pages are side
by side, which is why the old hands keep paper copies even now. Read them
slowly; the margins carry as much as the entries.

*Patience is the whole craft,*
and the people who last are the ones who write things down: the date, the
temperature, what the equipment was doing, and what they changed afterwards.

```python
    indented_first_line = True
def after():
    return "the first line of this block is indented"
```

| Step | Command | Note |
|---|---|---|
| 1 | `git switch -c topic` | branch from the target |
| 2 | `git merge --squash source` | one commit |
| 3 | `git commit --amend` | fold in hook changes |

1. [Create a topic branch](https://example.com/steps/1)
   from the tip of the destination branch.
2. Squash-merge the source branch into it.
3. **Bump the version**
   and rebuild before opening the request.

A closing paragraph for this section, written on a single long source line so that the editor wraps it when wrapping is on, and the preview renders it as one block.

## 3. Sourdough starters

[Levain basics](https://example.com/bread/3)
start with flour, water and time; the first season is mostly watching, with a
notebook, how rye flour behaves when the weather turns.

**A good log reads like a list of small changes,**
one at a time, each given a full week before judging it; two changes at once
teach nothing, because afterwards there is no telling which one mattered.

`entry 03: no change, wait`
over and over, until one day it does not. Then the pages earn their keep, and
the crossings-out carry more than the entries.

> *Quoted emphasis at the end of a line*
> keeps the quote together across its source lines, and the preview keeps up
> with the source as it scrolls past.

```

    The blank line above, and this indented line, open this block.
entry 03-1: proofing basket cleaned
entry 03-2: no change, wait
```

- **Bold item opener**
  carries on below.
- [Linked item opener](https://example.com/items/3)
  carries on below as well.

## 4. Canal locks

[Lock keeping](https://example.com/locks/4)
starts at the lower gate, where the work is quiet and slow and nobody hurries;
the first season is mostly watching how the paddle gear behaves.

*The balance beam needs grease in winter,*
and the gate needs a firm push in spring, when the water runs high and the
paddles stick in their slides.

| Week | Paddle gear | Balance beam | Note |
|---|---:|:---:|---|
| 1 | 13 | yes | steady |
| 2 | 23 | no | steady |
| 3 | 33 | yes | watch it |

`lock_log.append(entry)`
is all the code there is; the rest is walking the towpath and writing down
what the water did overnight.

```yaml

lock: 4
gates:
  - lower
  - upper
```

## 5. Letterpress

[The composing stick](https://example.com/press/5)
holds one line of type at a time, set by hand, letter by letter, with spacing
between words judged by eye and corrected after the first proof.

**Leading goes between the lines,**
and the chase and quoins lock the whole forme in place before it goes on the
press. Too tight and the type lifts; too loose and it falls out.

> [A quote from the shop floor](https://example.com/press/quote)
> says the chase and quoins will tell you what they need, if you are there
> early enough to hear it, and quiet enough to listen.

```text
    indented: the forme, locked
not indented: the proof, pulled
```

## End of the soft-break fixture

[The last link](https://example.com/end)
opens the last paragraph. Both panes reach it together.
