# Integration and new controls

The supported implementation toolchain is GCC or Clang in C++20 mode. The
public API has C linkage, but the implementation uses the pinned Clay build's
C++ macros and compound-literal extensions; MSVC and strict C builds are not
currently verified. Include `clay.h` and `clay-widgets/widgets.h`, defining each
implementation macro in exactly one translation unit. All UI calls run on the
same thread as the current Clay context.

The tested Clay revision is `e6cc36941ab2af5d81107617039d6f527a1c660b` (the
`subprojects/clay` gitlink). Use that revision with the amalgam too. Clay's
internal Begin/End helpers are isolated in `core.h`; arbitrary upstream Clay
versions are not interchangeable.

## Scrolling

Mouse-wheel scrolling eases to a stop by default, retaining Clay's total wheel
distance. `ui.scrollMomentumTime` controls the decay time in seconds (default
`0.10`); set it to zero for immediate scrolling. `ui.animationsEnabled = false`
also disables wheel momentum. Applications receiving native trackpad inertia
can disable this extra smoothing to avoid applying it twice.

Momentum stays with its original scroll container when the pointer moves.
Changing direction responds immediately; clicking, keyboard navigation,
opening/closing modals, or changing a scroll position programmatically cancels
pending motion. Nested widget clips forward vertical wheel input to their
enclosing scroll area. Scrollbar dragging remains direct. Clay's existing
drag-scroll momentum is controlled separately by `enableDragScroll` in BeginFrame.

## Editing

Existing TextInput/TextArea calls continue to work. Appended options add:

- `readOnly`: focus, navigation, selection and copy remain available; mutation
  is suppressed. `disabled` also prevents focus and selection.
- `history`: optional caller-owned, zero-initialized `ClayWidgets_TextHistory`.
  Give each editor its own persistent instance. It retains up to 16 snapshots
  of 8,192 bytes including the terminator by default. Tune
  `CLAY_WIDGETS_HISTORY_DEPTH` / `CLAY_WIDGETS_HISTORY_BYTES` before including
  the headers. A history instance is approximately 128 KiB: put it in application
  state or static storage, not a short-lived per-frame local. External buffer
  changes reset history. Oversized text remains editable; `historyUnavailable`
  reports that undo is unavailable. Redo is discarded after a new edit.
- `validate(text,length,userData)`: accept/reject the proposed complete buffer.
  Rejection restores text and selection. Validation uses a temporary allocation
  for rollback; allocation failure rejects editing. The callback is synchronous.
- `result`: optional `ClayWidgets_EditResult*`, overwritten each call, with
  `changed`, `submitted`, `cancelled`, `rejected`, `historyUnavailable`, and
  `clipboardFailed`. The legacy bool still means the buffer changed. Enter
  submits single-line text; Ctrl+Enter submits a text area, while Enter inserts
  a newline. Escape blurs and signals cancellation; it does not revert all edits.

```cpp
static ClayWidgets_TextHistory history{};
ClayWidgets_EditResult result{};
ClayWidgets_TextInputOptions options{};
options.history = &history;
options.result = &result;
ClayWidgets_TextInput(&ui, CLAY_ID("Name"), CLAY_STRING("Name"),
                     name, sizeof(name), options);
if (result.submitted && !result.rejected) save(name);
```

Map platform keys to `keyUndo`, `keyRedo`, and `controlDown`; Ctrl+Left/Right
and Ctrl+Backspace/Delete operate on words. Text area Ctrl+Home/End reaches
document boundaries. Word navigation treats non-ASCII codepoints as word
characters; grapheme segmentation, shaping, and bidirectional layout are not
implemented. The demo maps Ctrl+Z, Ctrl+Y, and Ctrl+Shift+Z.

Clipboard setters previously returned void. Those are still supported and are
assumed successful. An adapter that can detect failures should also install
`ClayWidgets_SetClipboardWriteFunction`; its bool setter takes precedence and
uses the clipboard user data. Cut only deletes after successful full copying.
Large selections use a temporary allocation instead of being truncated.
Buffer capacity always includes the NUL terminator. Text and callback-provided
strings must remain valid until rendering finishes.

## Overlays, focus, and disabled groups

`ClayWidgets_BeginModal` opens a fixed, centered dialog. Use
`ClayWidgets_BeginModalEx(ctx, id, title, &open, {true, 440})` for a draggable
dialog with a preferred width of 440px. Dragging starts on the title bar,
excluding its close button. Position is retained while open, constrained to the
viewport when the dialog fits, and reset to center on reopening. Drag state uses
the widget state pool; if the pool is full the dialog stays centered. Both forms
use `ClayWidgets_EndModal`, trap focus, and close via Escape, the scrim, or X.

Modal, menu, and combo layers are relative to their enclosing overlay. Keep
Begin/End pairs balanced. Modal nesting and overlay nesting are bounded at 16;
a Begin that cannot open returns false. Modal initial focus goes to its close
button; call `ClayWidgets_RequestFocus` with a body control's ID to choose another
control. Closing restores the opener's focus. Layout/hit testing and semantic
bounds use the previous frame's geometry; a newly opened overlay settles on the
next frame. Prefer opening a modal from a consumed widget action.

Enter and Space activate controls. Menus support Up/Down, Home/End, Left/Right
between menu-bar titles, Enter/Space selection, and Escape. Lists support
Home/End and timed prefix search (ASCII case folding, exact UTF-8 otherwise).
Tab-focused controls and keyboard-selected list items are revealed in their
enclosing scroll panel.

Use `if (ClayWidgets_BeginDisabled(&ui)) { ...; ClayWidgets_EndDisabled(&ui); }`
to make an entire group inert and muted, including widgets without an individual
disabled option. It is nestable and always paired when Begin returns true.

`ClayWidgets_BeginTable` now returns bool. Only emit rows and call EndTable when
it returns true. Valid existing calls that ignore its return keep working, but
new code should check it. Nested basic tables restore the enclosing column state.

## Collections

`ClayWidgets_VirtualList` takes an item count, a label callback, an application
selection index, and fixed viewport/row heights. It emits only visible rows plus
overscan. Normal rendering is proportional to the viewport size, not item count;
type-ahead searches labels and can visit all items. Empty lists are supported.

`ClayWidgets_SearchableCombo` combines an always-visible search field with a
dropdown. Pass a persistent query buffer and the original item array. Filtering
uses substring matching with ASCII case folding, and the returned selection
always indexes the original array. Filtering uses temporary descriptor/index
arrays; label bytes remain caller-owned. It is intended for moderate option
counts; use VirtualList for very large datasets.

`ClayWidgets_DataTable` takes column descriptors, a cell callback, an optional
comparison callback, an `order` permutation and `selection` array, persistent
`ClayWidgets_TableState`, and viewport options. Initialize `order[i]=i`, clear
selection, and zero-initialize state. Both arrays have `rowCount` entries.

- Clicking a header sorts ascending, then toggles direction. Sorting is in-place
  O(n log n), with original row index breaking ties. `sortChanged` identifies the
  new sort column/direction. Selection remains indexed by original record.
- Drag header dividers to resize columns. Widths live in `state.widths`; zero
  initializes from column sizing. Grow columns share the remaining viewport width
  until manually resized; explicit widths stay fixed. `widthChanged` reports resizing.
- The header sits outside the vertically scrolling, virtualized body.
- Click selects one row, Ctrl-click toggles, Shift-click selects an anchored
  range, and Ctrl+A selects all. Arrow/Home/End navigation reveals the row.
- `selectionChanged`, `sortChanged`, and `widthChanged` reset each call.
- Up to 12 columns; cells return borrowed strings valid through rendering.
  Arrays must remain a valid permutation/selection when records change; the app
  owns insertion/deletion and re-applies sorting after data changes if desired.

## Panels

```cpp
ClayWidgets_SplitOptions split{};
split.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(240)};
split.minFirst = split.minSecond = 60;
if (ClayWidgets_BeginSplit(&ui, CLAY_ID("Panes"), &ratio, split)) {
    drawFirstPane();
    ClayWidgets_NextSplit(&ui, CLAY_ID("Panes"), split.vertical);
    drawSecondPane();
    ClayWidgets_EndSplit(&ui);
}
```

`ratio` is caller-owned. The divider accepts drag, arrows, and Home/End. A
vertical split stacks the panes. Give the split a definite size along its split
axis; minimum sizes are best-effort if their sum exceeds available space.

`BeginResizablePanel` takes caller-owned dimensions and min/max constraints.
Its corner handle supports pointer dragging and keyboard arrows. Call
`EndResizablePanel` after a successful Begin. These are layout panels, not
native movable windows or docking containers.

## Fonts and platform adapters

The raylib backend's `FontCache_Register(cache,id,ttfBytes,size,codepoints,count)`
registers a face by the same ID used in Clay text configuration. Byte storage
must outlive the cache; glyph lists are copied. Registered faces are searched in
registration order for missing glyphs. Measurement and drawing use the same
per-glyph fallback. Default coverage includes Latin-1; supply glyph lists and
fonts covering the scripts your app needs. Call `Clay_ResetMeasureTextCache`
after replacing fonts, and `FontCache_Unload` before closing the graphics context.
This supplies glyph fallback, not shaping or native IME event acquisition.

For IME, send transient preedit in `compositionUtf8`, `compositionLength`, and
`compositionCursor`; send committed text through `textUtf8` with preedit cleared.
Editors display preedit underlined without modifying the document. Install
`ClayWidgets_SetImeFunction` to receive caret geometry and position the platform
candidate window. The raylib demo does not acquire native composition events;
an OS/browser input adapter must supply them.

`ClayWidgets_SetSemanticFunction` receives semantic nodes during layout for
editors, buttons, checks/toggles, radio choices, sliders, menus, modals, and virtual
lists. Use `ClayWidgets_Semantic` to supply richer labels, values, and roles for
custom controls or composite rows; merge nodes by ID in the adapter. Strings are
borrowed during the callback, and bounds are from the previous frame. Start/end
the adapter's own collection around BeginFrame/EndFrame and drop unseen IDs.
`ClayWidgets_RequestFocus` lets a platform adapter request keyboard focus through
the normal modal/disabled rules. Native UI Automation, NSAccessibility, and web
accessibility trees are application-side work; this is an adapter API, not a
claim of native screen-reader support.

## Verification

`make test` and `make test-amalgam` run the frame-based regression suite.
`python tools/build.py --test` is the equivalent wrapper. The CI Linux job adds
address/undefined-behavior sanitizers; Windows builds the actual desktop demo.
The gallery includes a 10,000-row table, history-enabled editor, searchable combo,
split pane, and resizable panel. `tests/test-raylib.cpp` additionally tests font
ID routing, fallback measurement/drawing, and UTF-8 encoding in a hidden window;
run it with `make test-backend` on a machine with a graphics context.
