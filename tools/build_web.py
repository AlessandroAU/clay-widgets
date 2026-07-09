#!/usr/bin/env python3
"""Build clay-widgets for the web (WebAssembly + <canvas>) via Emscripten.

This compiles the *existing* raylib app to WebAssembly and emits an HTML page
that runs it on a <canvas>, so the web build looks identical to the desktop
demo. It is the counterpart to build.py (which builds the native .exe).

What it does:
  1. Auto-installs the Emscripten SDK into subprojects/emsdk on first run.
  2. Builds raylib for PLATFORM_WEB  -> subprojects/raylib/src/libraylib.web.a
     (a separate file from the desktop libraylib.a, so the two never clash).
  3. Compiles demo/main.cpp with emcc into:
       build/web/index.html  +  index.js  +  index.wasm
     (the UI font is baked into the binary, so there is no preloaded index.data).

Usage:
  python tools/build_web.py                # build (installs emsdk if missing)
  python tools/build_web.py --serve        # build, then serve build/web/ at http://localhost:8000
  python tools/build_web.py --clean        # remove build/web/ and libraylib.web.a, then build
  python tools/build_web.py --skip-raylib  # don't rebuild raylib (reuse libraylib.web.a)
  python tools/build_web.py --emsdk-version 3.1.64   # pin a specific emsdk version

Note: the output must be served over HTTP (browsers won't fetch .wasm/.data
from file://). Use --serve, or any static server pointed at build/web/.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EMSDK_DIR = ROOT / "subprojects" / "emsdk"
RAYLIB_SRC = ROOT / "subprojects" / "raylib" / "src"
RAYLIB_WEB_LIB = RAYLIB_SRC / "libraylib.web.a"
# Project-owned emscripten shell: a bare full-page canvas with no raylib header
# bar. Falls back to raylib's stock shell.html if this file is ever removed.
SHELL_FILE = ROOT / "web" / "shell.html"
RAYLIB_SHELL_FILE = RAYLIB_SRC / "shell.html"
OUT_DIR = ROOT / "build" / "web"


def resolve_shell() -> Path:
    if SHELL_FILE.exists():
        return SHELL_FILE
    return RAYLIB_SHELL_FILE
EMSDK_REPO = "https://github.com/emscripten-core/emsdk.git"


def run(cmd: list[str], cwd: Path | None = None, env: dict | None = None) -> None:
    print("+", " ".join(str(c) for c in cmd))
    subprocess.run([str(c) for c in cmd], cwd=str(cwd) if cwd else None, env=env, check=True)


def find_make() -> str:
    env_make = os.environ.get("MAKE")
    if env_make:
        return env_make
    for candidate in ("mingw32-make", "make"):
        if shutil.which(candidate):
            return candidate
    raise FileNotFoundError(
        "No make command found. Install make (MSYS2: mingw32-make) or set the MAKE env var."
    )


def ensure_emsdk(version: str) -> None:
    """Clone + install + activate the Emscripten SDK under subprojects/emsdk."""
    if not (EMSDK_DIR / "emsdk.py").exists():
        print(f"Cloning Emscripten SDK into {EMSDK_DIR} ...")
        EMSDK_DIR.parent.mkdir(parents=True, exist_ok=True)
        run(["git", "clone", "--depth", "1", EMSDK_REPO, str(EMSDK_DIR)])

    # If a usable emcc is already present in the activated env, skip the slow
    # install/activate (~1GB download) on subsequent runs.
    already = (EMSDK_DIR / "upstream" / "emscripten").exists()
    if not already:
        print("Installing + activating Emscripten SDK (one-time, downloads ~1GB) ...")
        run([sys.executable, str(EMSDK_DIR / "emsdk.py"), "install", version], cwd=EMSDK_DIR)
        run([sys.executable, str(EMSDK_DIR / "emsdk.py"), "activate", version], cwd=EMSDK_DIR)
    else:
        print("Emscripten SDK already installed; reusing it.")


def _first_glob(pattern: str) -> Path | None:
    matches = sorted(EMSDK_DIR.glob(pattern))
    return matches[0] if matches else None


def emsdk_env() -> dict:
    """Return os.environ augmented so emcc/emar and make can find the toolchain.

    Rather than shelling out to emsdk_env (whose env is awkward to capture
    reliably on Windows), we build the environment directly from emsdk's known
    on-disk layout: the emscripten launcher dir, the bundled node/python, and
    the .emscripten config that emcc reads for LLVM/node paths.
    """
    env = os.environ.copy()

    emscripten_dir = EMSDK_DIR / "upstream" / "emscripten"
    config = EMSDK_DIR / ".emscripten"
    node_exe = _first_glob("node/*/bin/node.exe") or _first_glob("node/*/bin/node")
    py_exe = _first_glob("python/*/python.exe") or _first_glob("python/*/bin/python3")

    if not emscripten_dir.exists():
        raise FileNotFoundError(f"Emscripten not found at {emscripten_dir}")
    if not config.exists():
        raise FileNotFoundError(f"emsdk config not found at {config} (activation may have failed)")

    env["EMSDK"] = str(EMSDK_DIR)
    env["EM_CONFIG"] = str(config)
    if node_exe:
        env["EMSDK_NODE"] = str(node_exe)
    if py_exe:
        # emcc.exe/emar.exe's native launcher spawns this as the interpreter, so
        # it must be the python executable itself, not its directory.
        env["EMSDK_PYTHON"] = str(py_exe)

    # Prepend the toolchain locations so shutil.which() and make's $(CC)=emcc
    # both resolve against the freshly installed SDK.
    prepend = [str(emscripten_dir), str(EMSDK_DIR)]
    if node_exe:
        prepend.insert(1, str(node_exe.parent))
    if py_exe:
        prepend.append(str(py_exe.parent))
    env["PATH"] = os.pathsep.join(prepend + [env.get("PATH", "")])
    return env


def require_tool(name: str, env: dict) -> str:
    tool = shutil.which(name, path=env.get("PATH"))
    if not tool:
        raise FileNotFoundError(
            f"'{name}' not found after activating emsdk. The SDK may not have installed correctly."
        )
    return tool


def build_raylib(env: dict, make: str) -> None:
    print("Building raylib for PLATFORM_WEB ...")
    # raylib compiles its translation units to fixed-name objects (rcore.o, ...)
    # directly in src/, shared between the desktop and web builds. Without -B,
    # make would reuse whatever .o are already there (e.g. native objects from a
    # desktop build) and just re-archive them, producing a libraylib.web.a full
    # of non-wasm objects. -B forces a clean emcc recompile.
    run([make, "-C", RAYLIB_SRC, "PLATFORM=PLATFORM_WEB", "-B", "-j"], env=env)
    if not RAYLIB_WEB_LIB.exists():
        raise FileNotFoundError(f"Expected {RAYLIB_WEB_LIB} was not produced.")
    # Delete the platform-specific objects so a later desktop build can't archive
    # these wasm objects into libraylib.a (and vice-versa). The libs themselves
    # (libraylib.a / libraylib.web.a) are kept; only the shared .o are removed.
    for obj in RAYLIB_SRC.glob("*.o"):
        obj.unlink()


def build_app(env: dict) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    emcc = require_tool("emcc", env)
    print("Compiling demo/main.cpp -> build/web/index.html with emcc ...")
    cmd = [
        emcc,
        "demo/main.cpp",
        "-std=c++20", "-O2",
        # clang (emcc) makes C++11 brace-init narrowing a hard error by default,
        # where the desktop g++ build only warns. The widget headers rely on
        # int->float radius/spacing conversions, so match g++ and demote it.
        "-Wno-c++11-narrowing",
        "-DPLATFORM_WEB",
        "-I.", "-Isubprojects/clay", "-Isubprojects/raylib/src",
        str(RAYLIB_WEB_LIB),
        "-sUSE_GLFW=3",              # raylib's web backend uses GLFW3 (WebGL)
        "-sASYNCIFY",                # lets emscripten_set_main_loop keep main()'s stack alive
        # Use a fixed (non-resizable) heap rather than ALLOW_MEMORY_GROWTH: a
        # growable heap is a *resizable* ArrayBuffer, and emscripten's TextDecoder
        # path (hit when raylib reads the GL version string) throws on those,
        # aborting init with a black canvas. 256MB is ample for this demo.
        "-sINITIAL_MEMORY=268435456",
        "-sSTACK_SIZE=1048576",      # Clay layout + raylib recurse; give a 1MB stack
        "-sGL_ENABLE_GET_PROC_ADDRESS",
        # No --preload-file: the UI font is baked into the binary (via
        # assets/generated/embedded-font.h), so the web build needs no virtual
        # filesystem / no separate index.data.
        "--shell-file", str(resolve_shell()),
        "-o", str(OUT_DIR / "index.html"),
    ]
    # Run from the repo root so demo/main.cpp and includes resolve relatively.
    run(cmd, cwd=ROOT, env=env)
    print(f"\nDone. Open {OUT_DIR / 'index.html'} via an HTTP server (see --serve).")


def clean() -> None:
    if OUT_DIR.exists():
        print(f"Removing {OUT_DIR}")
        shutil.rmtree(OUT_DIR)
    if RAYLIB_WEB_LIB.exists():
        print(f"Removing {RAYLIB_WEB_LIB}")
        RAYLIB_WEB_LIB.unlink()


def serve() -> None:
    import http.server
    import socketserver

    os.chdir(OUT_DIR)
    port = 8000
    handler = http.server.SimpleHTTPRequestHandler
    with socketserver.TCPServer(("", port), handler) as httpd:
        print(f"Serving {OUT_DIR} at http://localhost:{port}  (Ctrl+C to stop)")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nStopped.")


def main() -> int:
    parser = argparse.ArgumentParser(description="Build clay-widgets for the web via Emscripten")
    parser.add_argument("--clean", action="store_true", help="remove build/web/ and libraylib.web.a before building")
    parser.add_argument("--skip-raylib", action="store_true", help="reuse an existing libraylib.web.a")
    parser.add_argument("--serve", action="store_true", help="serve build/web/ on http://localhost:8000 after building")
    parser.add_argument("--emsdk-version", default="latest", help="emsdk version to install/activate (default: latest)")
    args = parser.parse_args()

    if not resolve_shell().exists():
        print(f"error: no emscripten shell found ({SHELL_FILE} or {RAYLIB_SHELL_FILE})", file=sys.stderr)
        return 1

    try:
        if args.clean:
            clean()

        ensure_emsdk(args.emsdk_version)
        env = emsdk_env()
        require_tool("emcc", env)  # fail early with a clear message if activation failed

        if not args.skip_raylib or not RAYLIB_WEB_LIB.exists():
            make = find_make()
            build_raylib(env, make)

        build_app(env)

        if args.serve:
            serve()
    except FileNotFoundError as err:
        print(f"error: {err}", file=sys.stderr)
        return 1
    except subprocess.CalledProcessError as err:
        return err.returncode or 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
