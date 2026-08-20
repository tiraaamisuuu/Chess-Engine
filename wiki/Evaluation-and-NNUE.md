# Evaluation and NNUE

The engine supports two evaluation paths: a hand-written classical evaluator and a custom efficiently updatable neural network (NNUE). Classical evaluation remains the default until a trained NNUE candidate passes the project’s promotion gates.

## Score convention

Evaluation scores are expressed in centipawns from the side-to-move perspective. A positive score favours the player whose turn it is; a negative score favours the opponent. Mate scores use a separate high-value range and encode distance to mate.

## Classical evaluation

The classical evaluator combines:

- material values and piece-square tables;
- mobility and the bishop pair;
- doubled, isolated, and passed-pawn terms;
- rook activity on open and semi-open files;
- king safety; and
- tapered interpolation between middlegame and endgame scores.

This evaluator is deterministic, inexpensive, and remains the reference implementation while neural candidates are trained and tested.

## Network architecture

The NNUE implementation uses a HalfKP-style feature set:

```text
feature index = king square x piece type/colour x piece square
input size    = 64 x 10 x 64 = 40,960
hidden size   = 256 units per king perspective
output input  = white-perspective accumulator + black-perspective accumulator
```

Each perspective has an accumulator containing the input bias plus the active feature weights. The two clipped accumulator vectors are concatenated and passed to the output layer. The exported network is versioned and stores quantized integer weights together with the scales needed by the C++ evaluator.

## Incremental updates

Most moves add and remove only a few active features, so the evaluator updates accumulators instead of rebuilding the full input vector. Captures, promotions, and castling are handled explicitly. A king move changes the king anchor and therefore refreshes the affected perspective.

Accumulator state follows the search stack so make/unmake operations remain exact. Tests compare incremental scores against full rebuilds across ordinary moves and randomized legal sequences.

## Runtime controls

The UCI interface exposes `Use NNUE` and `EvalFile`. A network must pass format and dimension validation before it can be loaded. If NNUE is disabled, missing, or invalid, the engine continues with classical evaluation.

## Promotion policy

A lower validation loss or a faster evaluator is not sufficient by itself. A candidate network must:

1. pass Python-to-C++ score verification;
2. pass quantization and accumulator-correctness tests;
3. meet the required evaluation-throughput budget; and
4. beat the current default in a reproducible paired match with saved artifacts.

The current small training model did not pass the playing-strength gate, so classical evaluation is intentionally still the default. See [NNUE Training](NNUE-Training.md) for the complete pipeline.
