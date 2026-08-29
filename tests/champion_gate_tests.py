#!/usr/bin/env python3
"""Deterministic tests for candidate-versus-champion gate decisions."""

from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "champion_gate", ROOT / "scripts" / "champion_gate.py"
)
assert SPEC and SPEC.loader
champion_gate = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(champion_gate)


class ChampionGateTests(unittest.TestCase):
    def match(self, decision: str, *, completed: bool = True) -> dict[str, object]:
        return {
            "completed": completed,
            "failures": {
                "timeForfeits": 0,
                "crashes": 0,
                "illegalMoves": 0,
                "disconnects": 0,
            },
            "sprt": {"enabled": True, "decision": decision},
        }

    def test_promotes_only_when_sprt_accepts_h1(self) -> None:
        decision, _ = champion_gate.classify_gate_result(
            self.match("accepted_h1")
        )
        self.assertEqual(decision, "promote")

    def test_rejects_when_sprt_accepts_h0(self) -> None:
        decision, _ = champion_gate.classify_gate_result(
            self.match("accepted_h0")
        )
        self.assertEqual(decision, "reject")

    def test_preserves_inconclusive_result(self) -> None:
        decision, _ = champion_gate.classify_gate_result(
            self.match("inconclusive")
        )
        self.assertEqual(decision, "inconclusive")

    def test_technical_failure_blocks_promotion(self) -> None:
        match = self.match("accepted_h1")
        match["failures"]["crashes"] = 1
        decision, _ = champion_gate.classify_gate_result(match)
        self.assertEqual(decision, "technical_failure")

    def test_quick_run_is_only_a_smoke_pass(self) -> None:
        decision, _ = champion_gate.classify_gate_result(
            self.match("disabled"), quick=True
        )
        self.assertEqual(decision, "smoke_pass")

    def test_interrupted_match_is_not_strength_evidence(self) -> None:
        decision, _ = champion_gate.classify_gate_result(
            self.match("inconclusive", completed=False)
        )
        self.assertEqual(decision, "interrupted")

    def test_builds_default_contract_from_versioned_registry_keys(self) -> None:
        args = argparse.Namespace(
            games=None,
            time_control=None,
            threads=None,
            hash_mb=None,
            concurrency=None,
            seed=None,
            elo0=None,
            elo1=None,
            quick=False,
        )
        champion = {
            "gateDefaults": {
                "games": 10000,
                "timeControl": "10+0.1",
                "threads": 1,
                "hashMb": 256,
                "concurrency": 6,
                "seed": 4701,
                "elo0": 0.0,
                "elo1": 5.0,
                "alpha": 0.05,
                "beta": 0.05,
            }
        }

        contract = champion_gate.contract_from_args(args, champion)

        self.assertEqual(contract["games"], 10000)
        self.assertEqual(contract["timeControl"], "10+0.1")
        self.assertEqual(contract["hashMb"], 256)
        self.assertTrue(contract["sprt"])


if __name__ == "__main__":
    unittest.main()
