# Stockfish 18 Slow-Control Calibration

This experiment tests Forklift near the first ladder's estimated boundary at a
three-times-longer base time and with three times as many games per rung. It is
a rating anchor in this exact local engine pool, not a FIDE, Chess.com, human,
or universal engine rating.

## Contract

- Candidate: Forklift commit `071b4b624629ead2326b8ff03cace89b0520aa1f`
- Release relation: subsequent v1.1.0 commits changed branding, documentation,
  and packaging, not the search or evaluation used in this match
- Opponent: Stockfish 18 with `UCI_LimitStrength=true`
- Rungs: 2250, 2300, 2350, and 2400 `UCI_Elo`
- Games: 600 per rung; 2,400 total with reversed UHO openings
- Time control: `30+0.3`
- Resources: one thread and 256 MiB hash per engine; six games concurrently
- Seed: 2701
- Host: Windows 11, AMD64, AMD Family 25 Model 33, 24 logical CPUs
- Start: 2026-08-26 22:22 BST
- Completion: 2026-08-27 07:31 BST; approximately 9 hours 9 minutes wall time
- Stockfish SHA-256: `9bde420202717ce083412027fbfb8c5c935b537591d712be8a8a8bae92f6e8d6`
- Forklift SHA-256: `3c0b5e1cdfa350be7aa35165f91276662e77105891e6a8984089b24f85651a11`
- Opening-suite SHA-256: `3f499996ff0b674a04f85f2634811d102dd53b5115841e8f11d18e1f550ba2ca`
- Cute Chess SHA-256: `8889f9582dc688c567704cf083f6025baf77f791cde903698c70b3420caf5d7e`

The complete manifests, 2,400-game PGN set, logs, binaries, and JSON results
are retained at
`E:\Dev\Forklift-Research\matches\absolute-calibration-slow-20260826`.

## Results

| Stockfish `UCI_Elo` | Forklift wins | Draws | Losses | Score | Relative Elo | Anchored point |
|---:|---:|---:|---:|---:|---:|---:|
| 2250 | 385 | 42 | 173 | 67.7% | +128.3 +/- 28.6 | 2378.3 |
| 2300 | 365 | 43 | 192 | 64.4% | +103.1 +/- 27.9 | 2403.1 |
| 2350 | 360 | 33 | 207 | 62.7% | +90.6 +/- 27.9 | 2440.6 |
| 2400 | 272 | 59 | 269 | 50.2% | +1.7 +/- 26.4 | 2401.7 |

All 2,400 games completed with zero crashes, time forfeits, illegal moves, or
disconnects.

![Forklift slow-control Stockfish calibration curve](../assets/stockfish-calibration-slow.svg)

![Forklift slow-control calibration WDL by rung](../assets/stockfish-calibration-slow-wdl.svg)

## Interpretation

At the most informative rung, Forklift was effectively level with Stockfish's
2400 limited-strength setting: `272-59-269`, a 50.2% score and a relative result
of **+1.7 +/- 26.4 Elo**. Directly anchoring that one match gives 2401.7, with a
simple sampling interval of approximately 2375.3 to 2428.1 in this pool.

The formal ladder result remains `above_range`, because the observed score was
fractionally above 50% at every configured rung. An interpolated crossing must
not be invented without a rung on the other side of 50%; a 2450 or 2500
extension is the clean next measurement.

The result nevertheless supplies strong evidence that Forklift is roughly
level with the 2400 setting under these exact conditions. It does not replace
the approximately 2321.5 `10+0.1` crossing with a universal number. The change
between time controls may reflect Forklift's scaling, Stockfish's
limited-strength behaviour, or both. The non-monotonic anchored values at the
lower rungs are another reason to prefer the direct near-boundary result and to
retain the full qualification.

The subsequent [2450 extension](2026-08-27-stockfish-2450-extension.md)
finished `280-69-251` for Forklift and moved the tested boundary above 2450;
the formal crossing remains open pending the 2500 rung.

## Evidence checksums

- `ladder-manifest.json`: `85e96889473462d41a442bf5d54fcf047eb0938ecc6276953740e922062f8c68`
- `summary.json`: `b14b99c977e40660077bb34c5644cdf2e13d0d7633e82a447b584d6b8c6caa4a`
- 2250 PGN: `85f42fc66d238c3634b51c1533cf2143599fbd3619244fbb2082bbf07cf0600e`
- 2300 PGN: `2c31074b54075b68de4c88f6b91911f947ed011033900e159c3457cbe40c5537`
- 2350 PGN: `aa568e25cc6579ae01be60de353482d3693282aa7c505bf6c6dcb8c933a59b44`
- 2400 PGN: `654d9b5428a0866618330bc2f947f36dcd85cbc32d479624d10a32be18e7c55d`
