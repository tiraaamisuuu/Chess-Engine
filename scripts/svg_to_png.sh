#!/usr/bin/env bash
set -euo pipefail

mkdir -p assets/pieces_png

for f in assets/pieces/*.svg; do
  base="$(basename "$f" .svg)"
  # Match tile size so sprites render at 1:1 pixels in the GUI.
  rsvg-convert -w 96 -h 96 "$f" -o "assets/pieces_png/${base}.png"
done

echo "Done: assets/pieces_png generated."
