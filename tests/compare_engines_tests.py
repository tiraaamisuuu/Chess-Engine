#!/usr/bin/env python3
"""Deterministic tests for match-runner metadata and result parsing."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "compare_engines", ROOT / "scripts" / "compare_engines.py"
)
assert SPEC and SPEC.loader
compare_engines = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(compare_engines)


class CompareEnginesTests(unittest.TestCase):
    def test_parses_uci_option_bounds_and_values(self) -> None:
        output = "\n".join(
            [
                "id name Stockfish 18",
                "option name Threads type spin default 1 min 1 max 1024",
                "option name UCI_LimitStrength type check default false",
                "option name UCI_Elo type spin default 1320 min 1320 max 3190",
                "option name Style type combo default Normal var Solid var Normal var Risky",
                "uciok",
            ]
        )

        options = compare_engines.parse_uci_options(output)

        self.assertEqual(options[0]["name"], "Threads")
        self.assertEqual(options[0]["min"], 1)
        self.assertEqual(options[0]["max"], 1024)
        self.assertEqual(options[2]["default"], "1320")
        self.assertEqual(options[2]["min"], 1320)
        self.assertEqual(options[2]["max"], 3190)
        self.assertEqual(options[3]["vars"], ["Solid", "Normal", "Risky"])

    def test_configures_supported_options_and_allows_overrides(self) -> None:
        inspection = {
            "options": [
                {"name": "Hash", "type": "spin"},
                {"name": "Threads", "type": "spin"},
                {"name": "UCI_LimitStrength", "type": "check"},
                {"name": "UCI_Elo", "type": "spin", "min": 1320, "max": 3190},
            ]
        }

        options = compare_engines.configured_options(
            inspection,
            "Baseline",
            threads=2,
            hash_mb=128,
            custom_values=["Hash=64", "UCI_LimitStrength=true", "UCI_Elo=1800"],
            eval_file=None,
        )

        self.assertEqual(
            options,
            [
                ("Hash", "64"),
                ("Threads", "2"),
                ("UCI_LimitStrength", "true"),
                ("UCI_Elo", "1800"),
            ],
        )

    def test_rejects_option_outside_discovered_bounds(self) -> None:
        inspection = {
            "options": [
                {"name": "Hash", "type": "spin"},
                {"name": "Threads", "type": "spin"},
                {"name": "UCI_Elo", "type": "spin", "min": 1320, "max": 3190},
            ]
        }

        with self.assertRaisesRegex(RuntimeError, "below its minimum 1320"):
            compare_engines.configured_options(
                inspection,
                "Baseline",
                threads=1,
                hash_mb=16,
                custom_values=["UCI_Elo=1200"],
                eval_file=None,
            )

    def test_rejects_unadvertised_custom_option(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "does not advertise"):
            compare_engines.configured_options(
                {
                    "options": [
                        {"name": "Hash", "type": "spin"},
                        {"name": "Threads", "type": "spin"},
                    ]
                },
                "Candidate",
                threads=1,
                hash_mb=16,
                custom_values=["Unknown=1"],
                eval_file=None,
            )

    def test_engine_definition_preserves_external_arguments(self) -> None:
        definition = compare_engines.engine_definition(
            "Calibrated Stockfish",
            Path("stockfish.exe"),
            ["--some-argument"],
            [("UCI_LimitStrength", "true"), ("UCI_Elo", "1800")],
        )

        self.assertIn("arg=--some-argument", definition)
        self.assertIn("option.UCI_LimitStrength=true", definition)
        self.assertIn("option.UCI_Elo=1800", definition)
        self.assertNotIn("arg=--uci", definition)

    def test_parses_machine_readable_result_and_failures(self) -> None:
        log = """\
Finished game 1 (Candidate vs Baseline): 1-0 {White mates}
Score of Candidate vs Baseline: 1 - 0 - 0  [1.000] 1
Finished game 2 (Baseline vs Candidate): 1-0 {Black loses on time}
Score of Candidate vs Baseline: 1 - 1 - 0  [0.500] 2
Finished game 3 (Candidate vs Baseline): 0-1 {White makes an illegal move}
Score of Candidate vs Baseline: 1 - 2 - 0  [0.333] 3
Finished game 4 (Baseline vs Candidate): 1/2-1/2 {Draw by adjudication}
Score of Candidate vs Baseline: 1 - 2 - 1  [0.375] 4
Elo difference: -88.7 +/- 100.1, LOS: 4.1 %, DrawRatio: 25.0 %
"""

        result = compare_engines.parse_match_result(
            log, "Candidate", "Baseline", expected_games=4, exit_code=0
        )

        self.assertTrue(result["completed"])
        self.assertEqual(result["score"]["candidateWins"], 1)
        self.assertEqual(result["score"]["baselineWins"], 2)
        self.assertEqual(result["score"]["draws"], 1)
        self.assertEqual(result["elo"]["difference"], -88.7)
        self.assertEqual(result["failures"]["timeForfeits"], 1)
        self.assertEqual(result["failures"]["illegalMoves"], 1)

    def test_non_finite_elo_is_valid_strict_json_data(self) -> None:
        log = """\
Finished game 1 (Candidate vs Baseline): 1-0 {White mates}
Score of Candidate vs Baseline: 1 - 0 - 0  [1.000] 1
Elo difference: inf +/- nan, LOS: 97.7 %, DrawRatio: 0.0 %
"""

        result = compare_engines.parse_match_result(
            log, "Candidate", "Baseline", expected_games=1, exit_code=0
        )

        self.assertIsNone(result["elo"]["difference"])
        self.assertIsNone(result["elo"]["uncertainty"])
        self.assertEqual(result["elo"]["display"], "inf +/- nan")
        json.dumps(result, allow_nan=False)


if __name__ == "__main__":
    unittest.main()
