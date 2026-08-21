#!/usr/bin/env python3
"""Deterministic tests for the Stockfish ladder aggregation logic."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
SPEC = importlib.util.spec_from_file_location(
    "calibrate_rating", ROOT / "scripts" / "calibrate_rating.py"
)
assert SPEC and SPEC.loader
calibrate_rating = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(calibrate_rating)


class CalibrateRatingTests(unittest.TestCase):
    def test_parse_rungs_sorts_and_validates(self) -> None:
        self.assertEqual(calibrate_rating.parse_rungs("1900, 1320,1600"), [1320, 1600, 1900])
        with self.assertRaisesRegex(RuntimeError, "duplicates"):
            calibrate_rating.parse_rungs("1320,1320")
        with self.assertRaisesRegex(RuntimeError, "integers"):
            calibrate_rating.parse_rungs("1320,club")

    def test_interpolates_a_bracketed_local_anchor(self) -> None:
        estimate = calibrate_rating.estimate_local_rating(
            [
                {"uciElo": 1320, "candidateScore": 0.75},
                {"uciElo": 1600, "candidateScore": 0.25},
            ]
        )

        self.assertEqual(estimate["status"], "bracketed")
        self.assertEqual(estimate["estimate"], 1460.0)
        self.assertEqual(estimate["lowerAnchor"], 1320)
        self.assertEqual(estimate["upperAnchor"], 1600)

    def test_reports_out_of_range_without_inventing_a_rating(self) -> None:
        estimate = calibrate_rating.estimate_local_rating(
            [
                {"uciElo": 1320, "candidateScore": 1.0},
                {"uciElo": 1600, "candidateScore": 0.75},
            ]
        )

        self.assertEqual(estimate["status"], "above_range")
        self.assertIsNone(estimate["estimate"])
        self.assertEqual(estimate["lowerAnchor"], 1600)

    def test_resume_uses_only_completed_attempts(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            rung_dir = Path(temporary)
            incomplete = rung_dir / "attempt-001" / "match"
            incomplete.mkdir(parents=True)
            (incomplete / "result.json").write_text(
                json.dumps({"completed": False}), encoding="utf-8"
            )
            completed = rung_dir / "attempt-002" / "match"
            completed.mkdir(parents=True)
            (completed / "result.json").write_text(
                json.dumps({"completed": True, "score": {"games": 4}}),
                encoding="utf-8",
            )
            corrupt = rung_dir / "attempt-003" / "match"
            corrupt.mkdir(parents=True)
            (corrupt / "result.json").write_text("{", encoding="utf-8")

            found = calibrate_rating.completed_attempt(rung_dir)

            self.assertIsNotNone(found)
            assert found is not None
            self.assertEqual(found[0].name, "attempt-002")
            self.assertTrue(found[1]["completed"])
            self.assertEqual(calibrate_rating.next_attempt(rung_dir).name, "attempt-004")


if __name__ == "__main__":
    unittest.main()
