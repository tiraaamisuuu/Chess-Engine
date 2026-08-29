# Stockfish 2550 Calibration Extension

This experiment extends Forklift's slower Stockfish 18 limited-strength ladder
to the 2550 rung and closes the first strict bracket around its 50% crossing.
It is a rating estimate in this exact local engine pool, not a FIDE, Chess.com,
human, or universal engine rating.

## Contract

- Candidate: Forklift commit `071b4b624629ead2326b8ff03cace89b0520aa1f`
- Opponent: Stockfish 18 with `UCI_LimitStrength=true` and `UCI_Elo=2550`
- Games: 600 with reversed UHO openings
- Time control: `30+0.3`
- Resources: one thread and 256 MiB hash per engine; six games concurrently
- Seed: 2701
- Host: Windows 11, AMD64, AMD Family 25 Model 33, 24 logical CPUs
- Start: 2026-08-29 17:14 BST
- Completion: 2026-08-29 19:34 BST; approximately 2 hours 21 minutes wall time
- Stockfish SHA-256: `9bde420202717ce083412027fbfb8c5c935b537591d712be8a8a8bae92f6e8d6`
- Forklift SHA-256: `3c0b5e1cdfa350be7aa35165f91276662e77105891e6a8984089b24f85651a11`
- Opening-suite SHA-256: `3f499996ff0b674a04f85f2634811d102dd53b5115841e8f11d18e1f550ba2ca`
- Cute Chess SHA-256: `8889f9582dc688c567704cf083f6025baf77f791cde903698c70b3420caf5d7e`

The complete manifest, 600-game PGN, logs, binaries, and JSON results are
retained at
`E:\Dev\Forklift-Research\matches\absolute-calibration-extension-2550-20260829`.

## Primary result

| Stockfish `UCI_Elo` | Forklift wins | Draws | Losses | Score | Relative Elo | Anchored point |
|---:|---:|---:|---:|---:|---:|---:|
| 2550 | 236 | 48 | 316 | 43.3% | -46.6 +/- 26.9 | 2503.4 |

All 600 games completed with zero time forfeits, crashes, illegal moves, or
disconnects.

![Forklift slow-control Stockfish calibration curve](../assets/stockfish-calibration-slow.svg)

![Forklift slow-control calibration WDL by rung](../assets/stockfish-calibration-slow-wdl.svg)

## Bracketed estimate

The adjacent rungs now straddle 50%:

- 2500: 50.4% (`274-57-269`), with a direct anchored point of 2502.9.
- 2550: 43.3% (`236-48-316`), with a direct anchored point of 2503.4.

Linear interpolation gives 2502.8 when using the published one-decimal score
percentages and 2502.9 when using the exact WDL fractions. Reporting more
precision than the measurements justify would be misleading, so the result is
stated as **approximately 2503 Elo**. The close agreement between the two
independent anchored points is reassuring, although it does not remove the
reported sampling uncertainty or the limitations of Stockfish's
limited-strength pool.

The 2500 rung contained six Stockfish-side timeout results. Excluding those six
games as a declared post-hoc sensitivity check makes the 2500 result exactly
50.0% (`269-56-269`) and therefore places the conservative crossing at
**2500.0**. The primary and sensitivity analyses support the compact claim that
Forklift is approximately **2503 local-pool Elo**, or approximately **2500**
under the conservative timeout treatment, for this exact contract.

Across the complete slower campaign, Forklift played 4,200 games over seven
rungs from 2250 through 2550. Results from the separate `10+0.1` campaign must
not be pooled with this estimate because its time control, candidate build, and
tested range differ.

## Evidence checksums

- `ladder-manifest.json`: `c4c7c511d41c43cb2dc985fe74185e160a4bdb201c27ebcc9594fed5c0cb793c`
- `summary.json`: `8e88216d118cbdb5f3a88ff770395715fda891a8996bdf7d56042c731dc0d3d1`
- `report.md`: `73d0cab663429c28896818cb2b9afae37df229e407b297ad28fe182315416ab2`
- 2550 PGN: `1be385f434cb19750aa92e615e4ab384d443cfbbd020c7941fbcd35cc47e9826`
