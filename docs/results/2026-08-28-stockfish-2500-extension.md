# Stockfish 2500 Calibration Extension

This experiment extends Forklift's slower Stockfish 18 limited-strength ladder
to the 2500 rung. It is a rating anchor in this exact local engine pool, not a
FIDE, Chess.com, human, or universal engine rating.

## Contract

- Candidate: Forklift commit `071b4b624629ead2326b8ff03cace89b0520aa1f`
- Opponent: Stockfish 18 with `UCI_LimitStrength=true` and `UCI_Elo=2500`
- Games: 600 with reversed UHO openings
- Time control: `30+0.3`
- Resources: one thread and 256 MiB hash per engine; six games concurrently
- Seed: 2701
- Host: Windows 11, AMD64, AMD Family 25 Model 33, 24 logical CPUs
- Start: 2026-08-28 17:10 BST
- Completion: 2026-08-28 19:31 BST; approximately 2 hours 21 minutes wall time
- Stockfish SHA-256: `9bde420202717ce083412027fbfb8c5c935b537591d712be8a8a8bae92f6e8d6`
- Forklift SHA-256: `3c0b5e1cdfa350be7aa35165f91276662e77105891e6a8984089b24f85651a11`
- Opening-suite SHA-256: `3f499996ff0b674a04f85f2634811d102dd53b5115841e8f11d18e1f550ba2ca`
- Cute Chess SHA-256: `8889f9582dc688c567704cf083f6025baf77f791cde903698c70b3420caf5d7e`

The complete manifest, 600-game PGN, logs, binaries, and JSON results are
retained at
`E:\Dev\Forklift-Research\matches\absolute-calibration-extension-2500-20260828`.

## Primary result

| Stockfish `UCI_Elo` | Forklift wins | Draws | Losses | Score | Relative Elo | Anchored point |
|---:|---:|---:|---:|---:|---:|---:|
| 2500 | 274 | 57 | 269 | 50.4% | +2.9 +/- 26.5 | 2502.9 |

All 600 games completed with zero crashes, illegal moves, or disconnects. Six
games had timeout terminations.

![Forklift slow-control Stockfish calibration curve](../assets/stockfish-calibration-slow.svg)

![Forklift slow-control calibration WDL by rung](../assets/stockfish-calibration-slow-wdl.svg)

## Timeout audit and sensitivity

All six timeout results were on the Stockfish side: five Stockfish time losses
were scored as Forklift wins, while one Stockfish timeout produced a draw. They
were not silently discarded from the primary result.

As a transparent sensitivity check, excluding all six timeout games changes the
result from `274-57-269` over 600 games to **`269-56-269` over 594 games**: an
exactly level 50.0% score. This exclusion is post-hoc and therefore does not
replace the preregistered primary result, but it shows that the timeout anomaly
does not conceal evidence of a clear Forklift advantage.

## Interpretation

The primary local-pool anchor is **2502.9 +/- 26.5 Elo**, and its uncertainty
comfortably includes level. The timeout-excluded sensitivity result is exactly
level. Together these support the conservative statement that Forklift is
approximately level with Stockfish's 2500 limited-strength setting under these
exact conditions.

The observed primary score is still fractionally above 50%, so the formal
ladder status remains `above_range`. A crossing must not be extrapolated beyond
the tested data. The next strict-bracketing measurement is 600 games against
Stockfish 2550 with the same candidate and experimental contract.

## Evidence checksums

- `ladder-manifest.json`: `212c1172c9d131088a605aa17ba6f03f82ec0ad7fb79f96b1e20629ff1bbd056`
- `summary.json`: `97100415e241188d294b1a930bb4c6225fb6ab33594c0e2fcdaa85f303dfad14`
- `report.md`: `3ce77a2fbf7563878e35958409a4200a54c265cac586f58bbeb2c3d94b64635d`
- 2500 PGN: `c8ace3d1c4bea2e93f39e899be0b6d0241207e37aee34cfac3cbd9aeba40ecfc`
