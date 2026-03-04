#!/usr/bin/env bash
set -euo pipefail

GUI_BIN="${1:-./build/gui}"
PERFT_DEPTH="${PERFT_DEPTH:-4}"
BENCH_DEPTH="${BENCH_DEPTH:-6}"
BENCH_TIME_MS="${BENCH_TIME_MS:-1500}"
BENCH_TT_MB="${BENCH_TT_MB:-128}"

if [[ ! -x "$GUI_BIN" ]]; then
  echo "Error: GUI binary not found or not executable: $GUI_BIN" >&2
  echo "Build first, e.g. cmake -S . -B build && cmake --build build -j8" >&2
  exit 1
fi

echo "== Perft Regression =="
"$GUI_BIN" --perft-tests --max-depth "$PERFT_DEPTH"

echo
echo "== Search Benchmark =="
"$GUI_BIN" --bench \
  --bench-depth "$BENCH_DEPTH" \
  --bench-time "$BENCH_TIME_MS" \
  --bench-tt "$BENCH_TT_MB"

echo
echo "Regression run completed successfully."
