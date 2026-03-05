# README – Build Instructions

## Overview
This project is a C++ chess engine with an SFML 2.6.x GUI.
It will NOT compile against SFML 3.x.
Make sure you are using SFML 2.6.x on all platforms.
The same binary also supports a UCI mode (`--uci`) for engine-vs-engine testing.
Source files:
- `src/main.cpp` (app entrypoint)
- `src/ui.cpp` / `src/ui.hpp` (UI assets + mode helpers)
- `src/chess_core.hpp` (umbrella include)
- `src/chess_types.hpp`, `src/board.hpp`, `src/search.hpp` (core chess modules)
- `CMakeLists.txt` (cross-platform build, including Windows)

## BUILDING ON WINDOWS (MSVC + CMake)

### Requirements

Windows 10/11
Visual Studio 2022 (Desktop development with C++)
CMake 3.21+
SFML 2.6.x (not 3.x)

### Configure and build

Set `SFML_DIR` to SFML's CMake package folder (example path shown below), then run:
```bat
set SFML_DIR=C:\libs\SFML-2.6.2\lib\cmake\SFML
cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64 -DSFML_DIR="%SFML_DIR%"
cmake --build build-windows --config Release
```

Run:
```bat
build-windows\Release\gui.exe
```

You can also use:
```bat
scripts\build_windows.bat
```
after setting `SFML_DIR`.

## BUILDING ON macOS (Apple Silicon / Intel)

### Requirements

macOS
clang++
SFML 2.6.x (built from source or installed locally)

#### Important
Homebrew installs SFML 3.x by default.
This codebase uses SFML 2.6.x APIs, so you must NOT link against SFML 3.x.
Recommended setup
Build SFML 2.6.2 locally into ~/.local/sfml-2.6.2
#### Compile command
```bash
clang++ -O2 -std=c++17 src/main.cpp src/ui.cpp -o gui -I"$HOME/.local/sfml-2.6.2/include" -L"$HOME/.local/sfml-2.6.2/lib" -lsfml-graphics -lsfml-window -lsfml-system -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -pthread -Wl,-rpath,"$HOME/.local/sfml-2.6.2/lib" -Wl,-sectcreate,__TEXT,__info_plist,macos/Info.plist
```
Run
```bash
./gui
```

If you get missing dylib or freetype errors, it means the runtime linker cannot find SFML’s bundled frameworks. Re-check the rpath and that SFML was installed correctly.

## BUILDING ON Fedora / Linux

### Requirements
g++ or clang++
SFML 2.6.x development packages
pkg-config
#### Install dependencies (Fedora example)
```bash
sudo dnf install sfml-devel pkg-config
```

#### Compile command
```bash
g++ -O2 -std=c++17 src/main.cpp src/ui.cpp -o gui $(pkg-config --cflags --libs sfml-graphics sfml-window sfml-system) -pthread

```
Run
```bash
./gui
```

## ENGINE TOOLS (HEADLESS)

After building, you can run engine-only tools from the CLI:

```bash
./build/gui --help
./build/gui --uci
./build/gui --perft 4
./build/gui --divide 3
./build/gui --perft-tests --max-depth 4
./build/gui --bench --bench-depth 6 --bench-time 1500 --bench-tt 128
```

Automated regression runner:

```bash
scripts/run_regression.sh ./build/gui
```

UCI protocol smoke test:

```bash
scripts/run_uci_smoke.sh ./build/gui
```

One-command quality gate (regression + UCI smoke):

```bash
scripts/run_quality_gate.sh ./build/gui
```

Optional: include an Elo match in the quality gate (requires `cutechess-cli`):

```bash
RUN_ELO=1 BASELINE_BIN=./build/gui scripts/run_quality_gate.sh ./build/gui
```

## ELO MATCH RUNNER (CUTECHESS)

Standalone Elo/SPRT script:

```bash
scripts/run_elo_match.sh <candidate_bin> <baseline_bin>
```

Useful environment overrides:
- `GAMES` (default `200`)
- `CONCURRENCY` (default `2`)
- `TC` (default `10+0.1`)
- `HASH_MB` (default `256`)
- `SPRT=1` (enables SPRT mode; configure with `ELO0`, `ELO1`, `ALPHA`, `BETA`)

Example:

```bash
GAMES=400 CONCURRENCY=4 TC=40/10+0.1 scripts/run_elo_match.sh ./build/gui ./build/gui
```

## NOTES

Default AI search depth is defined in `src/main.cpp`.
Current default:
`int aiMaxDepth = 20;`

AI search runs on a worker thread to avoid UI freezes.

Board flipping is visual only and does not affect game logic.

Assets required:
assets/pieces_png/white_.png
assets/pieces_png/black_.png

Fonts are loaded dynamically from common Windows/Linux/macOS paths.
If no font loads, text will not render but the game will still run.
