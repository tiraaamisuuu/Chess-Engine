# Root-parallel safety fix

Date: 2026-07-24

Branch: `codex/v1-engine-rework`

Candidate revision: `4504065`

## Defects

The equal-time sanity match exposed two independent problems in the root-split
search:

1. Root results were selected in raw move-generation order. A fail-low
   null-window result that merely equalled alpha could therefore displace the
   fully searched principal move. This produced conspicuous rook and edge-piece
   shuffling.
2. The engine continued constructing and joining root-worker threads for search
   budgets shorter than their startup cost. Under a `2+0.02` clock this caused
   repeated time forfeits once the remaining time became small.

The correction preserves principal-search order when scores tie and
automatically uses the single-thread path when the hard search budget is below
100 ms. Search telemetry still reports the requested thread count and the
number of workers actually used.

Core regression coverage verifies both the tied-principal case and the
short-budget fallback.

## Stress checks

A 20-game worktree check before the short-budget fallback scored 0 wins,
19 losses, and 1 draw, including 17 six-thread time forfeits.

With both corrections applied, the same 20-game configuration scored 8 wins,
9 losses, and 3 draws, with no time forfeits.

## Clean-revision match

The committed revision was then rebuilt and tested with the same executable on
both sides:

```powershell
python scripts\compare_engines.py `
  --baseline-exe .\build-pc-max\Release\chess-engine-uci.exe `
  --candidate-exe .\build-pc-max\Release\chess-engine-uci.exe `
  --baseline-name Single-1T --candidate-name SafeRootSplit-6T `
  --baseline-version 4504065 --candidate-version 4504065 `
  --baseline-threads 1 --candidate-threads 6 `
  --baseline-hash 256 --candidate-hash 256 `
  --games 100 --tc 2+0.02 --concurrency 2 --seed 20260724 `
  --output-dir artifacts\elo\threads-safety-fix-100g-4504065
```

| Six-thread wins | One-thread wins | Draws | Six-thread score | Relative Elo |
|---:|---:|---:|---:|---:|
| 28 | 37 | 35 | 45.5% | -31.4 ± 55.4 |

Cute Chess completed all 100 games with zero time forfeits, crashes, illegal
moves, or disconnects.

## Decision

The safety regression is resolved: the six-thread path no longer exhibits the
catastrophic move-selection and timing behavior from the original 0/100 match.
The sample does not show a strength gain, so `Threads=1` remains the default.
Revision `0f06dd7` subsequently removed repeated worker creation inside
iterative deepening. See
[`2026-07-24-persistent-root-workers.md`](2026-07-24-persistent-root-workers.md).
