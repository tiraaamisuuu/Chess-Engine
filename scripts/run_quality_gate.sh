#!/usr/bin/env bash
set -euo pipefail

ENGINE_BIN="${1:-./build/gui}"

echo "== Regression =="
"$(dirname "$0")/run_regression.sh" "$ENGINE_BIN"

echo
echo "== UCI Smoke =="
"$(dirname "$0")/run_uci_smoke.sh" "$ENGINE_BIN"

if [[ "${RUN_ELO:-0}" == "1" ]]; then
  BASELINE_BIN="${BASELINE_BIN:-$ENGINE_BIN}"
  echo
  echo "== Elo Match =="
  "$(dirname "$0")/run_elo_match.sh" "$ENGINE_BIN" "$BASELINE_BIN"
fi

echo
echo "Quality gate: PASS"
