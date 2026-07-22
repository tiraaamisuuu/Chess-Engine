#!/usr/bin/env bash
set -euo pipefail

CANDIDATE_BIN="${1:-./build/tiramisu-uci}"
BASELINE_BIN="${2:-./build/tiramisu-uci}"

GAMES="${GAMES:-200}"
CONCURRENCY="${CONCURRENCY:-2}"
TC="${TC:-10+0.1}"
HASH_MB="${HASH_MB:-256}"
THREADS="${THREADS:-1}"
OUT_DIR="${OUT_DIR:-artifacts/elo}"
OPENINGS_FILE="${OPENINGS_FILE:-tests/openings.epd}"
OPENINGS_ORDER="${OPENINGS_ORDER:-random}"

if ! command -v cutechess-cli >/dev/null 2>&1; then
  echo "Error: cutechess-cli not found in PATH." >&2
  echo "Install Cute Chess CLI and retry." >&2
  exit 1
fi

if [[ ! -x "$CANDIDATE_BIN" ]]; then
  echo "Error: candidate binary not executable: $CANDIDATE_BIN" >&2
  exit 1
fi

if [[ ! -x "$BASELINE_BIN" ]]; then
  echo "Error: baseline binary not executable: $BASELINE_BIN" >&2
  exit 1
fi

if [[ ! -f "$OPENINGS_FILE" ]]; then
  echo "Error: opening suite not found: $OPENINGS_FILE" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"
STAMP="$(date +%Y%m%d-%H%M%S)"
PGN="$OUT_DIR/match-$STAMP.pgn"
LOG="$OUT_DIR/match-$STAMP.log"

CMD=(
  cutechess-cli
  -engine "name=Candidate" "cmd=$CANDIDATE_BIN" "arg=--uci"
  -engine "name=Baseline" "cmd=$BASELINE_BIN" "arg=--uci"
  -each "proto=uci" "tc=$TC" "option.Hash=$HASH_MB" "option.Threads=$THREADS"
  -openings "file=$OPENINGS_FILE" "format=epd" "order=$OPENINGS_ORDER"
  -games "$GAMES"
  -repeat
  -recover
  -concurrency "$CONCURRENCY"
  -resign "movecount=6" "score=700"
  -draw "movenumber=40" "movecount=8" "score=10"
  -pgnout "$PGN"
)

if [[ "${SPRT:-0}" == "1" ]]; then
  CMD+=(
    -sprt
    "elo0=${ELO0:-0}"
    "elo1=${ELO1:-5}"
    "alpha=${ALPHA:-0.05}"
    "beta=${BETA:-0.05}"
  )
fi

echo "Running Elo match..."
echo "Candidate: $CANDIDATE_BIN"
echo "Baseline : $BASELINE_BIN"
echo "Games    : $GAMES"
echo "TC       : $TC"
echo "Threads  : $THREADS"
echo "Openings : $OPENINGS_FILE ($OPENINGS_ORDER, paired colors)"
echo "Log      : $LOG"
echo "PGN      : $PGN"

"${CMD[@]}" | tee "$LOG"

echo
echo "Elo run completed."
echo "Log: $LOG"
echo "PGN: $PGN"
