# Testing and Reproducibility

Every engine change should pass correctness tests first, then fixed-work performance checks, and finally a strength match when the change can affect move choice.

## Automated tests

The CMake test suite runs two primary checks:

- `chess-core-tests` covers board state, legal move generation, make/unmake,
  hashing, search behaviour, evaluation, NNUE accumulators,
  transposition-table behaviour, and randomized legal sequences.
- `uci-smoke` covers protocol startup, command handling, and shutdown.

Run them with:

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Perft

The engine uses a six-position perft suite to validate legal move generation, including castling, en passant, checks, promotions, and difficult tactical legality cases. A search optimization is not ready for measurement if perft changes unexpectedly.

## Search benchmarks

Fixed-depth and fixed-position benchmarks compare node counts, elapsed time, and nodes per second. Node count is the primary signal for pruning and move-ordering changes because it represents fixed work. NPS remains useful for implementation-cost changes but is sensitive to machine load and build configuration.

## Strength matches

The match runner supports external UCI engines, paired UHO openings, colour reversal, resumable artifacts, relative-Elo summaries, SPRT, and calibration against known opponents. Always preserve the command line, engine revisions, time control, opening source, PGN, and summary.

Short matches are smoke tests, not proof. A change that appears neutral or positive in a small match should be extended before it becomes the default.

The current one-thread classical champion is frozen in
`research/champion.json`. Show the champion and its versioned match contract
with:

```powershell
py -3 scripts\champion_gate.py show
```

Run a committed candidate through `champion_gate.py run`. A serious gate uses
paired openings and SPRT, preserves its complete evidence, distinguishes
promote/reject/inconclusive outcomes, and blocks promotion on technical
failures. The separate `apply` command can update the registry only after H1 is
accepted and only if the tested champion is still current. See the repository's
[Benchmarking guide](https://github.com/tiraaamisuuu/Forklift/blob/main/docs/BENCHMARKING.md)
for the exact commands.

## Thread scaling

Parallel search is measured separately from single-thread strength. The benchmark records speedup and efficiency at each thread count. More NPS does not by itself prove more Elo, so playing-strength tests remain necessary.

## Raw evidence

The `docs/results` directory contains dated experiment reports and machine-readable artifacts. These are deliberately separate from the concise landing page: the README states verified headline results, while the raw reports preserve the full context and caveats.

See [Benchmarking](https://github.com/tiraaamisuuu/Forklift/blob/main/docs/BENCHMARKING.md) for commands and reporting conventions.
