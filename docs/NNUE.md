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

The exporter quantizes the PyTorch model directly into the format verified by
the C++ test suite. It also writes a `.pt` checkpoint.

## Use the network

```text
setoption name EvalFile value networks/engine-v1.nnue
setoption name Use NNUE value true
```

An NNUE candidate is releasable only after loader tests, classical-vs-NNUE
paired opening matches, and an SPRT pass. Network files should be versioned by
checksum and kept out of Git if they are too large for normal source history.

The current C++ evaluator is a correct reference implementation that rebuilds
accumulators from the position. A strength release should add accumulator updates
to make/unmake before making NNUE the default; the on-disk feature layout will not
need to change.
