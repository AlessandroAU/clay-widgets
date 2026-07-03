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

## Demo coverage

- Action buttons for apply/reset flows.
- Multiple text fields for simple form input.
- Checkbox and radio groups for boolean and single-choice settings.
- Sliders paired with progress bars for continuous values.
- Theme preset switching between Slate, Sand, and Forest palettes.
- Live state panel that echoes the current widget state and focused widget id.

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
                  [--mouse2 X Y] [--mousedown2] [--openmodal] [--toast]
```

- `--view` selects Settings (0), Documents (1) or Tab Plane (2).
- `--theme` picks a preset (1 Slate, 2 Sand, 3 Forest, 4 Windows).
- `--mouse`/`--mousedown` inject a pointer and a scripted click (`--rightclick`
  makes that a right-click, e.g. to open a context menu); `--mouse2`/
  `--mousedown2` add a second interaction phase (e.g. open a menu, then choose
  an item).
