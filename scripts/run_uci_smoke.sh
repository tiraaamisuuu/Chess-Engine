#!/usr/bin/env bash
set -euo pipefail

ENGINE_BIN="${1:-./build/tiramisu-uci}"

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
  sleep 0.6
  printf 'position startpos\n'
  printf 'go movetime 200 searchmoves a2a3\n'
  sleep 0.4
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

if ! rg -q '^bestmove a2a3$' "$OUT_FILE"; then
  echo "[FAIL] UCI searchmoves restriction was not respected" >&2
  cat "$OUT_FILE" >&2
  exit 1
fi

if ! rg -q '^option name Clear Hash type button$' "$OUT_FILE"; then
  echo "[FAIL] Clear Hash option was not advertised" >&2
  cat "$OUT_FILE" >&2
  exit 1
fi

echo "UCI smoke: PASS"
