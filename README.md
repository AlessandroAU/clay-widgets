# clay-widgets

Widget layer built on top of Clay with a raylib demo application.

## Project layout

- `clay-widgets/widgets.h`: umbrella header for the widget library built on Clay.
- `clay-widgets/`: structured header tree with shared core helpers plus one header per widget.
- `clay-widgets/themes.h`: theme preset header used by the widget layer and demo.
- `main.cpp`: demo app showing all currently implemented widgets.
- `Makefile`: builds raylib from source in `subprojects/raylib` and then builds the demo.
- `subprojects/raylib`: vendored raylib source (git clone).
- `subprojects/clay`: vendored Clay source (git clone, used for `clay.h`).

## Widgets currently demonstrated

- Label
- Heading
- Separator
- Image / Icon (tintable textures)
- Button
- Checkbox
- Toggle / switch
- Radio buttons
- Tabs (pill and attached styles)
- Slider
- Progress bar
- Text input
- Combo box (dropdown select)
- List box (selectable, keyboard-navigable list)
- Selectable list row (color swatch + label + trailing text)
- Segmented control (joined single-select buttons)
- Stepper / number input (+/- with min/max/step)
- Badge / chip / tag (status pills)
- Menu bar with drop-down menus
- Right-click context menu
- Tooltip (hover, delayed)
- Toast / notification (transient, auto-dismissing)
- Modal dialog (dimming scrim)
- Card / group box (titled bordered container)
- Collapsible / accordion section
- Tree view (hierarchical expand/collapse)
- Table / data grid (columns, header, zebra rows, selection)
- Scroll panel with draggable scroll bar

## Interaction behavior

- Pointer interaction for all controls.
- Keyboard focus traversal with `Tab` across interactive widgets.
- Keyboard activation with `Enter` for buttons, checkboxes, and radio buttons.
- Focused text input supports typing, `Backspace`, `Delete` to clear, and `Escape` to blur.

## Animations

Widgets animate their state changes using Clay's built-in transition engine
(`Clay_EaseOut`), so no extra per-frame bookkeeping is needed - a widget just
declares a `.transition` on its element and keeps setting its target color as
usual. Two kinds are used:

- **Color transitions** - hover / selected / pressed background colors ease in
  over a short curve instead of snapping. Wired into buttons, list/table/tree
  rows, tabs, segmented cells, menu items and combo triggers/items, and the
  toggle track. (Checkbox/radio boxes keep a constant fill, so they are not
  animated here.)
- **Enter transition** - the modal scrim's dimming overlay fades in when a modal
  opens, while the dialog stays crisp and remains clickable during the fade.

Some effects aren't a property of a single element and so can't be a Clay
transition - the toggle knob *sliding* across its track is the canonical case.
For these the library keeps a small per-widget eased scalar keyed by element id
(`ClayWidgets__AnimTo`), advanced each frame with frame-rate-independent
exponential smoothing. The toggle drives that 0..1 value into its knob position;
`animationsEnabled` and reduce-motion apply here too (the value snaps to its
target). This is the pattern to reuse for future motion like an accordion's
height or a tab underline.

Notes and knobs:

- `ctx->animationsEnabled` (default `true`) globally turns transitions off, which
  makes them snap instantly. Set it to `false` to honor a reduce-motion
  preference or to get deterministic frames for screenshot testing.
- Durations are compile-time tunables: `CLAY_WIDGETS_ANIM_HOVER_DURATION`
  (default `0.12f`) and `CLAY_WIDGETS_ANIM_ENTER_DURATION` (default `0.18f`).
- Clay transitions apply to the element they are set on and do **not** cascade to
  child elements, so fade-ins are only used on solid overlays (the scrim) whose
  visual is their own fill - not on panels that host crisp child text. Dropdown
  and toast enter/exit animations would need per-widget opacity handling instead.

## Demo coverage

The demo is organized like a small application, with four views:

- **Dashboard** (0): live task statistics in stat cards, a completion progress
  bar, a selectable data table, and quick-action buttons in all variants
  (default, primary, danger, disabled).
- **Tasks** (1): a working to-do manager - add tasks with Enter or a primary
  button, filter with a segmented control, select rows (the list-row widget
  with priority swatches and trailing text), edit the selected task in place,
  and delete via a confirming modal or right-click context menu.
- **Gallery** (2): the full widget catalog grouped into cards - buttons, text
  and choice inputs, ranges, attached tabs, toggles/checks, tree, icons,
  badges, and overlay demos (toasts, modal, context menu, tooltips).
- **Settings** (3): theme preset switching (Slate, Sand, Forest, Windows), an
  animations toggle, a notifications toggle that gates the demo's toasts, and
  a live diagnostics card (focused widget id, pointer, fps).

## MSYS2 setup (recommended)

Open the `MSYS2 MinGW 64-bit` shell and install toolchain dependencies:

```bash
pacman -Syu
pacman -S --needed mingw-w64-x86_64-toolchain make git
```

## Build

From the project root:

```bash
python build.py
```

or directly with make:

```bash
mingw32-make
```

This does two things:

1. Builds raylib static library from `subprojects/raylib/src`.
2. Builds `clay-widgets-demo.exe`.

## Run

```bash
mingw32-make run
```

or launch `./clay-widgets-demo.exe` directly.

## Web build (WebAssembly + canvas)

The same raylib app can be compiled to WebAssembly and run in a browser on an
HTML `<canvas>`, so the web page looks identical to the desktop demo. This uses
[Emscripten](https://emscripten.org/); `build-web.py` installs it for you.

From the project root:

```bash
python build-web.py           # installs emsdk (first run, ~1GB) + builds
python build-web.py --serve   # build, then serve at http://localhost:8000
```

This produces `web/index.html` (plus `index.js`, `index.wasm`, `index.data`).
The output must be served over HTTP - browsers won't `fetch` the `.wasm`/`.data`
from a `file://` URL. Use `--serve`, or point any static server at `web/`.

Other flags:

- `--clean` removes `web/` and the web raylib lib before building.
- `--skip-raylib` reuses an existing `libraylib.web.a` (skips the raylib rebuild).
- `--emsdk-version X` pins a specific Emscripten SDK version (default `latest`).

Notes:

- The web build is entirely separate from the desktop one: raylib for web is
  archived as `libraylib.web.a`, so it never clashes with the desktop
  `libraylib.a`. The Emscripten SDK lands in `subprojects/emsdk/` (git-ignored).
- `main.cpp` shares one code path for both targets; on web (`__EMSCRIPTEN__`) the
  frame loop is driven by `emscripten_set_main_loop` instead of a native
  `while (!WindowShouldClose())` loop. The desktop build is unchanged.
- The screenshot harness flags are desktop-only.

## Clean

```bash
python build.py --clean
```

or directly with make:

```bash
mingw32-make clean
mingw32-make raylib-clean
```

## Notes

- `clay-widgets/widgets.h` includes `clay.h` and the sibling split headers, so the Makefile adds `subprojects/clay` to include paths.
- Text input expects UTF-8 bytes from the platform layer. `main.cpp` converts raylib codepoints to UTF-8 bytes each frame.
- Keep widget IDs stable across frames for consistent interaction behavior.
- Image tint transport: this Clay build emits a stray RECTANGLE whenever an
  element's `backgroundColor` alpha is non-zero, so `image.h` sends the icon
  tint through the element's `userData` as a packed `0xRRGGBBAA` instead. The
  raylib renderer decodes it with `ClayWidgets_UnpackTint`.

## Screenshot harness

`main.cpp` accepts flags to render a few frames headless, inject synthetic
input, capture a PNG (via raylib `TakeScreenshot`, written relative to the
working directory) and exit. This is used to verify widgets without a human at
the keyboard:

```
clay-widgets-demo --shot out.png [--view N] [--frames N] [--theme N]
                  [--mouse X Y] [--mousedown] [--rightclick] [--scroll DY]
                  [--mouse2 X Y] [--mousedown2] [--openmodal] [--toast] [--no-anim]
```

- `--view` selects Dashboard (0), Tasks (1), Gallery (2) or Settings (3).
- `--no-anim` turns off all widget transitions so a single frame is the settled
  state. Because animations are time-based, without this a low `--frames` count
  captures a mid-transition frame; either pass `--no-anim`, or raise `--frames`
  (e.g. 20+) to let the animation settle.
- `--theme` picks a preset (1 Slate, 2 Sand, 3 Forest, 4 Windows).
- `--mouse`/`--mousedown` inject a pointer and a scripted click (`--rightclick`
  makes that a right-click, e.g. to open a context menu); `--mouse2`/
  `--mousedown2` add a second interaction phase (e.g. open a menu, then choose
  an item).
