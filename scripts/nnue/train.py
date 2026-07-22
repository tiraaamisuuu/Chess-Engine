#!/usr/bin/env python3
"""Train and export the TiramisuChess HalfKP-v1 NNUE network."""

from __future__ import annotations

import argparse
from array import array
import json
import math
import random
import struct
from pathlib import Path
from typing import Iterator

import chess
import numpy as np
import torch
from torch import nn
from torch.utils.data import DataLoader, Dataset, Subset


FEATURE_COUNT = 64 * 10 * 64
FORMAT_VERSION = 1
MAGIC = b"TNNUE1\0\0"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", required=True, type=Path, nargs="+")
    parser.add_argument("--validation-data", type=Path, nargs="+",
                        help="Optional game-disjoint validation JSONL shard(s)")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--hidden", type=int, default=256)
    parser.add_argument("--epochs", type=int, default=8)
    parser.add_argument("--batch-size", type=int, default=2048)
    parser.add_argument("--learning-rate", type=float, default=1e-3)
    parser.add_argument("--validation-fraction", type=float, default=0.02)
    parser.add_argument("--result-weight", type=float, default=0.15)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    parser.add_argument("--checkpoint", type=Path)
    return parser.parse_args()


class JsonlPositions(Dataset):
    def __init__(self, paths: list[Path], result_weight: float):
        self.paths = paths
        self.file_indices = array("I")
        self.offsets = array("Q")
        self._handles: dict[Path, object] = {}
        self.result_weight = result_weight
        for file_index, path in enumerate(paths):
            with path.open("rb") as source:
                while True:
                    offset = source.tell()
                    line = source.readline()
                    if not line:
                        break
                    if line.strip():
                        self.file_indices.append(file_index)
                        self.offsets.append(offset)

    def __len__(self) -> int:
        return len(self.offsets)

    def __getstate__(self) -> dict:
        state = self.__dict__.copy()
        state["_handles"] = {}
        return state

    def __getitem__(self, index: int) -> tuple[list[int], list[int], float]:
        path = self.paths[self.file_indices[index]]
        offset = self.offsets[index]
        handle = self._handles.get(path)
        if handle is None:
            handle = path.open("rb")
            self._handles[path] = handle
        handle.seek(offset)
        record = json.loads(handle.readline())
        board = chess.Board(record["fen"])
        first = active_features(board, board.turn)
        second = active_features(board, not board.turn)
        score = max(-3000.0, min(3000.0, float(record["score_cp"])))
        result_target = float(record.get("result", 0.0)) * 1000.0
        target = score * (1.0 - self.result_weight) + result_target * self.result_weight
        return first, second, target


def orient_square(square: chess.Square, perspective: chess.Color) -> int:
    if perspective == chess.WHITE:
        return square
    return chess.square(chess.square_file(square), 7 - chess.square_rank(square))


def active_features(board: chess.Board, perspective: chess.Color) -> list[int]:
    king_square = board.king(perspective)
    if king_square is None:
        raise ValueError("training position has no king")
    oriented_king = orient_square(king_square, perspective)
    features: list[int] = []
    for square, piece in board.piece_map().items():
        if piece.piece_type == chess.KING:
            continue
        piece_bucket = piece.piece_type - 1
        relative_color = 0 if piece.color == perspective else 1
        bucket = relative_color * 5 + piece_bucket
        oriented_piece = orient_square(square, perspective)
        features.append(oriented_king * 640 + bucket * 64 + oriented_piece)
    return features


def collate(samples: list[tuple[list[int], list[int], float]]) -> tuple[torch.Tensor, ...]:
    first_flat: list[int] = []
    second_flat: list[int] = []
    first_offsets: list[int] = []
    second_offsets: list[int] = []
    targets: list[float] = []
    for first, second, target in samples:
        first_offsets.append(len(first_flat))
        second_offsets.append(len(second_flat))
        first_flat.extend(first)
        second_flat.extend(second)
        targets.append(target)
    return (
        torch.tensor(first_flat, dtype=torch.long),
        torch.tensor(first_offsets, dtype=torch.long),
        torch.tensor(second_flat, dtype=torch.long),
        torch.tensor(second_offsets, dtype=torch.long),
        torch.tensor(targets, dtype=torch.float32),
    )


class HalfKpV1(nn.Module):
    def __init__(self, hidden: int):
        super().__init__()
        self.hidden = hidden
        self.feature_weights = nn.EmbeddingBag(FEATURE_COUNT, hidden, mode="sum")
        self.hidden_bias = nn.Parameter(torch.zeros(hidden))
        self.output = nn.Linear(hidden * 2, 1)
        nn.init.normal_(self.feature_weights.weight, mean=0.0, std=0.005)
        nn.init.normal_(self.output.weight, mean=0.0, std=0.05)

    def forward(self, first: torch.Tensor, first_offsets: torch.Tensor,
                second: torch.Tensor, second_offsets: torch.Tensor) -> torch.Tensor:
        first_activation = torch.clamp(self.feature_weights(first, first_offsets) + self.hidden_bias, 0.0, 1.0)
        second_activation = torch.clamp(self.feature_weights(second, second_offsets) + self.hidden_bias, 0.0, 1.0)
        return self.output(torch.cat((first_activation, second_activation), dim=1)).squeeze(1)


def batches(loader: DataLoader, device: torch.device) -> Iterator[tuple[torch.Tensor, ...]]:
    for batch in loader:
        yield tuple(value.to(device, non_blocking=True) for value in batch)


def export_network(model: HalfKpV1, destination: Path, hidden_scale: int = 127,
                   output_scale: int = 64) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    state = model.cpu().eval()
    input_weights = np.rint(state.feature_weights.weight.detach().numpy() * hidden_scale)
    input_weights = np.clip(input_weights, -32768, 32767).astype("<i2")
    hidden_bias = np.rint(state.hidden_bias.detach().numpy() * hidden_scale).astype("<i4")
    output_weights = np.rint(state.output.weight.detach().numpy().reshape(-1) * output_scale)
    output_weights = np.clip(output_weights, -32768, 32767).astype("<i2")
    output_bias = int(round(float(state.output.bias.detach()[0]) * hidden_scale * output_scale))

    with destination.open("wb") as output:
        output.write(struct.pack("<8sIIIiii", MAGIC, FORMAT_VERSION, FEATURE_COUNT, state.hidden,
                                 hidden_scale, output_scale, output_bias))
        output.write(hidden_bias.tobytes(order="C"))
        output.write(input_weights.tobytes(order="C"))
        output.write(output_weights.tobytes(order="C"))


def evaluate(model: HalfKpV1, loader: DataLoader, device: torch.device) -> float:
    model.eval()
    total_loss = 0.0
    samples = 0
    with torch.no_grad():
        for first, first_offsets, second, second_offsets, target in batches(loader, device):
            prediction = model(first, first_offsets, second, second_offsets)
            total_loss += torch.sum((prediction - target) ** 2).item()
            samples += target.numel()
    return math.sqrt(total_loss / max(1, samples))


def main() -> int:
    args = parse_args()
    if not 1 <= args.hidden <= 1024:
        raise SystemExit("--hidden must be between 1 and 1024 (the C++ loader limit)")
    if args.epochs < 1 or args.batch_size < 1 or args.workers < 0:
        raise SystemExit("--epochs and --batch-size must be positive; --workers cannot be negative")
    if not 0.0 <= args.result_weight <= 1.0:
        raise SystemExit("--result-weight must be in [0, 1]")
    if not args.validation_data and not 0.0 < args.validation_fraction < 1.0:
        raise SystemExit("--validation-fraction must be in (0, 1)")
    random.seed(args.seed)
    torch.manual_seed(args.seed)
    device = torch.device(args.device)

    dataset = JsonlPositions(args.data, args.result_weight)
    if len(dataset) < 100:
        raise SystemExit("dataset must contain at least 100 positions")
    if args.validation_data:
        training = dataset
        validation = JsonlPositions(args.validation_data, args.result_weight)
        if len(validation) == 0:
            raise SystemExit("validation dataset is empty")
    else:
        indices = list(range(len(dataset)))
        random.shuffle(indices)
        validation_size = max(1, int(len(indices) * args.validation_fraction))
        validation = Subset(dataset, indices[:validation_size])
        training = Subset(dataset, indices[validation_size:])

    loader_options = dict(batch_size=args.batch_size, num_workers=args.workers,
                          collate_fn=collate, pin_memory=device.type == "cuda")
    training_loader = DataLoader(training, shuffle=True, **loader_options)
    validation_loader = DataLoader(validation, shuffle=False, **loader_options)

    model = HalfKpV1(args.hidden).to(device)
    if args.checkpoint:
        model.load_state_dict(torch.load(args.checkpoint, map_location=device))
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.learning_rate, weight_decay=1e-6)
    loss_function = nn.SmoothL1Loss(beta=100.0)

    for epoch in range(1, args.epochs + 1):
        model.train()
        for first, first_offsets, second, second_offsets, target in batches(training_loader, device):
            optimizer.zero_grad(set_to_none=True)
            prediction = model(first, first_offsets, second, second_offsets)
            loss = loss_function(prediction, target)
            loss.backward()
            optimizer.step()
        rmse = evaluate(model, validation_loader, device)
        print(f"epoch={epoch} validation_rmse_cp={rmse:.2f}", flush=True)

    export_network(model, args.output)
    torch.save(model.state_dict(), args.output.with_suffix(".pt"))
    print(f"exported={args.output} hidden={args.hidden} train_positions={len(training)} "
          f"validation_positions={len(validation)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
