#!/usr/bin/env python3

from __future__ import annotations

import random
import sys
import tempfile
import unittest
from pathlib import Path

import chess


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts" / "nnue"))

from nnue_dataset import (  # noqa: E402
    BinaryShardWriter,
    SHARD_HEADER,
    SHARD_RECORD,
    active_features,
    active_features_from_packed,
    decode_record,
    encode_record,
    iter_compact_records,
    pack_board,
    read_shard_header,
)
from generate_dataset import (  # noqa: E402
    game_is_validation,
    game_shard,
    position_is_sampled,
    result_for_side_to_move,
)
from generate_shards import merge_shards, record_key  # noqa: E402


class NnueDatasetTests(unittest.TestCase):
    def test_packed_features_match_board_features(self) -> None:
        board = chess.Board()
        random_generator = random.Random(20260727)
        for _ in range(80):
            packed = pack_board(board)
            for perspective in (chess.WHITE, chess.BLACK):
                self.assertEqual(
                    active_features(board, perspective),
                    active_features_from_packed(packed, perspective),
                )
            if board.is_game_over():
                board.reset()
            board.push(random_generator.choice(list(board.legal_moves)))

    def test_record_round_trip(self) -> None:
        board = chess.Board("r3k2r/ppp2ppp/2n5/3qp3/8/2N2N2/PPP2PPP/R2Q1RK1 b kq - 7 14")
        payload = encode_record(board, -432, -1.0, game_id=1234, ply=27)
        self.assertEqual(len(payload), SHARD_RECORD.size)
        record = decode_record(payload)
        self.assertEqual(record.score_cp, -432)
        self.assertEqual(record.result, -1)
        self.assertEqual(record.turn, chess.BLACK)
        self.assertEqual(record.game_id, 1234)
        self.assertEqual(record.ply, 27)
        self.assertEqual(
            active_features(board, chess.WHITE),
            active_features_from_packed(record.board_bytes, chess.WHITE),
        )

    def test_writer_finalizes_count_and_records(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "fixture.nnuebin"
            boards = [chess.Board(), chess.Board()]
            boards[1].push_uci("e2e4")
            with BinaryShardWriter(destination) as writer:
                writer.write(boards[0], 12, 0.0, game_id=1, ply=8)
                writer.write(boards[1], -34, 1.0, game_id=2, ply=9)

            with destination.open("rb") as source:
                header = read_shard_header(source)
                self.assertEqual(header.count, 2)
                self.assertEqual(destination.stat().st_size,
                                 SHARD_HEADER.size + 2 * SHARD_RECORD.size)

            records = list(iter_compact_records([destination]))
            self.assertEqual([record.score_cp for record in records], [12, -34])
            self.assertEqual([record.game_id for record in records], [1, 2])

    def test_game_split_is_deterministic_and_whole_game(self) -> None:
        assignments = [game_is_validation("ab" * 32, game, 17, 0.2)
                       for game in range(1, 1001)]
        repeated = [game_is_validation("ab" * 32, game, 17, 0.2)
                    for game in range(1, 1001)]
        self.assertEqual(assignments, repeated)
        self.assertGreater(sum(assignments), 150)
        self.assertLess(sum(assignments), 250)
        self.assertNotEqual(
            assignments,
            [game_is_validation("ab" * 32, game, 18, 0.2)
             for game in range(1, 1001)],
        )

    def test_result_is_oriented_to_side_to_move(self) -> None:
        self.assertEqual(result_for_side_to_move("1-0", chess.WHITE), 1.0)
        self.assertEqual(result_for_side_to_move("1-0", chess.BLACK), -1.0)
        self.assertEqual(result_for_side_to_move("1/2-1/2", chess.BLACK), 0.0)

    def test_sampling_and_game_shards_are_stateless(self) -> None:
        checksum = "cd" * 32
        selected = [position_is_sampled(checksum, 17, ply, 9, 0.25)
                    for ply in range(1, 1001)]
        self.assertEqual(
            selected,
            [position_is_sampled(checksum, 17, ply, 9, 0.25)
             for ply in range(1, 1001)],
        )
        self.assertGreater(sum(selected), 200)
        self.assertLess(sum(selected), 300)
        assignments = [game_shard(checksum, game, 9, 4) for game in range(1, 1001)]
        self.assertTrue(all(0 <= shard < 4 for shard in assignments))
        for shard in range(4):
            self.assertGreater(assignments.count(shard), 200)
            self.assertLess(assignments.count(shard), 300)

    def test_global_merge_deduplicates_parts_and_validation_leakage(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            start = chess.Board()
            e4 = chess.Board()
            e4.push_uci("e2e4")
            d4 = chess.Board()
            d4.push_uci("d2d4")
            c4 = chess.Board()
            c4.push_uci("c2c4")
            first = root / "first.nnuebin"
            second = root / "second.nnuebin"
            validation = root / "validation-part.nnuebin"
            with BinaryShardWriter(first) as writer:
                writer.write(start, 12, 0.0, 1, 8)
                writer.write(e4, 20, 0.0, 1, 9)
            with BinaryShardWriter(second) as writer:
                writer.write(start, 99, 0.0, 2, 8)
                writer.write(d4, 30, 0.0, 2, 9)
            with BinaryShardWriter(validation) as writer:
                writer.write(e4, 25, 0.0, 3, 9)
                writer.write(c4, 40, 0.0, 3, 9)

            seen: set[int] = set()
            training_stats = merge_shards(
                [first, second], root / "train.nnuebin", seen,
            )
            validation_stats = merge_shards(
                [validation], root / "validation.nnuebin", seen,
            )
            self.assertEqual(training_stats, {"read": 4, "written": 3, "duplicatesSkipped": 1})
            self.assertEqual(validation_stats, {"read": 2, "written": 1, "duplicatesSkipped": 1})
            self.assertEqual(len(seen), 4)
            start_record = next(iter(iter_compact_records([first])))
            self.assertNotEqual(
                record_key(start_record.board_bytes, start_record.turn),
                record_key(start_record.board_bytes, not start_record.turn),
            )


if __name__ == "__main__":
    unittest.main()
