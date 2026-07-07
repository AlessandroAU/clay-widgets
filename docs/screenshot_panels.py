#!/usr/bin/env python3
"""Capture a screenshot of each main panel (view) of the clay-widgets demo.

The demo binary (`clay-widgets-demo`) ships a headless screenshot harness:
it can render a few frames, then write a PNG via raylib's TakeScreenshot and
exit. See the "Screenshot harness" section of the project README for the full
flag list. This script drives that harness once per view so you get a fresh
set of panel images in one command.

The four main panels (views) are:
    0  Dashboard  - live stats, a data table and quick actions
    1  Tasks      - a working to-do manager
    2  Gallery    - the full widget catalog
    3  Settings   - theme, behavior and diagnostics

Usage:
    python docs/screenshot_panels.py                 # all views, default theme
    python docs/screenshot_panels.py --theme 3       # Forest theme
    python docs/screenshot_panels.py --views 0 2     # only Dashboard + Gallery
    python docs/screenshot_panels.py --out docs/img  # custom output folder
    python docs/screenshot_panels.py --size 1600 1000

Notes:
    * raylib's TakeScreenshot writes the PNG relative to the working directory,
      and the demo loads fonts from `assets/` relative to the working directory
      too, so this script always runs the binary from the project root.
    * `--no-anim` is passed so each capture is the settled state (animations are
      time-based; without it a low frame count grabs a mid-transition frame).
    * A real display / GL context is required - this opens a window briefly per
      view. It is not usable over a headless SSH session without a virtual
      display.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

# docs/ lives directly under the project root.
PROJECT_ROOT = Path(__file__).resolve().parent.parent

# view index -> (filename stem, human label)
VIEWS: dict[int, tuple[str, str]] = {
    0: ("dashboard", "Dashboard"),
    1: ("tasks", "Tasks"),
    2: ("gallery", "Gallery"),
    3: ("settings", "Settings"),
}

# theme index -> label, matching main.cpp's --theme flag (0 keeps the default).
THEMES: dict[int, str] = {
    0: "default",
    1: "Slate",
    2: "Sand",
    3: "Forest",
    4: "Windows",
}


def find_binary() -> Path:
    """Locate the demo executable next to the project root."""
    names = ["clay-widgets-demo.exe", "clay-widgets-demo"]
    for name in names:
        candidate = PROJECT_ROOT / name
        if candidate.exists():
            return candidate
    # Fall back to PATH in case it was installed.
    on_path = shutil.which("clay-widgets-demo")
    if on_path:
        return Path(on_path)
    sys.exit(
        "error: could not find the demo binary. Build it first with "
        "`python build.py` (or `mingw32-make`) from the project root."
    )


def capture(binary: Path, view: int, out_dir: Path, args: argparse.Namespace) -> Path:
    """Render one view to a PNG and return the (project-root-relative) path."""
    stem, label = VIEWS[view]
    # Path passed to the binary must be relative to the working directory
    # (the project root) because raylib writes it verbatim from there.
    rel_out = (out_dir / f"{stem}.png").relative_to(PROJECT_ROOT)

    cmd = [
        str(binary),
        "--shot", str(rel_out),
        "--view", str(view),
        "--frames", str(args.frames),
        "--no-anim",
    ]
    if args.theme:
        cmd += ["--theme", str(args.theme)]
    if args.size:
        cmd += ["--size", str(args.size[0]), str(args.size[1])]

    theme_label = THEMES.get(args.theme, str(args.theme))
    print(f"  [{view}] {label:<10} theme={theme_label:<8} -> {rel_out.as_posix()}")

    result = subprocess.run(
        cmd,
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        sys.exit(f"error: demo exited with code {result.returncode} for view {view}")

    abs_out = PROJECT_ROOT / rel_out
    if not abs_out.exists():
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        sys.exit(
            f"error: expected screenshot was not written: {abs_out}. "
            "A display / GL context may be unavailable."
        )
    return abs_out


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Screenshot each main panel of the clay-widgets demo.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--out",
        default=str(PROJECT_ROOT / "docs" / "screenshots"),
        help="output directory for the PNGs (default: docs/screenshots)",
    )
    parser.add_argument(
        "--views",
        type=int,
        nargs="+",
        choices=sorted(VIEWS),
        metavar="N",
        help="only capture these view indices (default: all four)",
    )
    parser.add_argument(
        "--theme",
        type=int,
        default=0,
        choices=sorted(THEMES),
        metavar="N",
        help="theme preset: 0 default, 1 Slate, 2 Sand, 3 Forest, 4 Windows",
    )
    parser.add_argument(
        "--frames",
        type=int,
        default=3,
        help="frames to render before capture (default: 3; --no-anim is on so "
        "the first settled frame is fine)",
    )
    parser.add_argument(
        "--size",
        type=int,
        nargs=2,
        metavar=("W", "H"),
        help="window size in pixels, e.g. --size 1600 1000",
    )
    args = parser.parse_args()

    binary = find_binary()
    out_dir = Path(args.out).resolve()
    # raylib's TakeScreenshot won't create missing directories, so make it here.
    out_dir.mkdir(parents=True, exist_ok=True)

    views = args.views if args.views else sorted(VIEWS)

    print(f"binary : {binary}")
    print(f"output : {out_dir}")
    print(f"capturing {len(views)} panel(s):")

    written = [capture(binary, view, out_dir, args) for view in views]

    print(f"\ndone - wrote {len(written)} screenshot(s):")
    for path in written:
        print(f"  {path}")


if __name__ == "__main__":
    main()
