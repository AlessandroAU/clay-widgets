# Library improvement implementation

Scope accepted after the September 2026 review. Existing staged work is retained.

- [x] Correctness: clipboard, UTF-8, combo activation, capture lifetime, numeric overflow.
- [x] Overlay composition, modal focus lifecycle, keyboard menus and list navigation.
- [x] Consistent disabled scopes, read-only editing, explicit edit results and validation.
- [x] Undo/redo, word navigation, IME composition and accessibility adapter hooks.
- [x] Font IDs, configurable Unicode glyph coverage and fallback fonts.
- [x] Searchable combo and virtualized list.
- [x] Sortable/resizable table, sticky header and multi-selection.
- [x] Split panes and resizable panels.
- [x] Build entry points, supported toolchain/Clay documentation and regression coverage.

Platform adapters own native clipboard failure reporting, IME event acquisition,
and translation of semantic nodes to operating-system accessibility APIs.

Integration contracts and examples are in [integration.md](integration.md).
The existing build scripts were preserved (the review's file filter excluded
their names). The desktop wrapper gained `--test`. Linux sanitizer coverage is
configured in CI; it has not been executed locally on this Windows host.
