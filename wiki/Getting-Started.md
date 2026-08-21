# Getting Started

## Requirements

The headless engine requires:

- CMake 3.21 or newer
- A C++17 compiler
- Git
- Threads support supplied by the platform toolchain

The web interface additionally uses Python and `python-chess`. The launchers
create an isolated `.venv-web` automatically. SFML is not required unless the
legacy desktop GUI is explicitly enabled.

## Run the local web interface

Windows PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_web_gui.ps1
```

macOS/Linux:

```sh
scripts/run_web_gui.sh
```

The launcher locates or builds the UCI executable, installs the small web
dependency set into `.venv-web`, starts a service on the loopback interface,
and opens the local UI. The engine and positions remain on the computer.

The interface supports:

- player vs player, player vs computer, and computer vs computer
- click and pointer-following drag movement with legal-target guidance
- selectable repository revisions, NNUE models, official Stockfish, and trusted
  imported UCI executables
- independent white/black engine selection in CvC
- live analysis, evaluation, principal variation, depth, nodes, NPS and time
- undo, board orientation, timed autoplay, PGN/FEN/JSON export, and promotion

Only import executable engines you trust; they run with the current user's
permissions.

## Build the headless engine

Portable Release configuration:

```sh
cmake -S . -B build -DCHESS_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

With a Visual Studio multi-configuration generator on Windows:

```powershell
cmake -S . -B build-pc -DCHESS_BUILD_GUI=OFF -DBUILD_TESTING=ON
cmake --build build-pc --config Release --parallel
ctest --test-dir build-pc -C Release --output-on-failure
```

Primary targets:

| Target | Purpose |
|---|---|
| `chess-engine-uci` | Standalone UCI engine for GUIs and tournament runners |
| `chess-engine-tools` | Perft, FEN inspection, evaluation, and benchmarks |
| `chess-core-tests` | Deterministic core/search/NNUE test suite |
| `gui` | Optional SFML 2.6 desktop application |

## UCI

Minimal session:

```text
chess-engine-uci
uci
isready
position startpos moves e2e4 e7e5 g1f3
go movetime 1000
quit
```

Important options include:

- `Hash` — transposition-table size in MiB
- `Threads` — root search workers; one remains the strength-tested default
- `Move Overhead` — reserve for GUI/process/transport latency
- `Clear Hash`
- `EvalFile` — path to a compatible `HalfKP-v1` network
- `Use NNUE` — enables the loaded network when set to `true`

The engine also supports clock searches, fixed depth, fixed movetime,
`searchmoves`, `stop`, `ucinewgame`, and normal `position startpos`/`fen`
commands.

## Maximum-performance local build

On the verified Ryzen 9 5900X Windows host:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_max_performance.ps1
```

This enables AVX2 and interprocedural optimization. The output is tuned for the
machine that built it and is not a portable release artifact.

## Legacy SFML GUI

The desktop target requires SFML 2.6.x; SFML 3 is not API-compatible:

```sh
cmake -S . -B build-gui \
  -DCHESS_BUILD_GUI=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/sfml-2.6.2
cmake --build build-gui --parallel
```

Set `SFML_PREFIX` or `SFML_DIR` when the package is outside a standard CMake
location.

## Platform notes

- Windows scripts may require process-scoped `-ExecutionPolicy Bypass`; they do
  not change the user's permanent PowerShell policy.
- The headless targets build and run without SFML on Windows, macOS, and Linux.
- `.pgn.zst` NNUE sources are streamed and do not need to be decompressed to
  disk.
- Generated builds, virtual environments, datasets, downloaded engines,
  networks and match artifacts are ignored by Git.

For exact NNUE environment setup, continue to [NNUE Training](NNUE-Training.md).
