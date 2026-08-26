#!/usr/bin/env python3
"""Turn a square source mark into flat Forklift PNG and multi-size Windows ICO assets."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageOps


BACKGROUND = (9, 10, 10)
CREAM = (235, 235, 231)
ACCENT = (164, 211, 122)


def smoothstep(low: float, high: float, value: float) -> float:
    t = max(0.0, min(1.0, (value - low) / (high - low)))
    return t * t * (3.0 - 2.0 * t)


def blend(background: tuple[int, int, int], foreground: tuple[int, int, int], amount: float) -> tuple[int, int, int]:
    return tuple(round(a + (b - a) * amount) for a, b in zip(background, foreground))


def flatten_mark(source: Image.Image, size: int = 512) -> Image.Image:
    fitted = ImageOps.fit(source.convert("RGB"), (size, size), Image.Resampling.LANCZOS)
    pixels: list[tuple[int, int, int]] = []
    source_pixels = fitted.load()
    for y in range(size):
        for x in range(size):
            red, green, blue = source_pixels[x, y]
            is_accent = green > 70 and green > red * 1.08 and green > blue * 1.12
            if is_accent:
                coverage = smoothstep(70.0, 180.0, float(green))
                pixels.append(blend(BACKGROUND, ACCENT, coverage))
            else:
                luminance = 0.2126 * red + 0.7152 * green + 0.0722 * blue
                coverage = smoothstep(105.0, 205.0, luminance)
                pixels.append(blend(BACKGROUND, CREAM, coverage))

    flattened = Image.new("RGB", fitted.size, BACKGROUND)
    flattened.putdata(pixels)
    return flattened


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("png", type=Path)
    parser.add_argument("ico", type=Path)
    args = parser.parse_args()

    args.png.parent.mkdir(parents=True, exist_ok=True)
    args.ico.parent.mkdir(parents=True, exist_ok=True)

    with Image.open(args.source) as source:
        icon = flatten_mark(source)

    icon.save(args.png, optimize=True)
    icon.save(
        args.ico,
        format="ICO",
        sizes=[(16, 16), (20, 20), (24, 24), (32, 32), (40, 40), (48, 48), (64, 64), (128, 128), (256, 256)],
    )


if __name__ == "__main__":
    main()
