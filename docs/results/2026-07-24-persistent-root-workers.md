# Persistent root-worker optimization

Date: 2026-07-24

Branch: `codex/v1-engine-rework`

Candidate revision: `0f06dd7`

Baseline revision: `95eb16b`

## Change

The previous root-parallel implementation created and joined its full worker
team at every iterative-deepening depth. Revision `0f06dd7` creates one worker
team per move search and dispatches each completed-depth root batch through a
condition-variable barrier.

The short-budget safety fallback remains in place, so searches with a hard
limit below 100 ms still use one worker.

## Interleaved benchmark

Seven alternating A/B pairs used the maximum-performance MSVC build, six
threads, 256 MiB hash, four fixed positions, and 500 ms per position.

- Baseline tools SHA-256:
  `5f587a7d2113afd6d93fc03eb36228751ab411c233b15cab5ba5607b78b73e0e`
- Candidate tools SHA-256:
  `d383325b0d4b317ac92c0fe1a1a1be8b8d3960fb6e89a52c1452b3f16389e316`

| Build | Median NPS | Median mean depth |
|---|---:|---:|
| Per-depth workers | 422,864 | 11.5 |
| Persistent workers | 462,617 | 12.0 |

The candidate produced 1.094x the baseline node rate, a 9.4% increase in this
small sample.

## Paired match

The exact two builds were then tested with both sides at six threads:

```powershell
python scripts\compare_engines.py `
  --baseline-exe .tools\worktrees\pool-baseline\build-pc-max\Release\chess-engine-uci.exe `
  --candidate-exe .\build-pc-max\Release\chess-engine-uci.exe `
  --baseline-name PerDepthWorkers-6T --candidate-name PersistentWorkers-6T `
  --baseline-version 95eb16b --candidate-version 0f06dd7 `
  --baseline-threads 6 --candidate-threads 6 `
  --baseline-hash 256 --candidate-hash 256 `
  --games 100 --tc 2+0.02 --concurrency 2 --seed 20260724 `
  --output-dir artifacts\elo\persistent-workers-vs-95eb16b-100g
```

| Candidate wins | Baseline wins | Draws | Candidate score | Relative Elo |
|---:|---:|---:|---:|---:|
| 35 | 37 | 28 | 49.0% | -6.9 ± 58.2 |

There were no time forfeits, crashes, illegal moves, or disconnects.

## Decision

Keep the persistent worker team for its measurable throughput improvement and
clean failure profile. The 100-game match is statistically neutral, so it is
not evidence of an Elo gain. One thread remains the default pending a properly
powered six-thread-versus-one-thread result.

## Current six threads versus one thread

A following 100-game self-match compared the current executable at six threads
against itself at one thread, retaining the same `2+0.02`, 256 MiB, paired-UHO
configuration.

| Six-thread wins | One-thread wins | Draws | Six-thread score | Relative Elo |
|---:|---:|---:|---:|---:|
| 30 | 47 | 23 | 41.5% | -59.6 ± 61.0 |

There were again no time forfeits, crashes, illegal moves, or disconnects.
This fast-time-control sample is negative evidence for changing the default.
Persistent workers reduce overhead, but independent per-worker transposition
tables and root-only work sharing still fail to turn the additional CPU into
playing strength.
