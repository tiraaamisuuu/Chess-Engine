# Root-parallel equal-time sanity match

Date: 2026-07-24

Branch: `codex/v1-engine-rework`

Runner revision: `38859132d491871f6fa7f5a7f6e4996f936e65c7`

## Purpose

The short thread-scaling benchmark showed that six root workers searched more
nodes per second than one worker on the Ryzen 9 5900X. Node throughput does not
establish playing strength, so the same executable was tested against itself
with only the UCI `Threads` option changed.

## Configuration

```powershell
python scripts\compare_engines.py `
  --baseline-exe .\build-pc-max\Release\chess-engine-uci.exe `
  --candidate-exe .\build-pc-max\Release\chess-engine-uci.exe `
  --baseline-name Single-1T --candidate-name RootSplit-6T `
  --baseline-version 3885913 --candidate-version 3885913 `
  --baseline-threads 1 --candidate-threads 6 `
  --baseline-hash 256 --candidate-hash 256 `
  --games 100 --tc 2+0.02 --concurrency 2 --seed 20260724 `
  --output-dir artifacts\elo\threads-6-vs-1-2s-20260724
```

- Engine SHA-256:
  `a9414b2470a1b3def8c03a7267f5bbffb7556244f4a4e936574cfaeaf47b3654`
- Opening suite: UHO 4060 v4, paired and colour-reversed
- Opening SHA-256:
  `3f499996ff0b674a04f85f2634811d102dd53b5115841e8f11d18e1f550ba2ca`
- Time control: `2+0.02`
- Hash: 256 MiB per engine
- Match concurrency: 2

## Result

| Six-thread wins | One-thread wins | Draws | Six-thread score |
|---:|---:|---:|---:|
| 0 | 100 | 0 | 0.0% |

Cute Chess completed all 100 games. It recorded no crashes, illegal moves, or
disconnects. The six-thread side had one time forfeit; the other 99 losses were
mates or adjudications. Relative Elo is unbounded for a 0% score and therefore
must not be quoted as a finite estimate.

Game records show the six-thread engine repeatedly selecting strategically
losing moves and often shuffling rooks while the one-thread engine searches
several plies deeper. This is not normal statistical noise and a longer match
would not be informative.

## Decision

The current root-split implementation is unsafe for play. `Threads=1` remains
the only supported strength setting while the parallel path is reproduced at
fixed depth and replaced or corrected. The earlier 1.677x six-thread node-rate
measurement is retained strictly as throughput telemetry; it is not evidence
of useful parallel scaling.

