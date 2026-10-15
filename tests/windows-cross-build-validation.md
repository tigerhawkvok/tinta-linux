# Portable Windows exe, cross-built on Linux: validation

Built on 2026-10-14 on Ubuntu 25.10, with no Windows machine and no CI.
The build uses:
- clang-cl and lld-link from LLVM 23.1.2 (llvm-mingw `20260922`, used only
  for its LLVM)
- the MSVC CRT and Windows SDK fetched by xwin 0.10.0. Philip accepted
  Microsoft's license for that download.

The toolchain and SDK are cached in `~/.cache/tinta-cross`, about 1.3 GB.

```
TINTA_ACCEPT_MSVC_LICENSE=1 platform/windows/build-windows.sh build/windows
```

The script produces `build/windows/tinta.exe` (3,617,280 bytes). The license
variable is needed only for the first SDK download.

## Why clang-cl + xwin rather than llvm-mingw

llvm-mingw (mingw-w64 headers) was tried first and stopped at 19 compile
errors:
- `_WIN32_WINNT` gating of the per-monitor DPI APIs
- no `ID2D1DeviceContext5` and an incomplete `ID2D1SvgDocument`

Even fixed, a mingw build depends on the mingw headers' workaround for COM
methods that return structs (D2D's `GetSize` and similar), and those differ
between the MSVC and Itanium ABIs. clang-cl compiles against the same SDK
headers as MSVC, with the same ABI and static CRT, so the shared code built
unchanged: 0 errors, and the Windows CMake path needed no edits beyond the
toolchain file.

## Checked

- **PE headers:** AMD64, `IMAGE_SUBSYSTEM_WINDOWS_GUI`.
- **Resources:** `IDI_ICON1` (4 sizes), `VERSIONINFO`, and the linker's
  default manifest. That manifest is the same as an MSVC build's, because
  `tinta.rc` declares none.
- **Imports are Windows DLLs only:** ADVAPI32, COMDLG32, DWrite, GDI32,
  IMM32, KERNEL32, SHELL32, SHLWAPI, USER32, WINHTTP, d2d1, d3d11, dbghelp,
  dwmapi, ole32 and urlmon. There are no VCRUNTIME, MSVCP, api-ms-win-crt or
  ucrtbase imports, so the static CRT works and the exe runs without the
  VC++ redistributable. The build script fails if a runtime DLL appears;
  its pattern was checked against the 5 runtime names and the system DLLs.
- **Wine 11.0-rc4 (staging) on a private Xvfb, in portable mode**
  (`settings.ini` beside the exe):
  - `--exporthtml` exits with 0 and writes 7,326 bytes.
  - The window opens at 992×666 and renders `tests/fixtures/gtk-window.md`
    through Wine's D2D and DirectWrite: headings, the table, lists, the
    quote and code.
  - Without the seeded settings, the first-run "set Tinta as the default
    viewer?" prompt shows (upstream behaviour) and blocks every run,
    exports included.

## Not checked (needs real Windows)

- **Rendering fidelity, PDF export and `--printpages`.** Under Wine these
  exit with 1: they need D3D11 and `ID2D1PrintControl`, which Wine does
  not provide. Wine's own D2D and DirectWrite also differ from Windows
  (substitute fonts, no bold, no tab strip in the caption).
- **The upstream test suites, built for Windows.** They were configured off
  here (`BUILD_TESTING=OFF`); building them and running them under Wine is
  possible but would only test Wine.
- **The fork's bundled fonts.** Windows still has no loader for
  `third_party/fonts`, so the default code face falls back to Consolas
  where Monaspace Argon is not installed, and UI text uses the system's
  Segoe UI.
- **Mermaid** uses the native parsers (merman is off on Windows, MM5).
