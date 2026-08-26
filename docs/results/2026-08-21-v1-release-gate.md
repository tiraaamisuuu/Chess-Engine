# v1 Release Strength Gate — 2026-08-21

## Decision

The classical v1 engine passes the playing-strength gate against `v0.4.0` and
is ready for a release pull request. NNUE remains optional and disabled by
default; its separate rejection does not weaken the classical release result.

## Frozen engines and environment

The match runner was clean commit `4cb398f6f044b4f24c8837439a3ccb1eeed5b3c1`.
Both engines were built from Git archives with MSVC 19.50 on Windows 11 and ran
on an AMD Ryzen 9 5900X host with 24 logical processors.

| Side | Git commit | Executable SHA-256 |
|---|---|---|
| v1 candidate | `4cb398f6f044b4f24c8837439a3ccb1eeed5b3c1` | `7157a2c607c71f9048b8ed7c3264c185fced44686d34db3299453f01469526d1` |
| `v0.4.0` | `1b52752430316cde9d2299dc5450102187369d28` | `f02c90c8945ab2998ed484c8a091a2e86a924dae88c05208e1d86079282ada49` |

The legacy tag still self-reports `TiramisuChess v0.5.0-dev`; the immutable Git
tag and commit above define the tested baseline. SFML 2.6.2 was built locally
only to compile and launch that legacy UCI-in-GUI executable.

Commit `30d49b1` subsequently changed only the v1 UCI/dashboard product labels
from development text to `v1.0.0`; the rebuilt CTest, UCI, Node, and web smoke
gates passed after that identity-only change.

## Match contract

- 400 games, fixed count rather than early-stopping SPRT
- UHO 4060 v4 opening suite, random order and reversed colours
- opening SHA-256:
  `3f499996ff0b674a04f85f2634811d102dd53b5115841e8f11d18e1f550ba2ca`
- `10+0.1` time control
- one thread and 256 MiB hash per engine
- concurrency four; deterministic seed 83
- Cute Chess 1.5.1 SHA-256:
  `8889f9582dc688c567704cf083f6025baf77f791cde903698c70b3420caf5d7e`

## Result

| v1 wins | `v0.4.0` wins | Draws | v1 score | Relative Elo | LOS |
|---:|---:|---:|---:|---:|---:|
| 299 | 18 | 83 | 85.1% | +303.0 +/- 37.1 | 100.0% |

Colour split:

| v1 colour | Wins | Losses | Draws | Score |
|---|---:|---:|---:|---:|
| White | 165 | 5 | 30 | 90.0% |
| Black | 134 | 13 | 53 | 80.2% |

There were zero time forfeits, crashes, illegal moves, or disconnects. The
result is relative to this release baseline and test contract, not an absolute
rating or a promise of +303 Elo at every time control.

## Quality gates

Local verification passed:

- Visual Studio Release headless configure and build
- core and UCI CTest targets
- six-position perft through depth 4
- match, calibration, and thread-tool Python tests
- 15 NNUE dataset and 14 NNUE training tests
- Python syntax compilation, Node syntax check, and web API smoke
- direct `v1.0.0` UCI handshake

GitHub Actions on `4cb398f` passed Ubuntu, macOS, Windows, sanitizers, and the
web GUI job. A conflict-free merge tree against `origin/main` was also verified.

## Release conclusion

The evidence supports releasing the classical v1 configuration. The next
external action is a reviewed merge of `dev/v1` into `main`, followed by a
`v1.0.0` tag and release package. Those repository-wide actions are deliberately
separate from the strength experiment.

## Publication outcome

The reviewed merge, `v1.0.0` tag, and cross-platform release publication were
subsequently completed. `main` is now the canonical development and release
branch; the text above is retained as the contemporaneous gate decision.
