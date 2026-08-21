#!/usr/bin/env bash
set -euo pipefail

SFML_PREFIX="${SFML_PREFIX:-$HOME/.local/sfml-2.6.2}"

cmake -S . -B build-macos \
  -DCHESS_BUILD_GUI=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$SFML_PREFIX"
cmake --build build-macos --parallel

echo "Built build-macos/gui and build-macos/chess-engine-uci"
