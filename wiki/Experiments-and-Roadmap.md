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

1. compare Stockfish teacher labels at approximately 5,000 and 20,000 nodes on the same sampled positions;
2. choose the lowest budget that preserves adequate label quality;
3. generate and validate a multi-million-position dataset from a modern source archive;
4. train, quantize, and verify a production-scale candidate; and
5. run paired promotion matches against classical evaluation.

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
