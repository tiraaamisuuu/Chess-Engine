# v1 Development Results

Measurements below are development telemetry from the same Apple Silicon host.
They are not Elo claims and may vary with background load.

## Correctness and build

- Headless and SFML GUI configurations build successfully.
- Core and UCI CTest targets pass.
- Six-position perft suite passes through depth 4.
- Python NNUE scripts pass syntax compilation.

## Move generation

Caching king squares and reusing move buffers raised typical depth-4 perft
throughput from roughly 11–18 million to roughly 24–29 million nodes/second on
the baseline host.

## Root parallelism

The original four-thread implementation searched every root move independently
at full width. At a one-second limit it reached 3–4 fewer plies than one thread.

The v1 root split searches the principal move first, shares its alpha between
workers, and uses null-window searches for the remaining moves. A representative
one-second run produced:

| Position | v1 1 thread | v1 4 threads |
|---|---:|---:|
| Start | 13 | 12 |
| Middlegame 1 | 12 | 13 |
| Middlegame 2 | 12 | 12 |
| Endgame | 15 | 16 |

This removes the catastrophic scaling regression, but one thread remains the
default until paired opening-suite matches establish a multi-thread Elo gain.

## Remaining evidence

- Run candidate-versus-baseline SPRT with `cutechess-cli` and a large balanced
  opening suite.
- Train at least one HalfKP-v1 candidate and compare classical and NNUE builds.
- Profile accumulator rebuilding before enabling NNUE by default.
