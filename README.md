# clay-widgets

Widget layer built on top of Clay with a raylib demo application.

## Project layout

- `clay-widgets.h`: header-only widget library built on Clay.
- `clay-widgets-themes.h`: extracted theme preset header used by the core widget layer and demo.
- `main.cpp`: demo app showing all currently implemented widgets.
- `Makefile`: builds raylib from source in `subprojects/raylib` and then builds the demo.
- `subprojects/raylib`: vendored raylib source (git clone).
- `subprojects/clay`: vendored Clay source (git clone, used for `clay.h`).

## Widgets currently demonstrated

- Label
- Heading
- Separator
- Button
- Checkbox
- Radio buttons
- Slider
- Progress bar
- Text input

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

- `clay-widgets.h` includes `clay.h`, so the Makefile adds `subprojects/clay` to include paths.
- Text input expects UTF-8 bytes from the platform layer. `main.cpp` converts raylib codepoints to UTF-8 bytes each frame.
- Keep widget IDs stable across frames for consistent interaction behavior.
