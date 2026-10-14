# Selawik

Microsoft's open-source, metric-compatible stand-in for Segoe UI, which
Tinta's layouts and default themes are tuned for. Off Windows, Tinta loads
these faces as application fonts and puts them first in place of Segoe UI
(`platform/linux/src/dwrite_pango.cpp`), so line breaks and widths match the
Windows build far more closely than with Noto Sans.

- Source: <https://github.com/microsoft/Selawik>, release 1.01
  (`Selawik_Release.zip`, sha256
  `3f62c51e05e3b5a1e6241cf92a371f0be2ea1183aa87b30718bbd40832a8d423`)
- Files: `selawk.ttf` (Regular), `selawksl.ttf` (Semilight), `selawkl.ttf`
  (Light), `selawksb.ttf` (Semibold), `selawkb.ttf` (Bold)
- Licence: SIL Open Font License 1.1, `OFL.txt`. "Selawik" is a Reserved
  Font Name; these files are redistributed unmodified.
- Known upstream limits: no kerning to match Segoe UI, basic hinting.
