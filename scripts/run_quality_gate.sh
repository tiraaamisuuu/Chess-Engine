#!/usr/bin/env bash
set -euo pipefail

TOOLS_BIN="${1:-./build/chess-engine-tools}"
UCI_BIN="${UCI_BIN:-./build/chess-engine-uci}"

echo "== Regression =="
"$(dirname "$0")/run_regression.sh" "$TOOLS_BIN"

echo
echo "== UCI Smoke =="
"$(dirname "$0")/run_uci_smoke.sh" "$UCI_BIN"

if [[ "${RUN_ELO:-0}" == "1" ]]; then
  CANDIDATE_BIN="${CANDIDATE_BIN:-$UCI_BIN}"
  BASELINE_BIN="${BASELINE_BIN:-$UCI_BIN}"
  echo
  echo "== Elo Match =="
  "$(dirname "$0")/run_elo_match.sh" "$CANDIDATE_BIN" "$BASELINE_BIN"
fi

echo
echo "Quality gate: PASS"
