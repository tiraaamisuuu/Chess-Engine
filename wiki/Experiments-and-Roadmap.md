# Experiments and Roadmap

The engine is developed experimentally: implement one coherent idea, prove correctness, measure fixed-work behaviour, run a strength test when appropriate, then keep or revert it.

## Established results

The following results are recorded in the repository’s experiment reports:

| Change | Result | Decision |
|---|---:|---|
| Slow Stockfish 18 calibration | 2,400 games at `30+0.3`; level with the 2400 rung | Strong slower-control anchor; add a higher rung to close the formal bracket |
| Stockfish 18 limited-strength calibration | 1,200 games; approximately 2321.5 local crossing | Fast-control baseline retained; do not combine it with the slower campaign |
| v1 versus `v0.4.0` | 299 wins, 18 losses, 83 draws | Released; +303 +/- 37 Elo in this test pool |
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
| Small NNUE candidate | 2-37-1, 158,699 positions, 71.8% input coverage | Rejected |
| Five-million-position NNUE | 32.5% over 400 games versus classical | Rejected; pipeline retained |

These figures are not mixed into one synthetic score. Node reductions, throughput gains, and match outcomes answer different questions.

## Current priority

The immediate work is post-v1 strength validation:

1. extend the slower calibration above 2400 to close the formal bracket;
2. confirm continuation history and qsearch SEE pruning at a slower time
   control;
3. profile root-parallel contention and prove equal-time strength before
   changing the one-thread default;
4. target rare HalfKP inputs and test a stronger representation; and
5. automate candidate-versus-champion promotion while preserving every result.

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

The detailed active checklist lives in [V1 Roadmap](https://github.com/tiraaamisuuu/Forklift/blob/main/docs/V1_ROADMAP.md), the unbounded programme lives in [Ultimate Roadmap](Ultimate-Roadmap.md), and the current implementation state is recorded in [Development Status](https://github.com/tiraaamisuuu/Forklift/blob/main/docs/DEVELOPMENT.md).
