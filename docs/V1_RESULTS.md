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

Finally, search now samples the steady clock every 256 nodes instead of at every
node, and insufficient-material detection exits as soon as it sees a pawn, rook,
or queen. Three interleaved depth-11 comparisons against the fixed-list revision
again kept the exact tree; median runtime fell from 1,465 ms to 1,401 ms (4.4%).
A 50 ms UCI search still stopped at the requested 50 ms on the profiled host.

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

## Node-efficiency match

A second 100-game paired match compared optimized revision `dea7450bba` with
the pre-optimization `814f95f7c5`. It used the same UHO suite, reversed colours,
one thread, 256 MB hash, and a deliberately fast `2+0.02` time control.

| Optimized wins | Pre-optimization wins | Draws | Optimized score | Relative Elo |
|---:|---:|---:|---:|---:|
| 48 | 25 | 27 | 61.5% | +81.4 ± 59.8 |

Cute Chess reported 99.6% likelihood of superiority. The optimized engine
scored 63.0% as White and 60.0% as Black. It also suffered one time forfeit
after spending the entire 20 ms increment on search; the following revision
added a low-clock transport/scheduling reserve. The match is encouraging
evidence that the throughput work converts to strength, but is not a final Elo
claim.

A subsequent 40-game `2+0.02` stress check of the configurable 25 ms default
`Move Overhead` produced zero time forfeits for either engine. Its playing result
(11 wins, 15 losses, 14 draws; -34.9 ±88.8 Elo) was statistically inconclusive,
as expected from a small sample where the safer candidate intentionally searches
less in the final milliseconds. At normal controls the fixed overhead is a much
smaller fraction of the available time.

## Remaining evidence

- Run a longer candidate-versus-baseline SPRT at a slower time control.
- Train at least one HalfKP-v1 candidate and compare classical and NNUE builds.
- Train a properly sized and labelled candidate before enabling NNUE by default.

## Incremental NNUE throughput

The Windows Ryzen 9 5900X pipeline trained and exported a 256-wide HalfKP smoke
network, then searched four fixed positions to depth 7 with one thread and a
64 MB transposition table. Full-rebuild and incremental inference returned the
same moves, scores, and 8,788-node tree in every run.

| NNUE mode | Run 1 | Run 2 | Run 3 | Median NPS |
|---|---:|---:|---:|---:|
| Full rebuild | 13,038 | 13,540 | 13,457 | 13,457 |
| Incremental | 46,252 | 46,010 | 45,298 | 46,010 |

Incremental inference was 3.42x faster at the median. The smoke network was
trained on only 321 positions and is not a strength candidate; the measurement
isolates accumulator cost rather than Elo.

## First NNUE training diagnostic

Eight parallel Stockfish 18 shards labelled 864 locally generated match games
at 5,000 nodes per position. Global merging retained 28,676 training and 3,746
whole-game validation positions after within-shard and cross-shard duplicate
removal.

The original raw-centipawn trainer improved validation RMSE only from 583.24 to
580.95 cp in 12 epochs, and the network lost all 40 paired games against the
classical evaluator. Training in normalized units improved best validation RMSE
to 536.73 cp; raising the integer hidden scale from 127 to 1024 reduced export
RMSE from 8.30 to 1.14 cp. That scaled network still lost all 20 diagnostic
games against classical.

These decisive rejections are useful pipeline evidence, not failed release
claims. HalfKP separates features by king square, so this small corpus of mostly
castled engine-match positions does not cover the feature space or learn robust
material values. A diverse corpus with millions of positions is required before
another strength match is justified.
