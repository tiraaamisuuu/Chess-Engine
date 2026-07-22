#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DESTINATION="$ROOT_DIR/.tools/openings"
ARCHIVE="$DESTINATION/UHO_4060_v4.epd.zip"
BOOK="$DESTINATION/UHO_4060_v4.epd"
URL="https://raw.githubusercontent.com/official-stockfish/books/master/UHO_4060_v4.epd.zip"
EXPECTED_SHA256="a97424c5b98b42f8c27ff450f0681ad11696148548c975752350e98417ead11d"

if [[ -f "$BOOK" ]]; then
  echo "$BOOK"
  exit 0
fi

mkdir -p "$DESTINATION"
curl --fail --location --silent --show-error "$URL" --output "$ARCHIVE"

if command -v shasum >/dev/null 2>&1; then
  ACTUAL_SHA256="$(shasum -a 256 "$ARCHIVE" | awk '{print $1}')"
else
  ACTUAL_SHA256="$(sha256sum "$ARCHIVE" | awk '{print $1}')"
fi
if [[ "$ACTUAL_SHA256" != "$EXPECTED_SHA256" ]]; then
  echo "Opening archive checksum mismatch." >&2
  exit 1
fi

unzip -oq "$ARCHIVE" -d "$DESTINATION"
echo "$BOOK"
