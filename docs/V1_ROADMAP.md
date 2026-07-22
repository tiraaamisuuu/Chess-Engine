# TiramisuChess v1 Roadmap

v1 is an incremental internal rebuild in the existing repository. The current
engine remains the correctness and playing-strength baseline while components
are replaced behind tests.

1. Make the chess core, UCI engine, GUI, tests, and benchmarks independent
   build targets.
2. Add deterministic tests for move generation, make/unmake, hashing, FEN,
   notation, rule draws, and UCI lifecycle behaviour.
3. Correct known evaluation, hashing, draw, time-management, and protocol bugs.
4. Require paired opening-suite matches and SPRT evidence for search/evaluation
   changes.
5. Improve node efficiency before adding more selective-search heuristics.
6. Replace root parallelism only when equal-time benchmarks show useful scaling.
7. Introduce tapered and automatically tuned classical evaluation.
8. Add a versioned NNUE pipeline after the core can export labelled positions,
   load networks safely, and compare them against the classical evaluator.

Release criteria are clean builds, passing CI and quality gates, no known rule
correctness regressions, and measured strength improvement over the baseline.

