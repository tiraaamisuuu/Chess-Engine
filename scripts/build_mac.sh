#!/usr/bin/env bash
set -euo pipefail

SFML_PREFIX="${SFML_PREFIX:-$HOME/.local/sfml-2.6.2}"

clang++ -O3 -DNDEBUG -flto -std=c++17 main.cpp -o gui \
  -I"$SFML_PREFIX/include" \
  -L"$SFML_PREFIX/lib" \
  -lsfml-graphics -lsfml-window -lsfml-system \
  -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo \
  -pthread \
  -Wl,-rpath,"$SFML_PREFIX/lib" \
  -Wl,-sectcreate,__TEXT,__info_plist,macos/Info.plist

echo "Built ./gui"
