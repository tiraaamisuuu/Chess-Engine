#!/usr/bin/env bash
set -euo pipefail

g++ -std=c++17 src/main.cpp src/ui.cpp -o gui $(pkg-config --cflags --libs sfml-graphics sfml-window sfml-system)

echo "Built ./gui"
