# NNUE Training

The NNUE toolchain covers source ingestion, teacher labelling, training,
quantization, C++ verification, and engine-vs-engine promotion testing. The
completed production baseline used a 256-unit HalfKP network; it was rejected
after its promotion match, while the verified pipeline was retained.

## Data contract

Training records contain a compact board representation plus the teacher centipawn score, game result, side to move, source game identifier, and ply. Dataset manifests record the source, licence, generator settings, engine identity, sampling policy, split policy, and checksums needed to reproduce a run.

## Source ingestion

The generator can stream compressed Lichess PGN archives (`.zst`) without first expanding them to disk. Games are sampled deterministically, positions are deduplicated, and train/validation assignment is performed by game so positions from one game cannot leak across the split.

For a large run, first use the inspection and planning commands to estimate eligible games, output size, and wall-clock cost. Distributed jobs can label independent shards on different machines and the merger validates manifests before combining them.

## Teacher labelling

Stockfish is used as the teacher at a fixed node budget. Fixed nodes are preferred to fixed time because they are easier to reproduce across machines. The generator can label the same sampled position at two node budgets, for example 5,000 and 20,000 nodes, and report:

- centipawn mean absolute error;
- root-mean-square error;
- sign disagreement;
- maximum absolute difference; and
- the additional labelling cost.

Stockfish's hash is cleared before each budget so neither label can reuse the
other search's work. That comparison is the evidence used to choose a
production budget rather than assuming that a deeper label is automatically
worth four times the work.

The completed 2026-08-20 comparison used 127,784 identical boards. The 5k and
20k scores had 366.40 cp MAE and 97.499% sign agreement. Because 3,196 labels
changed which side was favoured and the projected 5M/20k run is about 21 hours
on the measured host, 20k was selected for the completed production-scale
baseline.

## Training

The PyTorch trainer supports CUDA, deterministic seeds, resumable checkpoints, and validation metrics split by useful position categories. Training targets combine the teacher score with the game outcome according to the recorded configuration.

The dataset is kept separate from generated checkpoints and exported networks. Large generated artifacts are excluded from Git; small manifests and results are retained as evidence.

## Export and verification

After training, weights are quantized into the engine’s versioned network format. The verifier loads identical positions in Python and C++, compares their scores, and checks error thresholds before a candidate is eligible for match testing.

## Playing-strength gate

Candidate networks are tested with paired openings and colour reversal. Match summaries, PGNs, engine logs, and reproducibility metadata are written to an artifact directory. Only a statistically credible improvement is promoted to the runtime default.

For command examples and the exact current status, see the repository’s [NNUE documentation](https://github.com/tiraaamisuuu/Forklift/blob/main/docs/NNUE.md).
