# Bundled fonts

These fonts are redistributed under their own licenses, separate from this
project's LICENSE:

- `Roboto-Regular.ttf` - Apache License 2.0 (Google,
  https://fonts.google.com/specimen/Roboto). This is the demo's UI font; it is
  baked into the binary via `tools/embed_font.py` ->
  `assets/generated/embedded-font.h`, so the license also covers that
  generated header.
- `OpenSans-Regular.ttf` - SIL Open Font License 1.1 (
  https://fonts.google.com/specimen/Open+Sans).

If you swap or update a font, verify the license shipped with that specific
release and update this note.
