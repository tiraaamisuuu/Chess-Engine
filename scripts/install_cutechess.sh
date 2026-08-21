#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${CUTECHESS_VERSION:-v1.5.1}"
SOURCE_DIR="$ROOT_DIR/.tools/src/cutechess"
BUILD_DIR="$ROOT_DIR/.tools/build/cutechess"
JOBS="${BUILD_JOBS:-8}"

if [[ "${OSTYPE:-}" == darwin* ]]; then
  if ! command -v brew >/dev/null 2>&1; then
    echo "Homebrew is required to install Qt on macOS." >&2
    exit 1
  fi
  if ! brew list --versions qt >/dev/null 2>&1; then
    echo "Installing the Qt dependency..."
    brew install qt
  fi
  QT_PREFIX="${QT_PREFIX:-$(brew --prefix qt)}"
elif ! pkg-config --exists Qt6Core 2>/dev/null; then
  echo "Qt 6.8 or newer is required to build Cute Chess." >&2
  echo "Install the Qt 6 development packages, then rerun this script." >&2
  exit 1
else
  QT_PREFIX="${QT_PREFIX:-$(pkg-config --variable=prefix Qt6Core)}"
fi

mkdir -p "$(dirname "$SOURCE_DIR")" "$BUILD_DIR"
if [[ ! -d "$SOURCE_DIR/.git" ]]; then
  git clone --depth 1 --branch "$VERSION" https://github.com/cutechess/cutechess.git "$SOURCE_DIR"
fi

cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_PREFIX"
cmake --build "$BUILD_DIR" --target cli --parallel "$JOBS"

echo "Cute Chess CLI is ready: $BUILD_DIR/cutechess-cli"
