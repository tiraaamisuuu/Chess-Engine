# NNUE v1 Training and Integration

The engine supports a versioned `HalfKP-v1` network as an optional evaluation
backend. Classical evaluation remains the default until a network is trained and
wins controlled matches.

## Hardware

A Ryzen 9 5900X, RTX 3070, and 32 GB RAM are sufficient for a 256-wide network.
The model is small enough for the GPU; teacher labelling is normally the limiting
step. Generate several independent shards in parallel rather than assigning all
CPU threads to one Stockfish process.

## Environment

```sh
python -m venv .venv
.venv/bin/pip install -r scripts/nnue/requirements.txt
```

On Windows, use `.venv\Scripts\python.exe` and install the CUDA-enabled PyTorch
wheel recommended for the installed NVIDIA driver.

The Windows pipeline was verified on 2026-07-27 with Python 3.13,
PyTorch `2.12.1+cu130`, CUDA runtime 13.0, and an RTX 3070. Use the current
command from the official PyTorch selector rather than assuming that exact wheel
will remain current.

## Generate labelled positions

Use legally obtained PGNs and a strong Stockfish binary:

```sh
.venv/bin/python scripts/nnue/generate_dataset.py \
  --engine /path/to/stockfish \
  --pgn games-1.pgn \
  --output data/train-1.nnuebin \
  --validation-output data/validation-1.nnuebin \
  --validation-fraction 0.1 \
  --source-name "My licensed game collection" \
  --source-license "Licence or permission description" \
  --nodes 20000 --threads 3 --hash 512 --sample-rate 0.25
```

Run multiple shards against different PGNs. Five to twenty million diverse,
quiet and tactical positions is a sensible serious-training range; smaller
datasets are useful for validating the pipeline. Keep train and validation games
separate when producing final networks. The generator assigns entire games to a
split deterministically, deduplicates sampled positions, and writes a manifest
containing source, teacher, option, checksum, seed, and distribution metadata.

For a real multi-core run, use the resumable coordinator instead of launching
workers by hand:

```sh
.venv/bin/python scripts/nnue/generate_shards.py \
  --engine /path/to/stockfish \
  --pgn /path/to/licensed-games-*.pgn \
  --output-dir data/teacher-v1 \
  --shards 8 --jobs 4 --threads 3 --hash 512 \
  --source-name "My licensed game collection" \
  --source-license "Licence or permission description" \
  --nodes 20000 --sample-rate 0.25 --validation-fraction 0.1
```

Each worker receives a deterministic whole-game partition and stateless
position sampling. Re-running the command validates the generator commit,
teacher/data/configuration identity, manifests, and output checksums, then skips
complete shards. The final merge deduplicates across workers and removes any
position duplicated between training and validation. It retains every part and
its manifest for auditability. `--jobs 0` derives a concurrency level from the
CPU and per-teacher thread count; explicit jobs make resource use predictable.

The default `.nnuebin` format is a versioned 42-byte record containing the packed
board, teacher centipawns from the side-to-move viewpoint, game result, side to
move, source-game id, and ply. JSONL remains available for inspection and small
experiments. Mate values are bounded to keep targets finite.

## Train and export

```sh
.venv/bin/python scripts/nnue/train.py \
  --data data/train-*.nnuebin \
  --validation-data data/validation-*.nnuebin \
  --output networks/engine-v1.nnue \
  --hidden 256 --batch-size 2048 --epochs 8 --workers 4
```

The model is optimized in normalized units (`--target-scale 600` means one
model unit is 600 cp) and converted back to centipawns during validation and
export. This avoids making an Adam step of `0.001` effectively microscopic on
raw centipawn targets. The v2 checkpoint contract records this scale and the
Huber beta. Integer export defaults to a 1024 hidden scale and rejects weight
saturation instead of silently clipping it.

The exporter quantizes the PyTorch model directly into the format verified by
the C++ test suite. It writes:

- the deployable `.nnue` network and model-only `.pt` weights
- last, best-validation, and periodic versioned training checkpoints
- atomic JSON/CSV epoch metrics
- a checksummed provenance manifest with CUDA, data, and quantization details

Add `--cpp-tools build-pc/Release/chess-engine-tools.exe` to make exact
Python-versus-C++ inference agreement a required export gate. To resume after an
interruption, repeat the original training configuration and add:

```sh
--resume networks/engine-v1.checkpoint.pt
```

The checkpoint restores model, optimizer, scheduler, metrics, and Python,
NumPy, PyTorch, and CUDA random-number states. Cosine-scheduler resumes require
the original `--epochs` value so the learning-rate trajectory cannot silently
change. `--early-stopping-patience`, `--scheduler`, `--minimum-learning-rate`,
and `--verify-samples` provide the remaining training controls.

For direct inspection of either evaluator:

```sh
build-pc/Release/chess-engine-tools --eval --fen "FEN"
build-pc/Release/chess-engine-tools --eval --fen "FEN" --nnue networks/engine-v1.nnue
```

The benchmark accepts the same `--nnue` option.

## Use the network

```text
setoption name EvalFile value networks/engine-v1.nnue
setoption name Use NNUE value true
```

An NNUE candidate is releasable only after loader tests, classical-vs-NNUE
paired opening matches, and an SPRT pass. Network files should be versioned by
checksum and kept out of Git if they are too large for normal source history.

The search keeps a per-ply accumulator stack, applies piece deltas after moves,
and rebuilds only the moved king's perspective when its king square changes.
The full-rebuild path remains available for correctness and performance
comparisons through `--nnue-rebuild`. Random legal playout/unmake tests and
explicit castling, en-passant, promotion, and king-capture fixtures require the
incremental values and search tree to match the reference path exactly. The
on-disk feature layout did not change.
