# Windows Search and NNUE Tooling — 2026-08-20

These measurements were taken on the v1 development line on a Ryzen 9 5900X
Windows host. They are local development evidence, not an absolute rating.

## Environment and startup verification

- CMake 4.2.3
- Git 2.51.0.windows.2
- Python 3.13.14
- MSVC 19.50.35725 with Windows SDK 10.0.26100
- NVIDIA RTX 3070, driver 610.88
- Visual Studio Release generator, headless build, tests enabled

Commands:

```powershell
cmake -S . -B build-pc -DCHESS_BUILD_GUI=OFF -DBUILD_TESTING=ON
cmake --build build-pc --config Release --parallel
ctest --test-dir build-pc -C Release --output-on-failure
.\build-pc\Release\chess-engine-tools.exe --perft-tests --max-depth 4
```

Both CTest targets and the standard six-position depth-4 perft suite passed.
The web API smoke passed in `.venv-web`; manual browser verification covered the
setup modal, independent CvC engine selectors, Stockfish 18 registration,
autoplay/pause, board flip, live depth/PV/nodes/NPS/time analysis, export
controls, and the radial/status thinking countdown. Cute Chess 1.5.1 was already
installed and the four-game workflow check completed without failures.

## Search experiments

### Incremental null move — retained

Null move now updates en-passant, side-to-move, counters, and Zobrist state
incrementally instead of copying the full board and recomputing its hash. Exact
state/hash tests cover positions with and without en passant. Fixed-depth moves,
scores, and the 270,343-node depth-10 tree were unchanged. Median runtime fell
from 5,108 ms to 4,951 ms (about 3.1%).

Commit: `f1ee52f`

### Qsearch SEE pruning — retained

Quiescence skips sufficiently losing non-promotion, non-en-passant captures
after delta pruning. The depth-10 tree fell from 270,343 to 244,274 nodes; its
median time was 4,037 ms on the same build path.

A 100-game paired `2+0.02` match against the incremental-null baseline scored
`38-28-34` (55.0%, approximately +34.9 ±55.9 Elo, LOS 89.1%) with no reported
failures. The sample is encouraging rather than conclusive.

Commit: `8e72958`

### Full-sort replacement — rejected

A lazy selection-scan experiment produced a 4,743 ms median versus 4,037 ms for
the current sorter, roughly 17.5% slower. It was removed without committing.

### Pawn correction history — rejected and reverted

The implementation included incremental pawn hashing, state tests, and a
correction table, but slowed the fixed-depth benchmark. Its 100-game paired
match scored exactly `34-34-32` (50.0%, LOS 50%). The experiment was committed
for auditability and then reverted.

Commits: `8abdf2c`, revert `8c728b6`

### Continuation history — retained provisionally

A compact continuation table keyed by side, previous destination, and current
destination now contributes to quiet ordering and gravity-based history
updates. It reduced the depth-10 tree from 244,274 to 186,618 nodes without a
runtime regression.

The 100-game paired `2+0.02` diagnostic against the post-revert baseline scored
`37-31-32` (53.0%, approximately +20.9 ±56.6 Elo, LOS 76.7%). This is
directionally positive but requires a slower, longer confirmation.

Commit: `e0b096d`

## Current fixed-depth benchmark

Command, run three times:

```powershell
.\build-pc\Release\chess-engine-tools.exe `
  --bench --bench-depth 10 --bench-tt 256 --threads 1
```

Every run returned the same moves, scores, 186,618 main nodes, and 107,300
quiescence nodes across the four positions. Summary times were 3,976, 3,898,
and 3,915 ms; the median was 3,915 ms (47,667 NPS).

At depth 8 the current tree contains 81,425 main nodes. Three summary times were
1,782, 1,759, and 1,786 ms; the median was 1,782 ms (45,693 NPS).

## NNUE pipeline improvements

The dataset coordinator now accepts `--target-training-positions`, calculates
per-worker headroom, prints position/shard rates and ETAs, and deterministically
caps the globally deduplicated training merge at the requested size. Optional
same-position `--comparison-nodes` labels report teacher-budget MAE, RMSE,
maximum difference, bias, and sign agreement.

The trainer now records validation RMSE, MAE, and sign accuracy per epoch and
for the best model, then computes batched bias/MAE/RMSE/sign-accuracy slices by
side-to-move king square, phase, material imbalance, and teacher-evaluation
magnitude. A streaming diagnostic reports feature frequency/coverage,
king-square use, phase, material imbalance, label magnitude/sign, result, and
ply distributions. A separate portable merger verifies copied output and
manifest checksums, enforces compatible teacher/split contracts, permits
platform-specific Stockfish checksums, globally deduplicates, blocks
train/validation position leakage, and writes a new provenance manifest.

End-to-end two-worker Stockfish 18 smoke:

- requested/retained training positions: 100/100
- retained validation positions: 92
- teacher comparison: 224 samples at 50 vs 100 nodes
- comparison MAE: 22.75 cp
- comparison RMSE: 42.17 cp
- score-sign agreement: 96.88%
- portable re-merge reproduced the exact training and validation SHA-256 values

The full 158,699-position January 2013 training corpus was re-audited by the new
streaming tool and exactly reproduced the previously reported 29,421/40,960
HalfKP feature coverage (71.83%).

Python syntax checks and all 13 dataset plus 10 training unit tests passed. A
real 100-training/92-validation-position CPU smoke also completed training,
quantization, export, grouped diagnostics, and exact C++ verification.

## Interpretation

The accepted search work provides a stronger current candidate with modest
positive match evidence; continuation history still needs confirmation. The
NNUE work removes the operational blockers to a serious five-million-position
experiment, but no new strength network was trained in this run. Classical
evaluation therefore remains the default.
