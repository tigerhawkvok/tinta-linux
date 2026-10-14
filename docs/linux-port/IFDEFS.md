# Linux changes to shared code

Shared code stays upstream-identical except for the edits listed here
(PLAN.md §2.2). Each is either an `#ifdef` island or a portability fix that
also compiles on Windows. When an upstream merge conflicts, this is the list
to check.

## Portability fixes (no `#ifdef`; Windows behaviour unchanged)

| Change | Files | Why |
|---|---|---|
| `std::ifstream`/`std::ofstream` constructed from `std::filesystem::path(wide)` instead of a `std::wstring` or `wchar_t*`; brace-initialised where the path is the only argument, to avoid the most vexing parse; `#include <filesystem>` added | annotations, drafts, editor, export_docx, export_html, i18n, input, pandoc, print, render, settings, themes (27 sites) | Opening an fstream from a wide string is an MSVC extension. `std::filesystem::path` is standard C++17: lossless on Windows and UTF-8 on Linux, whatever the locale (checked with `café-中.txt` under `LC_ALL=C`). |
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
