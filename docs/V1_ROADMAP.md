# Forklift v1 Roadmap

v1 is an evidence-driven rebuild now integrated into `main`. Correctness,
reproducible testing, and measurable playing strength take priority over
accumulating heuristics.

This file records the completed v1 programme and immediate follow-up. The
open-ended path beyond it lives in the [Ultimate Roadmap](ULTIMATE_ROADMAP.md).

## Completed foundations

- Independent UCI, developer-tool, test, web, and optional SFML targets
- Deterministic move generation, make/unmake, hash, draw, TT, NNUE, and UCI tests
- Six-position perft regression and cross-platform headless CI
- Paired opening-suite match runner, SPRT support, Stockfish calibration, and
  thread-scaling reports with checksummed artifacts
- Efficient classical search with PVS, TT, quiescence, SEE, null move, LMR,
  aspiration, multiple pruning methods, and rich move-ordering histories
- Safe persistent root workers and a shared concurrent transposition table
- Versioned HalfKP-v1 loader, incremental quantized inference, and exact
  Python/C++ export verification
- Resumable parallel Stockfish labelling from streamed `.pgn.zst` sources
- Target-sized datasets, global deduplication, game-disjoint validation,
  cross-machine merging, coverage audits, and CUDA checkpoint/resume
- Batched validation error slices by king square, phase, material imbalance,
  and teacher-evaluation magnitude

## Completed five-million-position NNUE baseline

1. **Complete:** 127,784 same-position 5k-vs-20k comparisons selected 20k for
   the first baseline (97.499% sign agreement; fourfold fixed-node cost).
2. **Complete:** checksum the July 2026 Lichess standard-rated CC0 archive.
3. **Complete:** generate and audit 5,000,000 training plus 616,632 game-disjoint
   validation positions at 20k nodes (92.50% HalfKP input coverage).
4. **Complete:** train the unchanged 256-wide HalfKP architecture on CUDA.
5. **Complete:** pass quantization checks and exact C++ agreement during export.
6. **Complete:** run the 400-game classical-vs-NNUE diagnostic; the network
   scored 32.5% (-127.0 +/- 33.4 Elo) with no technical failures.
7. **Decision:** reject the network and retain classical evaluation by default.

WDL-probability training and configurable classical/NNUE blending are now
available for controlled experiments. Neither has yet earned promotion.

## Completed release evidence

- **Complete:** 400 paired `10+0.1` games versus `v0.4.0` scored `299-18-83`
  (85.1%, +303.0 +/- 37.1 Elo, 100% LOS) with zero technical failures.
- **Complete:** all local C++/Python/perft/UCI/web gates pass.
- **Complete:** Ubuntu, macOS, Windows, sanitizers, and web GitHub jobs pass.
- **Complete:** the merge tree against `main` is conflict-free.
- **Complete:** `v1.0.0` is tagged and published with verified platform
  packages.
- **Complete:** the subsequent Forklift desktop overhaul adds smooth movement,
  original sound cues, native icon packaging, corrected resource telemetry,
  and adaptive time profiles while preserving the tested engine core.
- **Complete:** `v1.1.0` publishes the overhaul under the permanent Forklift
  name with explicit headless archives and a tested bundled Windows GUI ZIP.

## Current priority: post-v1 strength work

- Preserve `main` as the single canonical branch.
- Preserve classical evaluation and one thread as the proven defaults.
- **Complete:** the 4,200-game `30+0.3` Stockfish ladder brackets Forklift's
  local 50% crossing at approximately 2503, with a conservative timeout
  sensitivity estimate of 2500.
- Confirm retained search gains at a slower time control before further search
  expansion.

## Search follow-up

- Confirm continuation history and qsearch SEE pruning at a slower time control.
- Profile the current full move sort before attempting a true staged picker.
- Investigate capture/history pruning and context-aware LMR incrementally.
- Add ProbCut or singular extensions only with targeted tactical/TT tests.
- Improve root-parallel scaling and prove equal-time Elo before changing the
  one-thread default.

## Next NNUE iteration

- Target the 3,070 unseen and 21,184 at-most-100-frequency HalfKP inputs.
- Test a stronger two-perspective hidden stack or HalfKA-style features before
  spending on an unchanged 20M repeat.
- Retain WDL/result-blending experiments and self-play as isolated variables.
- Automate network-vs-current-champion promotion while preserving manifests,
  binaries, PGNs, logs, and decisions.
- Establish automatically tuned classical parameters as a strong fallback and
  independent baseline.

## Release criteria

- Clean Release builds and all C++/Python/web quality gates pass.
- No known rule, state, hash, TT, NNUE, or UCI correctness regression remains.
- A properly sized paired test establishes strength over the release baseline.
- Classical evaluation stays default unless an NNUE separately proves stronger.
- Documentation and result artifacts are reproducible and accurately qualified.

## Beyond v1

Absolute-strength calibration, a stronger NNUE, proven parallel search,
distributed testing, large-scale self-play and the separate policy/value
research line are staged in the [Ultimate Roadmap](ULTIMATE_ROADMAP.md).
