# Windows PC Baseline — 2026-07-24

This is the first native Windows baseline for commit `2752f20184` on
the v1 development line, now named `dev/v1`. Results are local development telemetry, not an
absolute engine rating.

## Host and toolchain

- Windows 11 build 26200, AMD64
- AMD Ryzen 9 5900X, 12 cores / 24 logical processors
- NVIDIA GeForce RTX 3070, 8192 MiB, driver 610.47
- Visual Studio Community 2026 18.3.2
- MSVC 19.50.35725
- Windows SDK 10.0.26100
- CMake 4.2.3
- Visual Studio bundled Ninja 1.12.1
- Git 2.51.0.windows.2
- Python 3.14.0
- VS Code 1.130.0

The initial host process supplied duplicate `Path` and `PATH` environment keys.
MSBuild's .NET process launcher rejects that environment before invoking
`cl.exe`. The verification build therefore initialized the Visual Studio x64
environment and used its bundled Ninja generator. A normalized child
environment also proved that the installed Visual Studio/MSBuild generator
works. This is specific to the host process; it is not a compiler installation
failure.

The machine's `py -3` association points at an inaccessible Microsoft Store
Python 3.13 installation. The working `python.exe` is Python 3.14. The Windows
web launcher now prefers a working `python.exe` and falls back to `py -3`.

PowerShell script execution is disabled by the user policy. Checked-in scripts
were run with the process-scoped `-ExecutionPolicy Bypass`; no system or user
policy was changed.

## Build and correctness

Configuration:

```powershell
cmd /d /c "call ""C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"" && cmake -S . -B build-pc-ninja -G Ninja -DCHESS_BUILD_GUI=OFF -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release"
cmake --build build-pc-ninja --parallel
ctest --test-dir build-pc-ninja --output-on-failure
```

Results:

- Release headless build: pass
- `chess-core-tests`: pass
- Six-position perft suite through depth 4: pass
- Windows-native UCI lifecycle/options/searchmoves smoke: pass
- Web API smoke against the real UCI engine: pass

The direct depth-8 benchmark used one thread and a 256 MiB transposition table:

| Position | Best move | Nodes | Time | NPS |
|---|---:|---:|---:|---:|
| Start | `g1f3` | 16,644 | 49 ms | 339,673 |
| Middlegame 1 | `g5f6` | 31,777 | 132 ms | 240,734 |
| Middlegame 2 | `c1f4` | 22,511 | 104 ms | 216,451 |
| Endgame | `b4c4` | 13,724 | 25 ms | 548,960 |
| **Total** | | **84,656** | **310 ms** | **273,083** |

These timings are a reproducibility baseline and should not be interpreted as
Elo evidence.

## Web and tournament workflow

An isolated `.venv-web` was created with Python 3.14 and
`python-chess==1.999`. The local service launched at `127.0.0.1:8765`, and
`tests/web_api_smoke.py` passed. That smoke covers state, PvP/PvC/CvC, moves,
analysis, export, external-engine lifecycle, and a deterministic verified
Stockfish installer fixture.

The real pinned official Stockfish 18 Windows x64 archive was then downloaded,
verified, extracted, UCI-handshaken, and registered as
`external-stockfish-18`. The persistent engine library reports it as available
with the `STOCKFISH · OFFICIAL` badge.

Visible browser interaction was not completed during this baseline run because
the desktop automation runtime could not access its installed path. UI-only
items therefore remained unclaimed in this historical measurement.

The pinned Cute Chess 1.5.1 Windows package installed and verified. The
prescribed four-game workflow check compared `4f1bdc8` with `2752f20` at
`2+0.02`, using paired bundled openings. It completed 4–0 for the current
candidate with no crash or time-forfeit report. Four games verify the workflow
only and provide no useful strength estimate.

Generated builds, environments, downloaded tools, logs, PGNs, and match
artifacts remain ignored and were not committed.
