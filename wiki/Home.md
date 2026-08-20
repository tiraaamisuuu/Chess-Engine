# Chess Engine Wiki

This Wiki is the technical manual for the Chess Engine project: a C++17 UCI
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
- [Development Workflow](Development-Workflow.md) — repository layout,
  experiment discipline, release gates, and contribution workflow

## Current status

- Development branch: `dev/v1`
- Supported playing-strength default: one thread, classical evaluation
- Current NNUE format: `HalfKP-v1`, 256 hidden units for the next baseline
- Immediate experiment: compare Stockfish labels at 5k and 20k nodes on the
  same diverse positions
- Next training scale: five million training positions plus game-disjoint
  validation

Raw commands, hardware, commit IDs, checksums, match logs and negative results
remain in the repository's
[`docs/results/`](https://github.com/tiraaamisuuu/Chess-Engine/tree/dev/v1/docs/results)
directory. The Wiki explains the system; the result files are the evidence.
