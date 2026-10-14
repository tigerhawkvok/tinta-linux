# Linux changes to shared code

Shared code stays upstream-identical except for the edits listed here
(PLAN.md §2.2). Each is either an `#ifdef` island or a portability fix that
also compiles on Windows. When an upstream merge conflicts, this is the list
to check.

## Portability fixes (no `#ifdef`; Windows behaviour unchanged)

| Change | Files | Why |
|---|---|---|
| Every Tinta wide path entering `std::filesystem` or an fstream goes through `fsPath(wide)` (new `include/fs_path.h`): fstream constructions (27 sites, brace-initialised where the path is the only argument, to avoid the most vexing parse), `std::filesystem::path`/`fs::path` constructions and declarations, and one `wstring` assignment in search.cpp | annotations, drafts, editor, export_docx, export_html, i18n, inline_style, input, pandoc, print, render, search, settings, themes | Opening an fstream from a wide string is an MSVC extension. `std::filesystem::path` is standard C++17: lossless on Windows and UTF-8 on Linux, whatever the locale (checked with `café-中.txt` under `LC_ALL=C`). Shared code joins paths with `\`, so `fsPath` also turns `\` into `/` off Windows. That one `#ifdef` lives in `fs_path.h`; on Windows `fsPath` is the plain conversion. |
| `isLiveTintaProcess` splits the image path on `\` or `/` and accepts `tinta` as well as `tinta.exe` | drafts.cpp | Drafts carry their owner's pid; a draft whose owner is a running Tinta is a live autosave, not a crash leftover. Linux executables have no `.exe` and use `/`. |
| Command-line arguments starting with `/` are files when they exist, switches otherwise; `ifstream` of the startup file goes through `fsPath` | main_d2d.cpp (`WinMain`) | Linux absolute paths start with `/`. On Windows `/register` is matched first and no other slash switch exists as a file, so nothing changes there. |
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
