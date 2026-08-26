#!/usr/bin/env python3
"""Deterministic tests for the live calibration dashboard aggregation."""

from __future__ import annotations

from datetime import datetime, timezone
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "calibration_dashboard", ROOT / "scripts" / "calibration_dashboard.py"
)
assert SPEC and SPEC.loader
dashboard = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(dashboard)


class CalibrationDashboardTests(unittest.TestCase):
    def test_parses_live_score_and_elo(self) -> None:
        result = dashboard.parse_live_result(
            "Score of Forklift vs Stockfish 18 @ 2600: 17 - 11 - 12 [0.575] 40\n"
            "Elo difference: +52.5 +/- 91.0, LOS: 87.2 %, DrawRatio: 30.0 %\n"
        )
        self.assertEqual((result["wins"], result["draws"], result["losses"]), (17, 12, 11))
        self.assertEqual(result["games"], 40)
        self.assertEqual(result["score"], 0.575)
        self.assertEqual(result["relativeElo"], 52.5)

    def test_interpolates_completed_rating_bracket(self) -> None:
        estimate = dashboard.local_pool_estimate(
            [
                {"elo": 2400, "state": "complete", "scorePercent": 60.0},
                {"elo": 2600, "state": "complete", "scorePercent": 40.0},
            ]
        )
        self.assertEqual(estimate["status"], "bracketed")
        self.assertEqual(estimate["estimate"], 2500.0)

    def test_builds_snapshot_from_complete_and_live_rungs(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            run_dir = Path(temporary)
            manifest = {
                "configuration": {
                    "rungs": [2400, 2600],
                    "gamesPerRung": 4,
                    "timeControl": "10+0.1",
                    "threads": 1,
                    "hashMb": 256,
                    "concurrency": 2,
                    "candidate": {"commit": "abc123"},
                }
            }
            (run_dir / "ladder-manifest.json").write_text(json.dumps(manifest))

            complete = run_dir / "rung-2400" / "attempt-001" / "match"
            complete.mkdir(parents=True)
            (complete / "manifest.json").write_text(
                json.dumps({"createdAt": datetime.now(timezone.utc).isoformat()})
            )
            (complete / "result.json").write_text(
                json.dumps(
                    {
                        "completed": True,
                        "score": {
                            "candidateWins": 3,
                            "baselineWins": 0,
                            "draws": 1,
                            "candidateScore": 0.875,
                            "games": 4,
                        },
                        "elo": {"difference": 338.0, "uncertainty": 100.0},
                    }
                )
            )

            live_attempt = run_dir / "rung-2600" / "attempt-001"
            live_match = live_attempt / "match"
            live_match.mkdir(parents=True)
            (live_match / "manifest.json").write_text(
                json.dumps({"createdAt": datetime.now(timezone.utc).isoformat()})
            )
            (live_attempt / "driver.log").write_text(
                "Score of Forklift vs Stockfish: 1 - 0 - 1 [0.750] 2\n"
            )

            snapshot = dashboard.build_snapshot(run_dir)
            self.assertEqual(snapshot["completedGames"], 6)
            self.assertEqual(snapshot["totalGames"], 8)
            self.assertEqual(snapshot["currentRung"], 2600)
            self.assertEqual(snapshot["rungs"][1]["scorePercent"], 75.0)


if __name__ == "__main__":
    unittest.main()
