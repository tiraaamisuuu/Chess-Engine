#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-web}"
VENV_DIR="${WEB_VENV:-$ROOT_DIR/.venv-web}"
if [[ -n "${ENGINE_BIN:-}" ]]; then
  ENGINE_BIN="$ENGINE_BIN"
elif [[ -x "$ROOT_DIR/bin/chess-engine-uci" ]]; then
  ENGINE_BIN="$ROOT_DIR/bin/chess-engine-uci"
else
  ENGINE_BIN="$BUILD_DIR/chess-engine-uci"
fi

if [[ ! -x "$VENV_DIR/bin/python" && ! -x "$VENV_DIR/Scripts/python.exe" ]]; then
  python3 -m venv "$VENV_DIR"
fi
if [[ -x "$VENV_DIR/bin/python" ]]; then
  PYTHON="$VENV_DIR/bin/python"
else
  PYTHON="$VENV_DIR/Scripts/python.exe"
fi

if ! "$PYTHON" -c 'import chess' >/dev/null 2>&1; then
  "$PYTHON" -m pip install --disable-pip-version-check -r "$ROOT_DIR/web/requirements.txt"
fi

if [[ ! -x "$ENGINE_BIN" ]]; then
  if [[ ! -f "$ROOT_DIR/CMakeLists.txt" ]]; then
    echo "The packaged UCI engine is missing: $ENGINE_BIN" >&2
    exit 1
  fi
  cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCHESS_BUILD_GUI=OFF \
    -DBUILD_TESTING=OFF \
    -DCMAKE_BUILD_TYPE=Release
  cmake --build "$BUILD_DIR" --config Release --parallel "${BUILD_JOBS:-8}"
  if [[ -x "$BUILD_DIR/Release/chess-engine-uci.exe" ]]; then
    ENGINE_BIN="$BUILD_DIR/Release/chess-engine-uci.exe"
  fi
fi

exec "$PYTHON" "$ROOT_DIR/web/server.py" --engine "$ENGINE_BIN" "$@"
