# TiramisuChess

TiramisuChess is a C++17 chess engine with independent UCI, developer-tool, and
SFML 2.6 desktop targets. The v1 rework prioritizes rule correctness,
reproducible strength testing, efficient classical search, and an optional NNUE
evaluation path.

## Current engine

- Legal chess including castling, promotion, and en passant
- Iterative-deepening alpha-beta/PVS search
- Quiescence, transposition table, null-move pruning, LMR, aspiration windows,
  killer/history/countermove ordering, and time management
- UCI Hash, Threads, Clear Hash, searchmoves, EvalFile, and Use NNUE controls
- SFML modes for local play, play against the engine, and engine self-play
- Perft, fixed-position benchmarks, UCI smoke tests, paired opening matches,
  and cross-platform headless CI

The optional root-parallel search is still experimental. One thread remains the
default until paired Elo testing proves a multi-thread configuration stronger at
equal time.

## Build the headless engine and tools

No SFML installation is needed:

```sh
cmake -S . -B build -DCHESS_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Executables:

- `build/tiramisu-uci` — tournament/analysis engine
- `build/tiramisu-tools` — perft, divide, and benchmarks
- `build/chess-core-tests` — deterministic core test suite

Example UCI session:

```text
./build/tiramisu-uci
uci
isready
position startpos moves e2e4 e7e5 g1f3
go movetime 1000
quit
```

## Build the GUI

The GUI requires SFML 2.6.x; SFML 3 is not API-compatible with this project.

```sh
cmake -S . -B build-gui \
  -DCHESS_BUILD_GUI=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/sfml-2.6.2
cmake --build build-gui --parallel
./build-gui/gui
```

Platform helpers are provided in `scripts/build_mac.sh`,
`scripts/build_linux.sh`, and `scripts/build_windows.bat`. Set `SFML_PREFIX` on
macOS or `SFML_DIR` on Windows when SFML is not in a standard CMake prefix.

## Correctness and regression gates

```sh
scripts/run_quality_gate.sh
```

The gate runs:

- Standard six-position perft regression through depth 4
- Search benchmark telemetry
- UCI lifecycle, option, and searchmoves checks

The C++ test target additionally checks randomized make/unmake and incremental
hash invariants, FEN round-tripping, SAN, repetition hashing, draw adjudication,
transposition-table collisions, and the NNUE binary contract.

## Strength testing

Install `cutechess-cli`, build separate candidate and baseline binaries, then:

```sh
GAMES=400 TC=10+0.1 THREADS=1 \
  scripts/run_elo_match.sh ./candidate/tiramisu-uci ./baseline/tiramisu-uci
```

Matches use `tests/openings.epd` with paired colours. For serious tuning, set
`OPENINGS_FILE` to a much larger balanced suite and enable SPRT:

```sh
SPRT=1 ELO0=0 ELO1=5 GAMES=10000 OPENINGS_FILE=/path/to/uho.epd \
  scripts/run_elo_match.sh ./candidate/tiramisu-uci ./baseline/tiramisu-uci
```

Search and evaluation changes should not be accepted solely because they reach
more nodes or look positionally sensible; they need paired match evidence.

## NNUE

The engine contains a tested, versioned HalfKP-v1 loader and reference quantized
inference backend. Classical evaluation stays active unless a network is loaded
and explicitly enabled:

```text
setoption name EvalFile value networks/tiramisu-v1.nnue
setoption name Use NNUE value true
```

Dataset generation, PyTorch training, export commands, hardware guidance, and
release requirements are documented in [docs/NNUE.md](docs/NNUE.md). An NNUE
network is not bundled yet.

## Repository layout

```text
src/                  chess core, evaluation, search, UCI, GUI
tests/                deterministic tests and opening positions
scripts/              builds, quality gates, Elo matches
scripts/nnue/         dataset generation and PyTorch training
docs/                 baseline, roadmap, and NNUE specification
assets/               GUI piece artwork
```

See [docs/BASELINE.md](docs/BASELINE.md) for the pre-v1 measurements and
[docs/V1_ROADMAP.md](docs/V1_ROADMAP.md) for release criteria.
