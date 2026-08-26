# Forklift Wiki

This Wiki is the technical manual for Forklift: a C++17 UCI
engine with modern alpha-beta search, classical evaluation, an optional custom
HalfKP NNUE evaluator, a CUDA training pipeline, and reproducible strength
testing.

Classical evaluation is the current default. NNUE is fully integrated but will
not be promoted until a trained candidate beats the classical engine in a
properly powered match.

## Manual

- [Getting Started](Getting-Started.md) — build, run the GUI, use UCI, and
  platform-specific setup
- [Engine Architecture](Engine-Architecture.md) — board and move representation,
  make/unmake, hashing, TT, interfaces, and time management
- [Search](Search.md) — PVS, quiescence, pruning, reductions, extensions, and
  move ordering
- [Evaluation and NNUE](Evaluation-and-NNUE.md) — classical terms, HalfKP,
  accumulators, quantized inference, and UCI selection
- [NNUE Training](NNUE-Training.md) — datasets, Stockfish labels, CUDA training,
  diagnostics, distributed merging, and promotion
- [Testing and Reproducibility](Testing-and-Reproducibility.md) — unit tests,
  perft, benchmarks, paired matches, Elo qualification, and SPRT
- [Experiments and Roadmap](Experiments-and-Roadmap.md) — accepted/rejected
  work, current strongest configuration, and the staged improvement plan
- [Ultimate Roadmap](Ultimate-Roadmap.md) — the open-ended route through
  stronger search, production NNUE, distributed testing and neural self-play
- [Research Notebook](Research-Notebook.md) — preserved evidence and possible
  future research questions without making a paper a present priority
- [Development Workflow](Development-Workflow.md) — repository layout,
  experiment discipline, release gates, and contribution workflow

## Current status

- Canonical branch: `main`; `v1.1.0` is the latest published release
- Supported playing-strength default: one thread, classical evaluation
- Native client: minimalist Forklift desktop app with adaptive time profiles,
  smooth movement, original sound cues, and live telemetry
- Current NNUE format: `HalfKP-v1`; the completed 256-unit production candidate
  remains optional after failing its promotion match
- Teacher budget: 20k nodes selected after 127,784 same-position comparisons
- Completed training scale: five million positions plus 616,632 game-disjoint
  validation positions
- Measured local-pool strength: approximately 2321.5 from a 1,200-game
  Stockfish 18 limited-strength ladder at `10+0.1`; this is not FIDE,
  Chess.com, or universal Elo
- Informal external result: one manually relayed game received a 90.6% Chess.com
  accuracy review and 2750 single-game performance rating; this is anecdotal,
  not an established engine rating

![Forklift v1 release match results](https://raw.githubusercontent.com/tiraaamisuuu/Forklift/main/docs/assets/release-strength.svg)

![Forklift Stockfish calibration curve](https://raw.githubusercontent.com/tiraaamisuuu/Forklift/main/docs/assets/stockfish-calibration.svg)

Raw commands, hardware, commit IDs, checksums, match logs and negative results
remain in the repository's
[`docs/results/`](https://github.com/tiraaamisuuu/Forklift/tree/main/docs/results)
directory. The Wiki explains the system; the result files are the evidence.

Compact research registries, hypotheses and reproducible plotting code live in
the repository's
[research notebook](https://github.com/tiraaamisuuu/Forklift/tree/main/research).
