# NNUE Training and Promotion

The engine supports a versioned `HalfKP-v1` network as an optional evaluation
backend. Classical evaluation remains the default until a trained network wins
controlled matches. The intended loop is:

```text
licensed games -> sampled positions -> Stockfish labels -> CUDA training
-> quantized export -> C++ verification -> paired match -> promote or reject
```

No source change is required to train or test another 256-wide network.

## Hardware and environment

A Ryzen 9 5900X, RTX 3070 8 GB, and 32 GB RAM are sufficient for this model.
Teacher labelling is CPU-bound; training fits comfortably on the GPU. Several
small Stockfish processes normally produce labels faster than one process using
every hardware thread.

Create an isolated environment from the repository root:

```powershell
py -3 -m venv .venv-nnue
.\.venv-nnue\Scripts\python.exe -m pip install -r scripts\nnue\requirements.txt
```

Install the CUDA-enabled PyTorch wheel selected for the installed NVIDIA driver
if the requirements install did not provide one. Verify CUDA before a long run:

```powershell
.\.venv-nnue\Scripts\python.exe -c "import torch; print(torch.__version__, torch.cuda.is_available(), torch.cuda.get_device_name(0))"
```

The Windows pipeline has been exercised with Python 3.13, CUDA PyTorch, and an
RTX 3070. The exact PyTorch wheel is deliberately not pinned because supported
driver/runtime combinations change.

## Data contract and preflight checks

The compact format stores a packed board, Stockfish centipawn label from the
side-to-move viewpoint, game result from the same viewpoint, side to move,
source-game id, and ply. The test suites protect the feature encoding, score and
result perspective, deterministic whole-game split, compact binary format,
quantization, and Python/C++ prediction agreement:

```powershell
.\.venv-nnue\Scripts\python.exe tests\nnue_dataset_tests.py
.\.venv-nnue\Scripts\python.exe tests\nnue_training_tests.py
ctest --test-dir build-pc -C Release --output-on-failure
```

Run these before committing CPU-days to labelling.

## Generate a five-million-position baseline

The official [Lichess open database](https://database.lichess.org/) publishes
standard-game archives under CC0 with SHA-256 checksum files. Record and verify
the published archive checksum before generation. A modern monthly archive is a
large sampling reservoir; the generator stops after the requested target and
does not try to label every game or position.

This is the recommended Ryzen 9 5900X production command. Adjust only the three
paths if the July 2026 archive and Stockfish are stored elsewhere:

```powershell
$Corpus = "D:\ChessData\lichess_db_standard_rated_2026-07.pgn.zst"
$Stockfish = "$PWD\.tools\user-engines\external-stockfish-18\stockfish-windows-x86-64.exe"
$Dataset = "D:\ChessData\halfkp-5m-sf18-20k"

.\.venv-nnue\Scripts\python.exe scripts\nnue\generate_shards.py `
  --engine $Stockfish `
  --pgn $Corpus `
  --output-dir $Dataset `
  --shards 16 --jobs 6 --threads 2 --hash 256 `
  --source-name "Lichess standard rated 2026-07" `
  --source-license "CC0" `
  --source-url "https://database.lichess.org/standard/lichess_db_standard_rated_2026-07.pgn.zst" `
  --nodes 20000 --sample-rate 0.04 `
  --min-ply 8 --max-ply 180 `
  --validation-fraction 0.1 --seed 1 `
  --target-training-positions 5000000 --target-oversample 1.15
```

Six two-thread teachers use at most 12 engine threads and 1.5 GiB of Stockfish
hash, leaving capacity for decompression and the operating system. The low
sample rate spreads labels across many games. Sixteen deterministic shards make
interruption recovery and later provenance inspection manageable.

The coordinator prints positions, label rate, elapsed time, and ETA. Repeating
the exact command resumes: complete shard manifests and output checksums are
validated before a worker is skipped. It globally deduplicates training data,
then validation data against training, and deterministically caps `train.nnuebin`
at exactly five million records when sufficient source positions were sampled.
Increase `--target-oversample` if the final warning reports a shortfall.

Important outputs are:

```text
halfkp-5m-sf18-20k/
  train.nnuebin
  validation.nnuebin
  dataset.manifest.json
  parts/                         resumable source shards and manifests
```

Keep the manifest with the binaries. It records source and teacher checksums,
sampling/split parameters, generator identity, merge counts, and output hashes.

## Compare teacher budgets

Do not double the cost of the full five-million run merely to measure label
stability. Use a representative 100,000-position diagnostic first:

```powershell
.\.venv-nnue\Scripts\python.exe scripts\nnue\generate_shards.py `
  --engine $Stockfish --pgn $Corpus `
  --output-dir "D:\ChessData\teacher-quality-100k" `
  --shards 8 --jobs 6 --threads 2 --hash 128 `
  --source-name "Lichess standard rated 2026-07 teacher comparison" `
  --source-license "CC0" `
  --source-url "https://database.lichess.org/standard/lichess_db_standard_rated_2026-07.pgn.zst" `
  --nodes 5000 --comparison-nodes 20000 `
  --sample-rate 0.02 --validation-fraction 0.1 --seed 1 `
  --target-training-positions 100000 --target-oversample 1.15
```

The same sampled boards receive both labels. The dataset manifest aggregates
mean difference, MAE, RMSE, maximum absolute difference, and score-sign
agreement. Use this evidence to decide whether 20k labels justify their cost.

## Audit data coverage

Run the streaming audit before training:

```powershell
.\.venv-nnue\Scripts\python.exe scripts\nnue\dataset_diagnostics.py `
  --data "$Dataset\train.nnuebin" `
  --output "$Dataset\training-diagnostics.json"
```

It checksums the input and reports HalfKP feature coverage/frequency, king-square
coverage, phase, material imbalance, teacher magnitude/sign, game result, and
ply distributions. Sparse king squares or many features seen only a handful of
times are a reason to improve sampling/data before tuning the model.

## Combine data produced on Windows and Debian

The portable merger accepts either worker part manifests or complete dataset
manifests copied beside their `.nnuebin` files. This permits platform-specific
Stockfish executables: their hashes are all preserved, while teacher node
budgets, optional comparison budgets, UCI identities when present, and the
game-split seed/fraction must agree.

The simplest safe workflow is to give each machine different monthly archives,
use the same `--seed` and `--validation-fraction`, complete a coordinator run on
each, then copy both output directories to the main machine. Merge them with:

```powershell
$Manifests = @(
  "D:\ChessData\windows-shards\dataset.manifest.json",
  "D:\ChessData\debian-shards\dataset.manifest.json"
)

.\.venv-nnue\Scripts\python.exe scripts\nnue\merge_datasets.py `
  --manifest $Manifests `
  --output-dir "D:\ChessData\halfkp-combined" `
  --target-training-positions 5000000
```

For worker-level transfers, copy each `part-NNNN.manifest.json` with its
`part-NNNN.train.nnuebin` and `part-NNNN.validation.nnuebin`, then pass all part
manifest paths to the same command. Recorded absolute Windows/Linux paths may
differ: the merger locates files beside the copied manifest, verifies their
SHA-256 values, globally deduplicates, prevents train/validation position
leakage, and emits a new recursively mergeable provenance manifest.

Do not bypass a rejected contract. Different teacher budgets or split seeds
should become separate experiments rather than one silently mixed dataset.

## CUDA training and export

Keep the existing 256-wide architecture for the larger-data baseline so its
result measures data scale and quality independently. This command trains with
game-disjoint validation, checkpointing, early stopping, quantitative metrics,
quantization validation, and mandatory C++ export verification:

```powershell
$Dataset = "D:\ChessData\halfkp-5m-sf18-20k"
$Network = "D:\ChessNetworks\halfkp-256-5m-sf18-20k.nnue"

.\.venv-nnue\Scripts\python.exe scripts\nnue\train.py `
  --data "$Dataset\train.nnuebin" `
  --validation-data "$Dataset\validation.nnuebin" `
  --output $Network `
  --hidden 256 --batch-size 2048 --epochs 32 --workers 4 `
  --device cuda --learning-rate 0.001 --minimum-learning-rate 0.00001 `
  --scheduler cosine --early-stopping-patience 5 `
  --result-weight 0.15 --target-scale 600 --huber-beta-cp 100 `
  --hidden-scale 1024 --output-scale 64 --verify-samples 512 `
  --cpp-tools .\build-pc\Release\chess-engine-tools.exe
```

The trainer reports validation RMSE, MAE, score-sign accuracy, throughput, and
GPU peak memory each epoch. The final manifest also slices bias, MAE, RMSE, and
sign accuracy by side-to-move king square, phase, material imbalance, and
teacher-evaluation magnitude. It writes last/best/periodic v2 checkpoints, JSON
and CSV metrics, model weights, the deployable quantized network, and a
checksummed training manifest. Export fails instead of clipping overflowing
integer weights or accepting a Python/C++ prediction mismatch.

Resume an interrupted run with the original options plus:

```powershell
--resume "D:\ChessNetworks\halfkp-256-5m-sf18-20k.checkpoint.pt"
```

The original epoch count must remain unchanged when resuming the cosine
scheduler.

## Test and promote the resulting network

Inspect a known position through both evaluators and benchmark the network:

```powershell
.\build-pc\Release\chess-engine-tools.exe --eval --fen "r1bq1rk1/pp2bppp/2n1pn2/2pp4/2P5/2NP1NP1/PP2PPBP/R1BQ1RK1 b - - 0 9"
.\build-pc\Release\chess-engine-tools.exe --eval --fen "r1bq1rk1/pp2bppp/2n1pn2/2pp4/2P5/2NP1NP1/PP2PPBP/R1BQ1RK1 b - - 0 9" --nnue $Network
.\build-pc\Release\chess-engine-tools.exe --bench --bench-depth 8 --bench-tt 256 --threads 1 --nnue $Network
```

Then compare the candidate against classical evaluation using the identical
engine revision on both sides:

```powershell
py -3 scripts\compare_engines.py `
  --baseline HEAD --candidate HEAD `
  --candidate-eval-file $Network `
  --games 400 --tc 10+0.1 --threads 1 --hash 256 --concurrency 4
```

If the diagnostic is competitive, run the larger SPRT described in
`docs/BENCHMARKING.md`. Promote only a network that passes correctness/export
gates and statistically beats the current champion. Until then, leave classical
evaluation as the default and keep rejected experiments documented.

UCI activation is independent of source code:

```text
setoption name EvalFile value D:\ChessNetworks\halfkp-256-5m-sf18-20k.nnue
setoption name Use NNUE value true
```

## Later experiments

After the five-million 256-wide baseline, change one variable at a time. The
trainer already exposes hidden width, target/result blending, loss scale,
learning rate/scheduler, quantization scales, and reproducible seeds. Plausible
next experiments include 20M positions, wider hidden layers, improved HalfKA
features, phase-aware sampling, self-play data, and automatic network-vs-champion
promotion. Every format or feature change must be versioned and retain exact
Python/C++ agreement tests.
