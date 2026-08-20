# v1 Rework Baseline

The v1 development line, now named `dev/v1`, started from commit `518a3c1` on
2026-07-22.
These measurements are a reproducible reference, not a claimed playing-strength
rating.

## Correctness

- Standard six-position perft suite: passes through depth 4.
- UCI smoke test: passes.
- Strict Clang build: succeeds, with signed-index and header-local unused-code
  warnings.

## Search benchmark

Command:

```sh
./gui --bench --bench-depth 64 --bench-time 1000 --bench-tt 256 --threads N
```

Depth reached at a one-second hard limit:

| Position | 1 thread | 4 threads |
|---|---:|---:|
| Start | 14 | 10 |
| Middlegame 1 | 9 | 6 |
| Middlegame 2 | 10 | 7 |
| Endgame | 16 | 11 |

The original root-parallel implementation is therefore not an optimization:
at equal time it reaches substantially less depth. It must not be used as the
v1 default unless a replacement demonstrates positive scaling.

## Known baseline risks

- Piece-square table orientation does not match the `a1 == 0` board layout.
- En-passant state is hashed even when no capture is available.
- Opposite-coloured `K+B vs K+B` positions are treated as dead positions.
- GUI game termination does not consistently adjudicate rule draws.
- Core/search headers depend on SFML, preventing a truly headless build.
- Search benchmarks have no strength assertions, and Elo matches have no
  opening-position suite by default.

