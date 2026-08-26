# Forklift desktop overhaul

Date: 2026-08-26

Branch during implementation: `dev/v1`, subsequently integrated into `main`

Final feature commit: `92e57a3`

## Scope

The native SFML client was rebuilt as a restrained Forklift desktop
application without changing the UCI engine's proven defaults. The completed
work includes:

- a minimalist tactile layout with corrected coordinate and control alignment;
- 190 ms cubic ease-out piece movement and animated undo;
- original procedurally generated move, capture, check, and game-over cues;
- a native multi-resolution Windows icon and packaged SFML Audio/OpenAL runtime;
- removal of the user-facing depth ceiling in favour of time-driven search;
- manual time limits with a visible bounded negative overrun;
- position-aware automatic timing with four profiles; and
- corrected process CPU measurement normalized to whole-machine utilization.

## Automatic timing contract

The user selects a base time and an absolute automatic ceiling up to five
minutes. Position complexity is combined with the selected profile:

| Profile | Complexity weight | Share of automatic ceiling |
|---|---:|---:|
| Eco | 55% | 10% |
| Balanced | 85% | 25% |
| Performance | 130% | 50% |
| Performance+ | 180% | 100% |

The displayed suggestion is the search soft limit. A bounded hard limit permits
the current iterative-deepening pass to finish; the countdown crosses through
zero and shows that overrun as negative time.

## Verification

- MSVC Release GUI and headless targets built successfully.
- `chess-core-tests` and `uci-smoke` passed.
- All six canonical perft positions passed through depth 4.
- The icon generation utility passed Python bytecode compilation.
- A staged install contained `gui.exe`, the four SFML runtime DLLs,
  `openal32.dll`, piece assets, and the application icon.
- Live Windows testing covered menu/game layout, manual/automatic switching,
  all four profiles, position-dependent suggestions, an out-of-book search,
  piece movement, resource telemetry, and a bounded 10.0-to-13.333-second
  search overrun.
- GitHub Actions run
  [32970142988](https://github.com/tiraaamisuuu/Chess-Engine/actions/runs/32970142988)
  passed Ubuntu, macOS, Windows, sanitizers, and web GUI jobs.

This is a usability and time-control milestone, not a playing-strength claim.
The classical evaluator and one search thread remain the supported defaults.
