#!/usr/bin/env python3
"""Small build wrapper for clay-widgets.

Usage:
  python tools/build.py            # build
  python tools/build.py --clean    # clean (app + raylib) then build
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def find_make_command() -> str:
    env_make = os.environ.get("MAKE")
    if env_make:
        return env_make

    candidates = [
        "mingw32-make",
        "make",
    ]
    for candidate in candidates:
        if shutil.which(candidate):
            return candidate

    raise FileNotFoundError(
        "No make command found. Install make in MSYS2 (mingw32-make) or set MAKE env var."
    )


def run_command(cmd: list[str], cwd: Path) -> None:
    print("+", " ".join(cmd))
    subprocess.run(cmd, cwd=str(cwd), check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build clay-widgets demo through Makefile")
    parser.add_argument(
        "--clean",
        action="store_true",
        help="run clean and raylib-clean before building",
    )
    parser.add_argument('--test', action='store_true', help='run headless tests instead of building the demo')
    args = parser.parse_args()

    project_root = Path(__file__).resolve().parent.parent

    try:
        make = find_make_command()
    except FileNotFoundError as err:
        print(f"error: {err}", file=sys.stderr)
        return 1

    try:
        if args.clean:
            run_command([make, "clean"], cwd=project_root)
            run_command([make, "raylib-clean"], cwd=project_root)

        run_command([make, 'test'] if args.test else [make], cwd=project_root)
    except subprocess.CalledProcessError as err:
        return err.returncode

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
