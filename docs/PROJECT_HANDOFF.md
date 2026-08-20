# Project Status and Developer Handoff

This document records the current engineering state, reproducible development
workflow, accepted/rejected experiments, and the highest-priority work needed
to take the engine from the v1 development line to a release.

## Repository state

- Active development branch: `dev/v1`
- Release branch: `main`
- Classical evaluation remains the default.
- NNUE is optional and fully wired, but no network has earned promotion.
- Generated builds, datasets, downloaded engines, networks, match logs, and
  environments are intentionally ignored by Git.
- Do not merge `dev/v1` into `main` until the release gates below pass.

Before changing the engine, fetch the remote, inspect the current branch and
worktree, and read:

- `README.md`
- `docs/V1_RESULTS.md`
- `docs/V1_ROADMAP.md`
- `docs/NNUE.md`
- `docs/BENCHMARKING.md`

Preserve unrelated local changes. Keep search experiments in coherent commits,
benchmark them, and use paired matches before treating a changed search tree as
a strength improvement.

## Project goals and non-negotiable defaults

The objective is a correct, materially stronger modern C++ engine with a
repeatable improvement loop for classical search and NNUE. Work should improve
measured strength, efficiency, correctness, training quality, reproducibility,
or project usability.

The following defaults are deliberate:

- C++17 remains the engine language. It provides predictable data layout,
  mature optimizing compilers, direct SIMD/thread control, and an established
  chess-engine ecosystem. A Rust rewrite would consume substantial effort
  without inherently producing more Elo or NPS.
- One search thread remains the default. Root parallelism is safe and improves
  aggregate throughput on this host, but has not yet demonstrated an equal-time
  Elo gain.
- Classical evaluation remains the default until NNUE beats it in a properly
  powered paired test.
- The HalfKP hidden width remains 256 for the next large-data experiment, so
  the effect of data scale is measured independently from architecture changes.
- Failed experiments remain documented; rejected code should be reverted or
  cleanly disabled.

## Current architecture

### Build targets

- `chess-engine-uci`: standalone UCI engine for GUIs and tournament runners
- `chess-engine-tools`: perft, FEN/evaluation inspection, and fixed-position
  benchmarking
- `chess-core-tests`: deterministic core/search/NNUE tests
- `gui`: legacy SFML 2.6 desktop interface when `CHESS_BUILD_GUI=ON`
- `web/server.py`: local web interface and UCI bridge

### Chess core and correctness

The board uses incremental make/unmake state, Zobrist hashing, cached king
squares, bounded stack move lists, legal filtering, and explicit state for
castling, en passant, repetition, halfmove/fullmove counters, and promotion.
Dedicated incremental null moves update only the relevant state and hash.

Tests cover standard six-position perft, randomized make/unmake and hash
invariants, null moves, FEN round trips, SAN, repetition/draw rules, castling,
en passant, promotions, mate-score normalization, transposition-table
collisions, incremental NNUE accumulators, and UCI lifecycle/options.

### Search techniques currently implemented

The engine is well beyond basic minimax and alpha-beta. Its current search
includes:

- iterative deepening with principal-variation search
- aspiration windows around the previous iteration
- quiescence search with delta and SEE-based losing-capture pruning
- transposition table with bound-aware probing and mate-score normalization
- null-move pruning with verification safeguards
- internal iterative deepening/reduction support
- reverse futility pruning, razoring, ordinary futility pruning, and late-move
  pruning
- late-move reductions adjusted by search context
- static exchange evaluation for captures
- MVV/LVA-style capture ordering plus capture history
- killer moves, butterfly history, continuation history, and countermoves
- check/search extensions and draw/mate-distance handling
- time management with configurable move overhead and safe stop checks
- optional persistent root-parallel workers and a shared concurrent TT

Move ordering still uses a full sort. A lazy selection experiment was slower on
this engine and was rejected; a future staged picker should avoid rescanning
and be benchmarked against the present implementation.

### Evaluation

Classical evaluation contains material, piece-square, mobility, pawn-structure,
king-safety, bishop-pair, rook-file, and tapered phase terms. Mobility counting
is allocation-free.

The optional `HalfKP-v1` NNUE backend has a versioned loader, quantized integer
inference, UCI `EvalFile`/`Use NNUE` controls, and a per-ply incremental
accumulator stack. King moves rebuild only the affected perspective. Incremental
and full-rebuild paths are required to produce identical values and search
trees.

### Interfaces and test infrastructure

The local web GUI supports PvP, PvC, CvC, independent engine profiles, external
UCI engines, live analysis/PV, PGN/FEN/JSON export, board flipping, undo,
promotion, and visible radial/status countdowns while an engine is thinking.

Strength tooling builds committed refs in isolation, uses paired reversed-color
openings through Cute Chess, records binary/opening/configuration checksums, and
emits machine-readable manifests/results. A Stockfish limited-strength ladder
and fixed-position thread-scaling benchmark are also available.

## Development environment verification

The current Windows host is a Ryzen 9 5900X, RTX 3070 8 GB, and 32 GB RAM with
Visual Studio/MSVC, CMake, Git, and Python installed. A fresh development
session should run the following checklist before feature work.

### 1. Synchronize and inspect

```powershell
git fetch --all --prune
git switch dev/v1
git pull --ff-only
git status --short --branch
git log -5 --oneline --decorate
```

Do not discard a dirty worktree. Identify and preserve pre-existing changes.

### 2. Verify tools

```powershell
cmake --version
git --version
python --version
nvidia-smi
```

The verified Windows compiler is MSVC 19.50 with Windows SDK 10.0.26100. A
standalone Ninja installation is not required; the Visual Studio generator is
supported.

### 3. Configure, build, and test

```powershell
cmake -S . -B build-pc -DCHESS_BUILD_GUI=OFF -DBUILD_TESTING=ON
cmake --build build-pc --config Release --parallel
ctest --test-dir build-pc -C Release --output-on-failure
.\build-pc\Release\chess-engine-tools.exe --perft-tests --max-depth 4
```

Executable naming can vary slightly by generator. `ctest -N -C Release` and the
contents of `build-pc\Release` show the authoritative targets.

### 4. Record a fixed-position baseline

```powershell
.\build-pc\Release\chess-engine-tools.exe `
  --bench --bench-depth 8 --bench-tt 256 --threads 1
```

Record the commit, compiler, exact command, nodes, time, and NPS under
`docs/results/`. Fixed depth is useful for performance changes intended to keep
the tree identical; node count alone is not playing-strength evidence.

### 5. Verify the web path

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_web_gui.ps1
```

The launcher builds when necessary, creates `.venv-web`, starts the local UCI
bridge, and opens the browser. For noninteractive API verification:

```powershell
.\.venv-web\Scripts\python.exe tests\web_api_smoke.py `
  --engine .\build-pc\Release\chess-engine-uci.exe
```

### 6. Verify NNUE tooling when it is in scope

```powershell
Get-ChildItem scripts\nnue\*.py | ForEach-Object {
  .\.venv-nnue\Scripts\python.exe -m py_compile $_.FullName
}
.\.venv-nnue\Scripts\python.exe tests\nnue_dataset_tests.py
.\.venv-nnue\Scripts\python.exe tests\nnue_training_tests.py
```

See `docs/NNUE.md` for the full five-million-position generation, diagnostics,
cross-machine merge, CUDA training, export, and promotion commands.

## Current measured evidence

Detailed results and caveats live in `docs/V1_RESULTS.md`. The most recent
Windows search work established:

- Incremental null move preserved fixed-depth moves, scores, and nodes while
  reducing median depth-10 time by about 3.1%.
- Qsearch SEE pruning reduced the depth-10 tree from 270,343 to 244,274 nodes
  and scored 55% over 100 paired fast games (`38-28-34`, approximately +35 Elo
  with wide uncertainty). It was retained.
- Pawn correction history slowed the benchmark and scored exactly 50% over 100
  games (`34-34-32`). It was reverted.
- Continuation history reduced the same depth-10 tree to 186,618 nodes and
  scored 53% over 100 games (`37-31-32`, approximately +21 Elo with wide
  uncertainty). It is retained provisionally pending a longer test.
- A selection-scan replacement for full sorting was roughly 17.5% slower and
  was rejected before commit.

The first diverse NNUE corpus contained only 158,699 training positions and
covered 29,421/40,960 HalfKP inputs (71.8%). Its network lost decisively to
classical evaluation and was rejected. This diagnoses insufficient data and
coverage; it is not evidence against NNUE.

The production pipeline now supports target-sized generation, progress/rate/ETA,
same-position teacher-budget comparison, streaming feature/distribution audits,
validation RMSE/MAE/sign accuracy with error slices by king square, phase,
material, and teacher magnitude, and checksummed cross-machine merging.

## Prioritized roadmap

### Priority 1: establish the large-data NNUE baseline

1. Obtain and checksum a modern Lichess CC0 standard archive.
2. Run the 100k 5k-vs-20k teacher comparison.
3. Generate the five-million-position 20k-node dataset.
4. Audit coverage and distributions; do not train if obvious holes remain.
5. Train the fixed 256-wide baseline on CUDA with the C++ verification gate.
6. Inspect the validation error slices and run a 400-game classical-vs-NNUE
   diagnostic.
7. Promote only if competitive, then run a larger SPRT; otherwise use those
   error slices to improve sampling/training and iterate.

This is the single highest-value path because the engine implementation and
training/export plumbing are already complete, while the existing network is
known to be data-starved.

### Priority 2: validate retained search gains

- Run a slower, longer paired test of continuation history plus qsearch SEE
  pruning against the pre-change baseline.
- Tune continuation/history bonuses only with evidence.
- Profile the current move sorter before attempting a true staged move picker.
- Explore stronger SEE-based capture/history pruning and LMR/history-based
  pruning one change at a time.
- Consider ProbCut and singular extensions only after TT/search invariants have
  dedicated tests; both can create subtle tactical regressions.

### Priority 3: parallel search

- Profile root-worker TT contention and duplicated work.
- Compare 1/2/4/6 threads at equal wall time and equal per-engine resources.
- Require a paired-match strength gain before changing the default from one.
- A deeper shared-tree design is higher risk than current NNUE data work.

### Priority 4: evaluation and future NNUE work

- Establish a tuned classical parameter baseline rather than hand-adjusting
  many correlated terms.
- After the 5M network, try 20M positions before changing feature architecture.
- Then evaluate wider layers, HalfKA-style features, alternative target blends,
  and self-play data one controlled variable at a time.

## Release gates

The v1 branch is ready for a pull request only when:

- clean Release builds and all C++/Python/web quality gates pass;
- no known chess-rule, make/unmake, hash, TT, NNUE, or UCI regression remains;
- a slower, adequately sized paired test establishes strength over the release
  baseline;
- any default NNUE has separately beaten the classical champion;
- documentation contains reproducible commands and honestly qualified results;
- the worktree is clean and the remote `dev/v1` branch contains every intended
  commit.

## Git and experiment discipline

Keep logical milestones in separate descriptive commits and push them. Never
rewrite published history merely to hide an unsuccessful experiment. A clean
revert plus a result entry is useful engineering evidence. Avoid committing
generated datasets, networks, external executables, virtual environments,
build directories, PGNs, or match artifacts.

For strength changes, a normal cycle is:

```text
implement -> correctness tests -> fixed-depth benchmark
-> short paired diagnostic -> retain/revert -> longer confirmation if retained
```

For NNUE, use:

```text
generate -> audit -> train -> quantize -> exact C++ check
-> classical/champion match -> promote/reject -> preserve manifest and result
```
