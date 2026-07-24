# Windows maximum-performance and thread-scaling baseline

Date: 2026-07-24

Branch: `codex/v1-engine-rework`

Host: Ryzen 9 5900X, Windows 11, MSVC 19.50

## Maximum-performance build

The opt-in build enables:

- MSVC AVX2 code generation for the local x64 CPU
- release interprocedural/link-time optimization
- the normal headless core and UCI tests

Command:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_max_performance.ps1
```

The resulting binary is native to the build PC and is not a portable release
artifact.

An interleaved five-pair fixed-depth probe compared the ordinary MSVC Release
build with AVX2 plus IPO/LTO at depth 10, one thread, 256 MB hash, and a
15-second safety limit per position. Both builds searched the same 270,343
nodes. Median total time changed from 1,129 ms to 1,092 ms, approximately 3.3%.
The sample is small and subject to normal desktop noise, so this supports an
opt-in build mode rather than a strength claim.

## Thread scaling

The committed diagnostic command is:

```powershell
python scripts\benchmark_threads.py `
  --engine .\build-pc-max\Release\chess-engine-tools.exe `
  --threads 1,2,4,6,12 `
  --repetitions 3 --time-ms 500 --depth 64 --hash 256 `
  --output-dir artifacts\perf\2026-07-24-thread-scaling-max
```

Executable SHA-256:
`46eb0316d5241a7fdea7cc2172d7c4fc2fd95892640ec3e15f5906680ef4f23e`

| Threads | Median NPS | Speedup | Efficiency |
|---:|---:|---:|---:|
| 1 | 270,354 | 1.000x | 100.0% |
| 2 | 270,661 | 1.001x | 50.1% |
| 4 | 356,689 | 1.319x | 33.0% |
| 6 | 453,285 | 1.677x | 27.9% |
| 12 | 423,202 | 1.565x | 13.0% |

Six threads produced the best median node throughput in this short diagnostic.
The poor efficiency and twelve-thread regression justify further work on root
parallelism and shared search information.

Node rate is not chess strength. One thread remains the default until a
multi-thread setting wins equal-time paired matches.

## Web thinking countdown

Timed engine moves now show:

- a radial countdown and animated search activity over the board in PvC
- a compact remaining-time display in the live-position card
- the remaining time on the active engine's player strip
- a finalising state when the requested move time expires before a result

Desktop interaction, completion cleanup, console errors, and the responsive
mobile breakpoint were checked against the running local web service.
