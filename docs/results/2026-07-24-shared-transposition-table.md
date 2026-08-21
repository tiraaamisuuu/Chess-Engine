# Shared transposition table

Date: 2026-07-24

Branch: `dev/v1` (renamed after this measurement)

Candidate revision: `8811c0e`

Baseline revision: `64c26da`

## Change

Root workers previously divided the configured hash memory into isolated
per-worker transposition tables. The candidate instead shares the full table
between workers.

Entries use two atomic 64-bit words: one packed data word and one key checksum.
Writers publish the packed data before its key checksum; readers accept only a
complete matching snapshot. Concurrent writers may cause a clean miss, which
is safe for search, but cannot expose a partially mixed entry as a hit. The
four-entry clustered replacement policy remains in place.

The packed data stores score, depth, generation, bound type, and the root fields
of the best move. Core tests cover negative-score round trips, packed fields,
cluster replacement, and heavy concurrent collisions.

## Single-thread cost

Seven alternating fixed-depth A/B pairs searched the exact same 270,343-node
depth-10 tree in every run.

| Table | Median time | Relative time |
|---|---:|---:|
| Plain per-worker entry | 966 ms | 1.000x |
| Atomic packed entry | 983 ms | 1.018x |

The concurrency-safe representation cost approximately 1.8% in this small
single-thread sample.

## Thread scaling

The maximum-performance build used four fixed positions, 500 ms per position,
256 MiB hash, and three repetitions.

Candidate tools SHA-256:
`90991477d0ce2ed62a19ffbd797577040ee97c5739f839242e55ac4fb5af30f3`

| Threads | Median NPS | Median depth | Speedup | Efficiency |
|---:|---:|---:|---:|---:|
| 1 | 359,301 | 11.0 | 1.000x | 100.0% |
| 2 | 418,235 | 11.0 | 1.164x | 58.2% |
| 4 | 471,292 | 12.0 | 1.312x | 32.8% |
| 6 | 541,356 | 12.0 | 1.507x | 25.1% |
| 12 | 504,544 | 12.0 | 1.404x | 11.7% |

At fixed depth 8, six threads matched the one-thread score on three of four
positions and differed by one centipawn on the fourth. Before sharing the
table, worker-specific search information produced much larger score
differences.

## Equal-time matches

All matches used paired UHO openings, `2+0.02`, 256 MiB hash per engine,
two-game concurrency, and 100 games. Every match completed with zero time
forfeits, crashes, illegal moves, or disconnects.

### Shared versus isolated table

Both sides used six threads. This isolates revision `8811c0e` against
`64c26da`.

| Shared wins | Isolated wins | Draws | Shared score | Relative Elo |
|---:|---:|---:|---:|---:|
| 41 | 33 | 26 | 54.0% | +27.9 ± 59.2 |

The central result favours shared search information, but the 100-game sample
is not statistically decisive.

### Current thread-count sweep

Each multi-thread setting played the same current executable at one thread.

| Candidate | Wins | Losses | Draws | Score | Relative Elo |
|---|---:|---:|---:|---:|---:|
| 2 threads | 33 | 35 | 32 | 49.0% | -6.9 ± 56.6 |
| 4 threads | 36 | 36 | 28 | 50.0% | 0.0 ± 58.2 |
| 6 threads | 33 | 40 | 27 | 46.5% | -24.4 ± 58.7 |

Four threads is the best tested multi-core setting at this fast control.
None of these samples proves a gain over one thread, so one thread remains the
default. Four threads is appropriate for continued experimental analysis and a
longer, slower SPRT.
