# v1 Development Results

Measurements below are development telemetry from the named Apple Silicon or
Windows host and may vary with toolchain and background load. Match results are
relative estimates between the named revisions, not absolute ratings.

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

A second experiment streamed the checksum-verified January 2013 Lichess
standard archive under CC0. Eight Stockfish 18 shards sampled 50,000 games at
5,000 nodes per label and completed in 718.6 seconds on the Ryzen 9 5900X.

| Split | Retained positions | SHA-256 |
|---|---:|---|
| Training | 158,699 | `ab0dd1df8158fbd6898282c794f61b5b9bdba25bd5a864775d5548c71c67235c` |
| Validation | 17,543 | `bef55492464d9dbda6df438a938a894f611227486302926039d352adaba95d9e` |

The 256-wide model early-stopped at epoch 15 (best epoch 10), reaching 454.31 cp
validation RMSE at roughly 16.7k training positions/second. Integer export RMSE
was 1.68 cp across 512 samples. Network SHA-256:
`90310fb9de8a5e913c811a8829a4bc9792f09b6e9e55ec9eff9ca5743b6dfe67`.

Its 40-game paired diagnostic against classical at `2+0.02` scored 2 wins,
37 losses, and 1 draw (6.25%, approximately -470 Elo in this small low-draw
sample). There were no crashes or illegal moves; the NNUE side had one time
forfeit. Diversity clearly helped, but 176k total positions remain far below
the millions needed to cover king-conditioned HalfKP features. The network is
rejected and classical remains the default.

The training split covered 29,421 of 40,960 HalfKP inputs (71.8%). Of the full
feature set, 11,539 inputs were unseen and 25,954 occurred at most 100 times.
This is direct coverage evidence for scaling to millions of positions rather
than tuning more epochs on this model.

## Windows search pass — 2026-08-20

The Windows Ryzen 9 5900X search pass tested each material search change in
isolation. Incremental null moves preserved the 270,343-node depth-10 tree and
reduced median runtime from 5,108 to 4,951 ms (3.1%). Qsearch SEE pruning then
reduced the tree to 244,274 nodes and scored `38-28-34` over 100 paired fast
games (55.0%, approximately +34.9 ±55.9 Elo, LOS 89.1%), so it was retained.

Pawn correction history slowed the benchmark and scored exactly `34-34-32`
over 100 games; it was reverted. A selection-scan replacement for full sorting
was about 17.5% slower and was discarded before commit.

Compact continuation history reduced the depth-10 tree further to 186,618
nodes. Its 100-game diagnostic scored `37-31-32` (53.0%, approximately
+20.9 ±56.6 Elo, LOS 76.7%). It remains enabled provisionally, with a slower
and longer paired test still required.

Three current depth-10 runs returned the identical 186,618-node tree in 3,976,
3,898, and 3,915 ms. The median was 3,915 ms (47,667 NPS) under the Visual
Studio Release build. Full commands and caveats are recorded in
`docs/results/2026-08-20-windows-search-and-nnue-tooling.md`.

## Scalable NNUE data tooling — 2026-08-20

The coordinator now supports exact target-sized training merges, per-worker
headroom, progress/rate/ETA output, and optional same-position comparison at two
teacher node budgets. Training metrics now include validation MAE and sign
accuracy in addition to RMSE, plus batched error slices by side-to-move king
square, phase, material imbalance, and teacher-evaluation magnitude. A streaming
audit reports HalfKP feature frequency/coverage and position distributions,
while a portable merger combines
checksummed Windows/Linux bundles under strict teacher and game-split contracts
with global deduplication and train/validation leakage prevention.

A two-worker Stockfish 18 smoke retained exactly 100 requested training and 92
validation positions. Across 224 pre-merge labels, the 50-vs-100-node teacher
comparison measured 22.75 cp MAE, 42.17 cp RMSE, and 96.88% score-sign
agreement. Re-merging the two worker manifests reproduced the exact output
checksums. The new audit also reproduced the earlier January 2013 corpus
coverage exactly: 29,421/40,960 inputs (71.83%).

The grouped-error path was exercised through a separate real train, quantize,
export, and C++ verification smoke. No new production network was trained from
this smoke data. The next strength
experiment is the documented five-million-position, 20k-node, 256-wide
baseline; classical evaluation remains the default.

## Stockfish teacher-budget decision — 2026-08-20

Eight hash-isolated Stockfish 18 workers compared 5,000 and 20,000 nodes on
127,784 identical positions sampled deterministically from the January 2013
Lichess standard-rated CC0 archive. The run retained exactly 100,000 training
and 12,572 game-disjoint validation records.

The deeper labels differed by 366.40 cp MAE and 3,260.28 cp RMSE, with a
maximum 32,540 cp difference. Score signs agreed on 97.499% of positions;
3,196 positions (2.501%) changed which side was favoured. The result was
consistent across all eight shards. At four times the fixed-node work, a 5M
20k run projects to about 21 hours on the measured Ryzen 9 5900X configuration.

The project therefore selected 20k labels for the first 5M, 256-wide HalfKP
baseline. At that checkpoint production generation had not started because a
modern monthly archive was not present locally. Full provenance, command,
per-shard ranges, caveats, and output hashes are in
`docs/results/2026-08-20-stockfish-teacher-budget.md`.

## Five-million-position NNUE result — 2026-08-21

The July 2026 Lichess standard-rated CC0 archive produced exactly 5,000,000
Stockfish-18-labelled training positions plus 616,632 game-disjoint validation
positions at 20,000 nodes per label. The audit covered 37,890/40,960 HalfKP
inputs (92.50%); 3,070 remained unseen and 21,184 occurred at most 100 times.

The original 256-wide network passed quantization and exact C++ verification,
but scored `102-242-56` against classical over 400 paired `10+0.1` games (32.5%,
-127.0 +/- 33.4 Elo). There were no technical failures, so it was rejected on
strength rather than correctness.

An opt-in probability-space WDL objective improved the best pure-network
40-game screen to `15-20-5` (43.8%). A new tested `NNUE Weight` UCI option then
allowed classical/NNUE blends. The most promising 50% blend scored `16-15-9`
in its initial 40-game screen, but a fresh-seed 200-game confirmation scored
`76-82-42` (48.5%, -10.4 +/- 43.0 Elo, LOS 31.7%) with zero failures. It also
searched only 33,728 NPS at fixed depth 8 versus 62,682 for classical because
both evaluators run at every leaf.

The full pipeline is proven, but no model earned promotion. Classical remains
the release evaluator. Data hashes, network hashes, validation/quantization
metrics, frozen binary identity, all screening results, and the decision are
recorded in `docs/results/2026-08-21-five-million-nnue.md`.
