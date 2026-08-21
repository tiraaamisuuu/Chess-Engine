# Chess Engine

A high-performance C++17 chess engine combining modern alpha-beta search with
classical and custom trainable NNUE evaluation. The project includes a local
chess interface, UCI support, CUDA/PyTorch training, Stockfish teacher
labelling, incremental NNUE inference, and reproducible strength testing.

`C++` · `UCI` · `NNUE` · `CUDA / PyTorch` · `SFML`

<!-- HERO IMAGE
Recommended: a clean screenshot of the web interface during an interesting
middlegame, with the board, evaluation, PV and engine identity visible.
Suggested size: 1600 × 900 (16:9), WebP or PNG.
Save as: docs/assets/engine-room-hero.webp
Enable with: ![Chess Engine interface](docs/assets/engine-room-hero.webp)
-->

## Highlights

- Iterative-deepening alpha-beta/PVS with transposition tables, quiescence,
  null-move pruning, LMR, aspiration windows, SEE and history-based ordering
- Correct legal move generation with castling, en passant, promotion,
  repetition and rule-draw handling
- Standalone UCI engine plus a responsive local web interface for PvP, PvC,
  CvC, external engines, live analysis and game export
- Optional versioned HalfKP NNUE with quantized C++ inference and incremental
  per-ply accumulators
- Resumable Stockfish labelling from streamed Lichess `.pgn.zst` archives and
  CUDA training with exact Python/C++ export verification
- Perft, deterministic core tests, CI, fixed-position benchmarks, paired
  opening matches, Stockfish calibration and SPRT support

<!-- ENGINE ARCHITECTURE VISUAL
Recommended: restrained monochrome SVG showing UCI/web input → board state →
iterative search → classical or NNUE evaluation → best move/PV, with TT and
time management as supporting components.
Suggested size: 1400 × 760 (approximately 16:9).
Save as: docs/assets/engine-architecture.svg
Enable with: ![Engine architecture](docs/assets/engine-architecture.svg)
-->

## Performance

All figures below are relative development measurements, not absolute Elo
claims. Full hardware, commands, commits and uncertainty are preserved in
[`docs/results/`](docs/results/).

| Measurement | Result | Interpretation |
|---|---:|---|
| v1 vs `v0.4.0`, 400 paired `10+0.1` games | `299–18–83` | 85.1%, +303.0 +/- 37.1 Elo, 100% LOS; release strength gate passed |
| Depth-10 search work | 270,343 → 186,618 nodes | Qsearch SEE pruning plus continuation history; changed tree |
| Incremental null move | 5,108 → 4,951 ms | 3.1% median reduction with identical tree |
| Incremental NNUE accumulator | 13,457 → 46,010 median NPS | 3.42× over full accumulator rebuild on the same smoke network |
| Six-thread root search | 1.51×–1.68× benchmark throughput | No proven playing-strength gain; one thread remains default |

The retained qsearch SEE change scored `38–28–34` in its 100-game diagnostic.
Continuation history scored `37–31–32` and remains provisional pending a
slower, longer match. Pawn correction history and a selection-scan move picker
were rejected after neutral or negative measurements.

<!-- PERFORMANCE VISUAL
Recommended later, once another release-grade match exists: a simple line or
step chart of measured candidate score across tagged engine revisions. Include
sample size and uncertainty; do not mix NPS and Elo on one axis.
Suggested size: 1400 × 800 (7:4).
Save as: docs/assets/strength-development.svg
Enable with: ![Measured engine development](docs/assets/strength-development.svg)
-->

## NNUE

The custom `HalfKP-v1` pipeline turns licensed game archives into deterministic,
game-disjoint datasets, labels sampled positions with Stockfish, trains on
CUDA, quantizes the network, and verifies exact C++ predictions before match
testing. It supports target-sized generation, cross-machine shard merging,
feature-coverage audits, teacher-budget comparisons and sliced validation
errors.

The first production experiment retained five million Stockfish-18-labelled
training positions and 616,632 game-disjoint validation positions from the
July 2026 Lichess CC0 archive. It covered 92.5% of the HalfKP inputs, passed
quantization and exact C++ verification, and completed a 400-game match without
technical failures. The fixed 256-wide network still lost decisively to the
classical evaluator and was rejected. A WDL objective and configurable hybrid
evaluation improved the diagnostic results but did not establish a strength
gain, so classical evaluation remains the default.

<!-- NNUE PIPELINE VISUAL
Recommended: horizontal SVG of Lichess/self-play → sampling → Stockfish teacher
→ compact dataset → CUDA/PyTorch → quantization → C++ verification → paired
match → promote/reject.
Suggested size: 1800 × 700 (18:7).
Save as: docs/assets/nnue-pipeline.svg
Enable with: ![NNUE training and promotion pipeline](docs/assets/nnue-pipeline.svg)
-->

## Quick start

Launch the local web interface:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_web_gui.ps1
```

On macOS/Linux, use `scripts/run_web_gui.sh`. The launcher creates its isolated
Python environment, builds the headless engine when necessary, and opens the
local interface.

Build and test the headless engine directly:

```sh
cmake -S . -B build -DCHESS_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The principal executables are `chess-engine-uci`, `chess-engine-tools`, and
`chess-core-tests`.

## Documentation

- **[Technical Wiki](wiki/Home.md)** — architecture, search, evaluation, NNUE,
  building, testing and development workflows
- [NNUE training and promotion](docs/NNUE.md) — exact production commands
- [Benchmarking and strength testing](docs/BENCHMARKING.md) — paired matches,
  calibration and SPRT
- [Development status](docs/DEVELOPMENT.md) — current configuration and release
  gates
- [Raw experimental results](docs/results/) — permanent reproducibility record

The Wiki source is kept in this repository until GitHub Wiki is enabled, after
which the same pages can be published to the repository's separate Wiki Git
repository.

## Roadmap

**Current**

- Review and merge the release-ready `dev/v1` branch into `main`
- Tag and package `v1.0.0` after the merge
- Keep classical evaluation and one search thread as the proven defaults

**Next**

- Confirm retained search gains at a slower time control
- Profile root-parallel scaling and duplicated work
- Target rare HalfKP inputs and prototype a stronger NNUE representation
- Add engine self-play and targeted rare-position data only as controlled inputs
- Automate candidate-vs-champion promotion and rejection

**Longer term**

- Continue isolated search, parallelism and time-management experiments
- Build a repeatable `generate → train → test → promote` improvement loop

The project follows one rule throughout: **implement → test → measure → keep or
reject → document**.
