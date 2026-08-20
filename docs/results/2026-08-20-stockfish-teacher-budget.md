# Stockfish Teacher-Budget Comparison — 2026-08-20

## Decision

Use **20,000 Stockfish nodes per position** for the first five-million-position
HalfKP baseline.

The 20k label costs four times the fixed-node work of a 5k label, but the
deeper search changed the score sign on 3,196 of 127,784 positions (2.50%). The
measured one-machine projection is about 21 hours for five million 20k labels,
which is acceptable for a production dataset and materially safer than
retaining the shallower labels. This decision applies to the planned fixed-size
5M baseline; it does not prove that 20k is optimal for every future data mix.

## Scope

This experiment measures Stockfish label stability, not playing Elo. The same
deterministically sampled board was searched first at 5,000 nodes and then at
20,000 nodes. Stockfish's UCI hash was cleared before each budget, preventing
the 20k search from reusing work from the 5k search.

The locally available reservoir was the January 2013 Lichess standard-rated
CC0 archive. It is diverse enough for the budget diagnostic, but it is not the
modern monthly archive required for the production 5M run.

## Environment and provenance

- Host: Windows, AMD Ryzen 9 5900X (12 cores / 24 logical processors)
- Generator commit: `10e1b39a426a10109aad346a487bcbc2de7bcdf2`
- Generator script SHA-256:
  `7d1cba7b7dea586ff02999cf071bcf1cecf21861c8b8b9c8d1534d236af0a92a`
- Teacher: Stockfish 18
- Teacher executable SHA-256:
  `9bde420202717ce083412027fbfb8c5c935b537591d712be8a8a8bae92f6e8d6`
- Source archive size: 17,761,302 bytes
- Source archive SHA-256:
  `aa40b3671fa3cf1072eb182892cd90b0e1e003a4a5943492f64b77e7f3fd1635`
- Licence: CC0
- Seed: `20260820`
- Sampling: 2%, ply 8–180, deterministic eight-way game partition
- Validation: 10% assigned by whole game
- Concurrency: eight teachers, two threads and 128 MiB hash each
- Hash isolation: `clear-before-each-budget`

## Command

```powershell
.\.venv-nnue\Scripts\python.exe scripts\nnue\generate_shards.py `
  --engine .tools\user-engines\external-stockfish-18\stockfish-windows-x86-64.exe `
  --pgn data\sources\lichess_db_standard_rated_2013-01.pgn.zst `
  --output-dir data\teacher-quality-100k-sf18-5k-vs-20k-20260820 `
  --shards 8 --jobs 8 --threads 2 --hash 128 `
  --source-name "Lichess standard rated 2013-01 teacher comparison" `
  --source-license CC0 `
  --source-url https://database.lichess.org/standard/lichess_db_standard_rated_2013-01.pgn.zst `
  --nodes 5000 --comparison-nodes 20000 `
  --sample-rate 0.02 --validation-fraction 0.1 --seed 20260820 `
  --target-training-positions 100000 --target-oversample 1.15
```

## Results

The eight workers produced 127,784 paired comparisons in 2,433.607 seconds.
The merged dataset retained exactly 100,000 training records and 12,572
game-disjoint validation records.

| Metric | 5k vs 20k result |
|---|---:|
| Same-position comparisons | 127,784 |
| Mean difference (`20k - 5k`) | +52.21 cp |
| Mean absolute difference | 366.40 cp |
| Root-mean-square difference | 3,260.28 cp |
| Maximum absolute difference | 32,540 cp |
| Score-sign agreement | 97.499% |
| Score-sign disagreements | 3,196 (2.501%) |

The very large RMSE and maximum difference are driven by rare high-magnitude
scores. A difference above 32,000 requires opposite score signs, and mates are
converted to ±32,000 cp, so mate discovery is a likely contributor to these
outliers. The aggregate format does not retain mate flags or enough information
to report a median or percentile, so neither is invented.

The result was consistent across all eight independent shards:

| Per-shard range | Minimum | Maximum |
|---|---:|---:|
| MAE | 344.04 cp | 390.95 cp |
| RMSE | 3,151.65 cp | 3,373.51 cp |
| Sign agreement | 97.35% | 97.64% |

## Outputs

| Split | Records | SHA-256 |
|---|---:|---|
| Training | 100,000 | `241631a5219f0f47b4b0f18604740b1321406949d6bc33da30930693c901850c` |
| Validation | 12,572 | `f2e5f244d9b5923e67cb8834b9e4d740cf49e069bfb7e545e9cc5e830544fb87` |

The training audit saw 27,431/40,960 HalfKP inputs (66.97%). This diagnostic
dataset is not a training candidate; the coverage result reinforces the need
for the planned multi-million-position modern corpus.

## Cost interpretation

The combined diagnostic performed 25k fixed nodes per sampled board and
averaged 52.51 comparisons/second across the eight-worker host. Assuming linear
fixed-node throughput, the same configuration projects to approximately:

- 5.3 hours for five million labels at 5k nodes; or
- 21.2 hours for five million labels at 20k nodes.

These are planning estimates, not completed production timings. Archive I/O,
position distribution, machine load, and a different worker layout can change
the wall time.

The additional estimated 16 hours is justified for the first controlled 5M
baseline because 2.50% of labels changed which side was favoured and the deeper
budget produced consistent high-magnitude search differences across every
shard. The architecture remains fixed at 256 hidden units so data scale and
label quality are the principal experimental variables.

## Next action

Obtain and checksum a modern Lichess standard-rated CC0 monthly archive, run
the existing planning check, then start the five-million-training-position
generation at 20k nodes with whole-game validation. Generation was not started
from the 2013 diagnostic archive.
