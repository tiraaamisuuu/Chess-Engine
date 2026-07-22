# v1 Development Results

Measurements below are development telemetry from the same Apple Silicon host
and may vary with background load. Match results are relative estimates between
the named revisions, not absolute ratings.

## Correctness and build

- Headless and SFML GUI configurations build successfully.
- Core and UCI CTest targets pass.
- Six-position perft suite passes through depth 4.
- Python NNUE scripts pass syntax compilation.

## Move generation

Caching king squares and reusing move buffers raised typical depth-4 perft
throughput from roughly 11–18 million to roughly 24–29 million nodes/second on
the baseline host.

## Classical evaluation throughput

An Apple M3 sampling profile identified pseudo-move generation inside the
classical mobility term as the largest self-time cost. Evaluation only needed
the number of moves, but was building two complete move vectors at every
evaluated node. Replacing that work with a tested allocation-free counter kept
every fixed-depth node count, score, and best move identical.

At depth 12 across the four benchmark positions, revision `4f1bdc80a0` searched
585,267 main nodes in 2,536 ms (230,783 NPS). The allocation-free mobility
counter searched the identical tree in 2,268 ms (258,054 NPS): an 11.8%
throughput increase on the same host. Time-limited results still require paired
matches because greater NPS alone is not an Elo claim.

The next pass moved recursive pseudo/legal, quiescence, tried-move, and perft
lists from heap-backed vectors to a bounded 320-entry stack container. The
known legal-move maximum is 218, and the existing perft suite exercises the
same container. Three interleaved depth-11 comparisons against the mobility-only
revision kept an identical 391,564-node tree; median runtime fell from 2,427 ms
to 2,147 ms, an additional 11.5% reduction on the profiled host.

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

## Preliminary v1 versus v0.4.0 match

A 100-game paired match compared candidate `3e011273bd` with legacy release
`1b52752430` (`v0.4.0`). It used the UHO 4060 opening suite, reversed colours,
one thread, 256 MB hash, and a `2+0.02` time control.

| Candidate wins | Legacy wins | Draws | Candidate score | Relative Elo |
|---:|---:|---:|---:|---:|
| 70 | 13 | 17 | 78.5% | +225.0 ± 73.7 |

Cute Chess reported 100% likelihood of superiority for this sample. The
candidate scored 86.0% as White and 71.0% as Black. One candidate loss was on
time, so this is strong evidence that v1 is materially better, but the exact
Elo figure remains preliminary. A longer SPRT at a slower time control is still
required for a release-grade strength claim.

## Remaining evidence

- Run a longer candidate-versus-baseline SPRT at a slower time control.
- Train at least one HalfKP-v1 candidate and compare classical and NNUE builds.
- Profile accumulator rebuilding before enabling NNUE by default.
