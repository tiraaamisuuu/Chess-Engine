# Chess Engine v1 Roadmap

v1 is an evidence-driven rebuild on `dev/v1`. Correctness, reproducible testing,
and measurable playing strength take priority over accumulating heuristics.

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

## Current priority: large-data NNUE baseline

1. Compare 5k and 20k Stockfish labels on the same 100k representative boards.
2. Generate and audit five million diverse 20k-node training positions.
3. Train the unchanged 256-wide HalfKP architecture on CUDA.
4. Require quantization checks and exact C++ agreement during export.
5. Run a 400-game classical-vs-NNUE diagnostic, followed by SPRT only if the
   network is competitive.
6. Promote the model only after statistically supported strength evidence.

## Search follow-up

- Confirm continuation history and qsearch SEE pruning at a slower time control.
- Profile the current full move sort before attempting a true staged picker.
- Investigate capture/history pruning and context-aware LMR incrementally.
- Add ProbCut or singular extensions only with targeted tactical/TT tests.
- Improve root-parallel scaling and prove equal-time Elo before changing the
  one-thread default.

## After the five-million baseline

- Scale to 20M positions if learning curves and coverage still improve.
- Test wider layers, HalfKA-style features, alternate target/result blending,
  and self-play data one controlled variable at a time.
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
