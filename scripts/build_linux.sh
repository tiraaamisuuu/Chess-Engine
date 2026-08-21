#!/usr/bin/env bash
set -euo pipefail

cmake -S . -B build-linux -DCHESS_BUILD_GUI=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --parallel

echo "Built build-linux/gui and build-linux/chess-engine-uci"
