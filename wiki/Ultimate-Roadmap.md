# Ultimate Roadmap

Forklift's roadmap deliberately has no final “done” state. The goal is the
strongest correct, reproducible and usable engine possible at named time and
compute limits.

Every improvement follows the same loop:

```text
idea -> invariant tests -> fixed-work benchmark -> paired match
     -> statistically powered confirmation -> promote or reject -> archive
```

## The horizons

1. **Establish the truth:** calibrate Forklift against Stockfish and an
   independent engine pool at multiple time controls, with uncertainty and full
   hardware manifests.
2. **Single-thread strength:** improve move ordering, pruning, reductions,
   extensions, classical evaluation and time management one measured change at
   a time.
3. **Hot-path efficiency:** profile cache behaviour, representations, SIMD,
   PGO/LTO and architecture-specific builds, then prove speed gains in matches.
4. **Production NNUE:** target rare features, test stronger feature sets and
   network structures, improve data and objectives, optimize incremental
   inference, and automatically gauntlet checkpoints.
5. **Parallel search:** reduce duplicate work and TT contention, then prove
   equal-wall-time Elo scaling across increasing thread counts and NUMA hosts.
6. **Data flywheel:** combine diverse self-play, external teachers, game
   outcomes and targeted failure mining through controlled ablations and
   permanent holdouts.
7. **Distributed experimentation:** build a Fishtest-style worker network with
   SPRT, automated parameter tuning, reproducible builds and a public experiment
   ledger.
8. **Complete product:** add tablebases, pondering, MultiPV, Chess960, analysis
   workflows, accessibility, signed packages and exhaustive stability testing.
9. **Neural-search research:** maintain a separate policy/value plus MCTS branch
   with GPU batching and self-play; compare it fairly with alpha-beta and test
   hybrid uses such as learned move ordering.
10. **Frontier scale:** use community, university and rented compute for larger
    training, distributed analysis and new research while tracking cost,
    provenance and strength per resource.
11. **Longevity:** expand fuzzing, differential and property testing, harden all
    loaders, version every format, and make releases reproducible for future
    maintainers.

The immediate order is calibration, slower confirmation of retained search
work, automated champion/candidate promotion, NNUE v2, then proven parallel
strength. Classical one-thread Forklift remains champion until another
configuration beats it under equal conditions.

The fully expanded programme, exit gates and research references are maintained
in the repository's
[Ultimate Roadmap](https://github.com/tiraaamisuuu/Chess-Engine/blob/main/docs/ULTIMATE_ROADMAP.md).
