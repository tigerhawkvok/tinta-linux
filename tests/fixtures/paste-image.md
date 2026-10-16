# Pasting screenshots

Copy a screenshot, then press **Ctrl+V** in the editor. When the clipboard
holds an image and no text, Tinta asks where the image should go. Your
choice can be remembered, and *Settings › Editor › Pasted images* changes it
later. See the [README](../../README.md) for the other editor shortcuts.

| Choice | Writes | Link |
|---|---|---|
| Assets folder | `<doc>.assets/<doc>-img-NN.png` beside the document | relative |
| Inline | nothing on disk | `data:image/png;base64,…` |
| Save as… | wherever you pick | relative when inside the document's folder |
| Off | nothing; a hint says pasting images is off | — |

Keys while the card is open:

1. **A** for the assets folder, **I** for inline, **S** for Save as…
2. **R** ticks *Remember*, **Enter** takes the first choice
3. **Esc**, or a click outside the card, cancels

> An untitled note has no folder yet, so the card offers *Inline* and
> *Save as…* only. Save the note first to use an assets folder.

## Already pasted

From the assets folder, linked relative to this file:

![image](paste-image.assets/paste-image-img-01.png)

Inline, with the bytes in the link itself:

![image](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAKAAAAA8CAIAAABuCSZCAAAA4UlEQVR42u3asQ2DAAADwd8k21JkUuqIAdggtMg6yfUVrr/P93jcef0ex3mnkxe2nbyw7eSFbScvbDt5YdvJC9tOXth28sK2kxe2nbyw7eSFbScvbDt5YdvJC9tOXth28sK2kxe2nbyw7eSFbScvbDt5YdvJC9tOXth28oIu2pu6aG/qojm6aI4umqOL5uiiddHe1EV7UxfN0UVzdNEcXTRHF62L9qYu2pu6aI4umqOL5uiiObpoji5aF+1NXbQ3ddEcXTRHF83RRXN00bpob+qivamL5uiiObpoji6a89e5AX5S34sAHz+kAAAAAElFTkSuQmCC)

A file whose name holds spaces and parentheses, in angle brackets:

![image](<paste-image.assets/screen shot (2).png>)

The links Tinta writes:

```markdown
![image](paste-image.assets/paste-image-img-02.png)
![image](<my notes.assets/my notes-img-01.png>)
![image](data:image/png;base64,iVBORw0KGgo...)
```

- Text on the clipboard always wins: copying a cell from a spreadsheet that
  also offers a picture pastes the text.
- `pasteImage=ask|assets|inline|off` in `settings.ini` holds the choice.
