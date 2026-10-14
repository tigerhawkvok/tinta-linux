# Monaspace Argon

Tinta's default code font: GitHub Next's Monaspace, Argon (the humanist
member of the superfamily). It replaces Consolas, which only Windows ships,
in the built-in themes, the source editor and its gutter. Off Windows,
Tinta registers these files as application fonts at startup
(`platform/linux/src/dwrite_pango.cpp`); on Windows the default falls back to
Consolas unless Monaspace Argon is installed.

- Source: <https://github.com/githubnext/monaspace>, release v1.400
  (`monaspace-static-v1.400.zip`, sha256
  `ab66d71be751495f679727332a3345597943bd4d7beebca03f5cde04bf994de7`)
- Files: `Static Fonts/Monaspace Argon/MonaspaceArgon-{Regular,Italic,Bold,BoldItalic}.otf`,
  the four faces Tinta's code formats use, unmodified
- Licence: SIL Open Font License 1.1, `OFL.txt` (from the repository at
  v1.400). "Monaspace" and its subfamily names are Reserved Font Names.
