# Pasting screenshots: validation

Requested 2026-10-15. Philip chose option D: "a multi option on each paste
(with a rememberable default and corresponding setting)". It replaces the
old behaviour, which always saved the PNG beside the document.

## Design

- **When.** Ctrl+V in the editor with an image on the clipboard and **no
  text**. Text always wins, because spreadsheets and browsers offer both.
- **The card.** It uses the prompt-chip style of the create-reference
  dialog, in the bottom-right corner. It shows the title, the path the
  assets folder would use, and these choices:
  - **Assets folder:** `<doc>.assets/<doc>-img-NN.png`, using the next free
    number.
  - **Inline:** `data:image/png;base64,…`, with nothing written to disk.
  - **Save as…:** the save dialog, pre-filled with the assets name. It
    opens in that folder, or in the document's folder while the assets
    folder doesn't exist yet.
  - **Remember:** a tick box (`R`).
  - Keys: `A`, `I`, `S`, `R`; Enter takes the first choice; Esc, the
    cross, or a press outside the card cancels.
- **An untitled note** has no folder, so its card offers Inline and
  Save as… only. A remembered "assets" setting still asks there.
- **Links.**
  - Relative to the document where possible; otherwise absolute.
  - In angle brackets (`![image](<my notes.assets/my notes-img-01.png>)`)
    when the path holds a space or a parenthesis. CommonMark allows both
    there. The reader takes paths as written, so a `%20` would not
    resolve.
- **The setting.** `pasteImage=ask|assets|inline|off` in `settings.ini`.
  Settings → Editor → *Pasted images* sets it with a segmented control
  (Ask / Folder / Inline / Off). Remember sets it too, and so does the
  segment. Both persist immediately, like the other editor switches. `off`
  shows a hint and pastes nothing.
- **Modal.** While the card is up it owns the keyboard. A press anywhere
  else, the tab strip included, cancels it and does nothing more. A tab
  switch by any route drops it, so a choice can never land in another
  document.
- **Clipboard.**
  - Windows reads the registered "PNG" format (Snipping Tool, browsers),
    else `CF_BITMAP` encoded through WIC (PrtScn).
  - Linux reads another application's image through GTK
    (`gdk_clipboard_read_texture_async`, saved as PNG) and serves it to
    shared code as the "PNG" format.

## Fixed on the way

- **The reader showed no `data:` images.** `getOrLoadImage` treated every
  source as a path. `imageDataUriBytes` and `decodeImageMemory`
  (image_loader.cpp) now decode base64 `data:image/*` in memory, so an
  inline paste shows in Tinta itself.
- **Word export** used to write an inline image as alt text plus a
  hyperlink carrying its whole base64. It now embeds the bytes as a
  picture. An unsupported type (webp, for example) names only the URI's
  header. The HTML export already passed `data:` sources through.
- **Linux `swprintf_s`** (msvc_crt.h) passed MSVC formats straight to
  glibc. There `%s` means a narrow string, so wide arguments printed only
  their first character:
  - the editor's word count read "2 words · 1 characters" for 202 and
    1,201;
  - Pandoc's temp file became `/tinta-pandoc-NNN.` at the filesystem root.

  The shim now translates MSVC's wide-format meaning: `%s`/`%c` are wide,
  and `%hs`/`%hc`/`%S`/`%C` narrow. `languages.ini` strings written for
  Windows therefore format correctly too.

## Checked

- **`paste_image`** (`tests/paste_image_tests.cpp`, ctest). It drives the
  real key and mouse handlers against a headless window. Settings live in
  a scratch `XDG_CONFIG_HOME`, and the test first checks they land there.
  - **The fixture:** the reader loads all three of its link forms (assets
    folder, inline, angle brackets with spaces and parentheses).
  - **Exports:** HTML inlines all three. Word draws three pictures,
    includes the inline one's PNG bytes, and carries no base64.
  - **The card:** it offers its five hits, inside the window and the card,
    at 900 and 420 px.
  - **A:** writes `paste notes.assets/paste notes-img-01.png` byte for
    byte, links it in angle brackets, swallows the chosen key's character,
    and the reader loads the result. Ctrl+Z takes the link back.
  - **Cancelling:** Esc cancels, a click on the card's blank space keeps
    it, and a press outside cancels without opening anything.
  - **Remember:** Remember plus the assets button writes img-02 and
    persists `assets`; the next paste goes straight to img-03.
  - **Settings segments:** they sit inside the panel at 640 and 900 px.
    Ask, Inline and Off each persist.
  - **Modes:** inline embeds the exact base64 and the reader shows it; off
    pastes nothing; text beside an image pastes as text.
  - **Untitled note:** no assets choice, `A` does nothing, Enter takes
    Inline. An image gone from the clipboard by the time of the choice
    pastes nothing.
- **`compat_crt`** (`platform/linux/tests/crt_tests.cpp`): `%s`, `%ls`,
  `%hs`, `%S`, `%c`/`%hc`/`%C`, flags, star width and precision, numbers,
  `%%`, and the Pandoc temp-file format.
- **Mutations**, each failing its test:
  - `data:` decoding off in the reader: 2 checks;
  - the Word `data:` route off: 2 checks;
  - the `swprintf_s` translation off: 6 checks, among them the Pandoc path
    coming out as `/tinta-pandoc-42.`.
- **Linux GUI**, under Xvfb with its own D-Bus session over X11. Another
  process owned a real `image/png` clipboard: a screenshot of the Tinta
  window, 1100 × 800. 9/9 checks passed.
  - The card at 1100 and 560 px, then Remember ticked.
  - `A` saved a 1100 × 800 PNG with the angle-bracket link, and the
    preview showed the screenshot. The next paste went straight to img-02,
    and `pasteImage=assets` was written.
  - Text plus image pasted the text.
  - Untitled note, dark theme: the card offered Inline and Save as… with
    "Save the note to use an assets folder". Enter embedded the
    screenshot, and the preview rendered it.
  - Save as… opened the portal dialog showing `paste notes-img-01.png` in
    the document's folder; Enter saved it and linked it relative.
  - Settings → Editor → *Pasted images*: light and dark at 1100 px, and
    700 px; German at 1100 and 700 px. Segments now size to their labels.
    Equal segments as wide as "Eingebettet" had left the German card
    title as "…" at 700 px.
- **Windows exe under Wine.** A small Windows program owned the clipboard.
  Wine's X11 bridge maps neither `image/png` nor PNG to a Windows format,
  so an X-side owner can't stand in.
  - **"PNG" (Snipping Tool's format):** card, `A`, the saved PNG equal to
    the source pixel for pixel, and the angle-bracket link.
  - **`CF_DIB` (PrtScn), read through `CF_BITMAP` and WIC:** the same,
    also pixel-identical (largest channel difference 0, so neither flipped
    nor channel-swapped).
  - **The fixture:** the reader shows the inline image. The HTML export
    inlines three images; the DOCX has three pictures and no base64.
  - The PDF export and page render write nothing under Wine. Earlier Wine
    runs did the same, before this change.

## Not checked

- A real Windows desktop: Win+Shift+S, PrtScn, and paste from a browser.
- Pandoc export on Linux end to end. Pandoc isn't installed here; the temp
  path's format is covered by `compat_crt`.
- Editor responsiveness with a very large inline image: unmeasured. The
  untitled run inlined a 1100 × 800 screenshot as one long line and stayed
  responsive to the next key, but nothing was timed.
