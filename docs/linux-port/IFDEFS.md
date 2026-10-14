# Linux changes to shared code

A map of where the Linux port touches shared code, for orientation. Since
the fork went hard (2026-10-14, PLAN.md §8), shared code may change freely;
this list records what changed and why. It is not a diff budget.

## Portability fixes (no `#ifdef`; Windows behaviour unchanged)

| Change | Files | Why |
|---|---|---|
| Every Tinta wide path entering `std::filesystem` or an fstream goes through `fsPath(wide)` (new `include/fs_path.h`): fstream constructions (27 sites, brace-initialised where the path is the only argument, to avoid the most vexing parse), `std::filesystem::path`/`fs::path` constructions and declarations, and one `wstring` assignment in search.cpp | annotations, drafts, editor, export_docx, export_html, i18n, inline_style, input, pandoc, print, render, search, settings, themes | Opening an fstream from a wide string is an MSVC extension. `std::filesystem::path` is standard C++17: lossless on Windows and UTF-8 on Linux, whatever the locale (checked with `café-中.txt` under `LC_ALL=C`). Shared code joins paths with `\`, so `fsPath` also turns `\` into `/` off Windows. That one `#ifdef` lives in `fs_path.h`; on Windows `fsPath` is the plain conversion. |
| `isLiveTintaProcess` splits the image path on `\` or `/` and accepts `tinta` as well as `tinta.exe` | drafts.cpp | Drafts carry their owner's pid; a draft whose owner is a running Tinta is a live autosave, not a crash leftover. Linux executables have no `.exe` and use `/`. |
| Command-line arguments starting with `/` are files when they exist, switches otherwise; `ifstream` of the startup file goes through `fsPath` | main_d2d.cpp (`WinMain`) | Linux absolute paths start with `/`. On Windows `/register` is matched first and no other slash switch exists as a file, so nothing changes there. |
| Inline code sits on the surrounding text's baseline (measured per format) instead of being centred by `1.2 × font size`, and its chip hugs the code's line box with a padding of 0.2 × code size, inside the text line | render.cpp (`layoutInlineContent`) | Fixes #262 on every platform: with Noto Sans and MonoLisa the old centring put code 2.9 px low at 100% (4.4 px at 150%). |
| Default code face `kDefaultCodeFont` = Monaspace Argon (bundled, OFL) instead of Consolas; `installedCodeFont` settles every code format on an installed face (the theme's, else Argon, else Consolas); `App::uiCodeFont` for interface mono text | app.h, themes.cpp, d2d_init.cpp/.h, editrail.cpp, overlays.cpp | PLAN.md §10.11. A theme naming a missing face no longer lands in a proportional fallback. |
| `mermaid::stripPreamble`, `flowchart-elk` | mermaid.h, mermaid.cpp, mermaid_ext.cpp | A Mermaid compatibility fix, not a Linux one (`7c4ed53`). |

## `#ifdef` islands

None yet. Phase 2 adds the window entry point, title bar and IME.

## Absorbed by the compat layer instead of shared code

- MSVC CRT spellings (`_stricmp`, `swprintf_s`, `sscanf_s`, `_countof`, ...)
  and MSVC's transitive standard includes. These are force-included through
  `platform/linux/include/tinta_prelude.h`.
- `_wcsicmp` on `std::filesystem::path::c_str()`, which is `char` on Linux
  (`search.cpp`): a `char` overload.
- `->DrawTextW(...)` on Direct2D targets (`editor.cpp`): on Windows the
  `DrawText` macro renames the method too, so the compat header provides
  both names.
- `\` in paths built by shared code (for example `outDir + L"\\page-1.png"`):
  the compat file functions accept either separator, as Windows does.
