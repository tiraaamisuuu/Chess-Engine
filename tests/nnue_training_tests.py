#!/usr/bin/env python3

from __future__ import annotations

import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

import chess
import numpy as np
import torch


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts" / "nnue"))

from nnue_dataset import BinaryShardWriter  # noqa: E402
from train import (  # noqa: E402
    FEATURE_COUNT,
    FORMAT_VERSION,
    MAGIC,
    HalfKpV1,
    PositionsDataset,
    active_features,
    collate,
    evaluate,
    export_network,
    find_dataset_manifest,
    quantize_network,
    quantized_predict,
    restore_rng_state,
    truncating_division,
    validation_error_diagnostics,
    verify_quantization,
)


class NnueTrainingTests(unittest.TestCase):
    def test_compact_and_jsonl_datasets_match(self) -> None:
        board = chess.Board()
        board.push_uci("e2e4")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            compact = root / "fixture.nnuebin"
            jsonl = root / "fixture.jsonl"
            with BinaryShardWriter(compact) as writer:
                writer.write(board, 125, -1.0, game_id=7, ply=9)
            jsonl.write_text(json.dumps({
                "fen": board.fen(en_passant="fen"),
                "score_cp": 125,
                "result": -1.0,
                "game_id": 7,
                "ply": 9,
            }) + "\n", encoding="utf-8")

            with PositionsDataset([compact], result_weight=0.2) as compact_dataset:
                compact_sample = compact_dataset[0]
            with PositionsDataset([jsonl], result_weight=0.2) as jsonl_dataset:
                jsonl_sample = jsonl_dataset[0]
            self.assertEqual(compact_sample, jsonl_sample)

    def test_quantized_prediction_tracks_float_model(self) -> None:
        torch.manual_seed(17)
        model = HalfKpV1(hidden=8)
        board = chess.Board()
        with tempfile.TemporaryDirectory() as directory:
            compact = Path(directory) / "fixture.nnuebin"
            with BinaryShardWriter(compact) as writer:
                for game_id, move in enumerate(("e2e4", "d2d4", "g1f3"), start=1):
                    position = board.copy()
                    position.push_uci(move)
                    writer.write(position, game_id * 10, 0.0, game_id, 1)
            with PositionsDataset([compact], result_weight=0.0) as dataset:
                report = verify_quantization(model, dataset, 3, 17, 127, 64)
                self.assertEqual(report["samples"], 3)
                self.assertLess(report["maxAbsErrorCp"], 1.0)

                first, second, _target = dataset[0]
                quantized = quantize_network(model, 127, 64)
                prediction = quantized_predict(quantized, first, second)
                self.assertIsInstance(prediction, int)

    def test_scaled_model_exports_centipawn_outputs(self) -> None:
        torch.manual_seed(29)
        model = HalfKpV1(hidden=8)
        board = chess.Board()
        first = active_features(board, board.turn)
        second = active_features(board, not board.turn)
        quantized = quantize_network(model, 1024, 64, target_scale=600.0)
        batch = collate([(first, second, 0.0)])
        with torch.no_grad():
            expected_cp = float(model(*batch[:4]).item()) * 600.0
        actual_cp = quantized_predict(quantized, first, second)
        self.assertLess(abs(actual_cp - expected_cp), 2.0)

    def test_validation_reports_rmse_mae_and_sign_accuracy(self) -> None:
        model = HalfKpV1(hidden=4)
        with torch.no_grad():
            for parameter in model.parameters():
                parameter.zero_()
        board = chess.Board()
        first = active_features(board, board.turn)
        second = active_features(board, not board.turn)
        loader = [collate([
            (first, second, -100.0),
            (first, second, 100.0),
        ])]
        report = evaluate(model, loader, torch.device("cpu"), target_scale=1.0)
        self.assertEqual(report["samples"], 2)
        self.assertAlmostEqual(report["rmseCp"], 100.0)
        self.assertAlmostEqual(report["maeCp"], 100.0)
        self.assertAlmostEqual(report["signAccuracy"], 0.5)

    def test_validation_error_slices_cover_position_groups(self) -> None:
        model = HalfKpV1(hidden=4)
        with torch.no_grad():
            for parameter in model.parameters():
                parameter.zero_()
        with tempfile.TemporaryDirectory() as directory:
            compact = Path(directory) / "validation.nnuebin"
            with BinaryShardWriter(compact) as writer:
                writer.write(chess.Board(), 50, 0.0, game_id=1, ply=8)
                writer.write(
                    chess.Board("8/8/8/8/8/4k3/4p3/4K3 w - - 0 1"),
                    -250, -1.0, game_id=2, ply=60,
                )
            with PositionsDataset([compact], result_weight=0.0) as dataset:
                report = validation_error_diagnostics(
                    model, dataset, torch.device("cpu"), target_scale=1.0,
                    batch_size=2,
                )
            self.assertEqual(report["overall"]["samples"], 2)
            self.assertEqual(set(report["phase"]), {"endgame", "opening"})
            self.assertEqual(set(report["teacherEvalMagnitude"]), {"0-99", "100-299"})
            self.assertEqual(set(report["sideToMoveKingSquare"]), {"e1"})
            self.assertAlmostEqual(report["overall"]["maeCp"], 150.0)

    def test_quantization_rejects_saturation(self) -> None:
        model = HalfKpV1(hidden=2)
        with torch.no_grad():
            model.feature_weights.weight[0, 0] = 40.0
        with self.assertRaisesRegex(ValueError, "input weights exceed int16"):
            quantize_network(model, hidden_scale=1024, output_scale=64)

    def test_export_header_and_size(self) -> None:
        model = HalfKpV1(hidden=4)
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "fixture.nnue"
            export_network(model, destination)
            with destination.open("rb") as source:
                magic, version, features, hidden, hidden_scale, output_scale, _bias = struct.unpack(
                    "<8sIIIiii", source.read(struct.calcsize("<8sIIIiii")),
                )
            self.assertEqual(magic, MAGIC)
            self.assertEqual(version, FORMAT_VERSION)
            self.assertEqual(features, FEATURE_COUNT)
            self.assertEqual(hidden, 4)
            self.assertEqual(hidden_scale, 1024)
            self.assertEqual(output_scale, 64)
            expected = struct.calcsize("<8sIIIiii") + 4 * 4 + FEATURE_COUNT * 4 * 2 + 8 * 2
            self.assertEqual(destination.stat().st_size, expected)

    def test_integer_division_matches_cpp_truncation(self) -> None:
        self.assertEqual(truncating_division(7, 3), 2)
        self.assertEqual(truncating_division(-7, 3), -2)

    def test_rng_state_restores_all_cpu_generators(self) -> None:
        import random

        random.seed(31)
        np.random.seed(31)
        torch.manual_seed(31)
        state = {
            "python": random.getstate(),
            "numpy": np.random.get_state(),
            "torch": torch.get_rng_state(),
            "cuda": None,
        }
        expected = (random.random(), float(np.random.random()), float(torch.rand(1).item()))
        random.random()
        np.random.random()
        torch.rand(1)
        restore_rng_state(state)
        actual = (random.random(), float(np.random.random()), float(torch.rand(1).item()))
        self.assertEqual(actual, expected)

    def test_dataset_manifest_discovery_validates_output_checksum(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dataset = root / "train.nnuebin"
            dataset.write_bytes(b"fixture")
            import hashlib
            checksum = hashlib.sha256(b"fixture").hexdigest()
            manifest = root / "dataset.manifest.json"
            manifest.write_text(json.dumps({
                "outputs": [{"path": str(dataset.resolve()), "sha256": checksum}],
            }), encoding="utf-8")
            self.assertEqual(find_dataset_manifest(dataset, checksum), manifest.resolve())
            self.assertIsNone(find_dataset_manifest(dataset, "0" * 64))


if __name__ == "__main__":
    unittest.main()
