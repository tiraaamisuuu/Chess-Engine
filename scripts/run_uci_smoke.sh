#!/usr/bin/env bash
set -euo pipefail

ENGINE_BIN="${1:-./build/gui}"

if [[ ! -x "$ENGINE_BIN" ]]; then
  echo "Error: engine binary not found or not executable: $ENGINE_BIN" >&2
  echo "Build first, e.g. cmake -S . -B build && cmake --build build -j8" >&2
  exit 1
fi

OUT_FILE="$(mktemp)"
trap 'rm -f "$OUT_FILE"' EXIT

{
  printf 'uci\n'
  printf 'isready\n'
  printf 'setoption name Hash value 128\n'
  printf 'ucinewgame\n'
  printf 'position startpos moves e2e4 e7e5 g1f3 b8c6\n'
  printf 'go movetime 300\n'
  sleep 1
  printf 'quit\n'
} | "$ENGINE_BIN" --uci > "$OUT_FILE"

if ! rg -q '^uciok$' "$OUT_FILE"; then
  echo "[FAIL] Missing uciok" >&2
  cat "$OUT_FILE" >&2
  exit 1
fi

if ! rg -q '^readyok$' "$OUT_FILE"; then
  echo "[FAIL] Missing readyok" >&2
  cat "$OUT_FILE" >&2
  exit 1
fi

if ! rg -q '^bestmove [a-h][1-8][a-h][1-8][qrbn]?$' "$OUT_FILE"; then
  echo "[FAIL] Missing or invalid bestmove" >&2
  cat "$OUT_FILE" >&2
  exit 1
fi

echo "UCI smoke: PASS"
