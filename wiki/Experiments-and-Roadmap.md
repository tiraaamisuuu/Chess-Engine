# Experiments and Roadmap

The engine is developed experimentally: implement one coherent idea, prove correctness, measure fixed-work behaviour, run a strength test when appropriate, then keep or revert it.

## Established results

The following results are recorded in the repository’s experiment reports:

| Change | Result | Decision |
|---|---:|---|
| v1 versus the legacy baseline | 70 wins, 13 losses, 17 draws | Promising preliminary result |
| Mobility evaluation | 11.8% fewer nodes | Kept |
| Stack-based move lists | 11.5% faster | Kept |
| Clock and material fast path | 4.4% faster | Kept |
| Null-move pruning | 3.1% fewer nodes | Kept |
| Quiescence SEE pruning | 38-28-34 match | Kept |
| Continuation history | 37-31-32 match | Kept |
| Root threads | 1.51-1.68x measured throughput | Kept; Elo not yet proven |
| Original root-parallel version | 0/100 in safety match | Reworked before acceptance |
| Correction history | 34-34-32 match | Reverted as neutral |
| Selection-based move ordering | 17.5% slower | Reverted |
| Small NNUE candidate | 2-37-1, 158,699 nodes, 71.8% of classical speed | Rejected |

These figures are not mixed into one synthetic score. Node reductions, throughput gains, and match outcomes answer different questions.

## Current priority

The immediate work is data quality for the 256-unit HalfKP NNUE:

1. obtain and checksum a modern Lichess standard-rated CC0 monthly archive;
2. generate and validate five million 20k-node training positions plus a
   game-disjoint validation split;
3. train, quantize, and verify the unchanged 256-wide HalfKP candidate; and
4. run paired promotion matches against classical evaluation.

The teacher budget is now resolved: 127,784 same-position 5k-vs-20k searches
had 97.499% score-sign agreement, leaving 3,196 disagreements. The project
selected 20k for the first 5M baseline despite its fourfold node cost because
the measured host projects to about 21 hours and the changed signs are material.

## Longer-term phases

After the data and NNUE gate, the roadmap continues through:

1. stronger single-thread search and evaluation tuning;
2. more rigorous time management and strength calibration;
3. proven multi-thread scaling rather than throughput-only wins;
4. production NNUE training and inference optimization;
5. larger reproducible match campaigns;
6. packaging and release hardening; and
7. continuing profiling and experiment-driven refinement.

The detailed active checklist lives in [V1 Roadmap](https://github.com/tiraaamisuuu/Chess-Engine/blob/dev/v1/docs/V1_ROADMAP.md), and the current implementation state is recorded in [Development Status](https://github.com/tiraaamisuuu/Chess-Engine/blob/dev/v1/docs/DEVELOPMENT.md).
