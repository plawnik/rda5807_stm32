#!/usr/bin/env python3
"""Convert an 84x48 PNG/GIF into byte-RLE C arrays for PCD8544.

Requires Pillow: python -m pip install Pillow
Example: python tools/convert_splash.py my_animation.gif > custom_splash.h
"""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Iterable

from PIL import Image, ImageOps, ImageSequence

WIDTH = 84
HEIGHT = 48


def framebuffer(image: Image.Image, invert: bool) -> bytes:
    image = ImageOps.contain(image.convert("L"), (WIDTH, HEIGHT))
    canvas = Image.new("L", (WIDTH, HEIGHT), 255)
    canvas.paste(image, ((WIDTH - image.width) // 2, (HEIGHT - image.height) // 2))
    pixels = canvas.load()
    output = bytearray(WIDTH * HEIGHT // 8)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            on = pixels[x, y] < 128
            if invert:
                on = not on
            if on:
                output[x + (y // 8) * WIDTH] |= 1 << (y & 7)
    return bytes(output)


def rle(data: bytes) -> bytes:
    output = bytearray()
    start = 0
    while start < len(data):
        end = start + 1
        while end < len(data) and data[end] == data[start] and end - start < 255:
            end += 1
        output.extend((end - start, data[start]))
        start = end
    return bytes(output)


def c_bytes(data: bytes) -> Iterable[str]:
    for offset in range(0, len(data), 12):
        yield "  " + ", ".join(f"0x{value:02X}U" for value in data[offset:offset + 12]) + ","


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("--name", default="custom_splash")
    parser.add_argument("--invert", action="store_true")
    parser.add_argument("--max-frames", type=int, default=64)
    args = parser.parse_args()

    with Image.open(args.image) as source:
        frames = list(ImageSequence.Iterator(source))[: args.max_frames]

    print("#pragma once")
    print("#include <stdint.h>")
    print()
    for index, frame in enumerate(frames):
        encoded = rle(framebuffer(frame.copy(), args.invert))
        print(f"static const uint8_t {args.name}_{index}[] = {{")
        print("\n".join(c_bytes(encoded)))
        print("};")
        print(f"#define {args.name.upper()}_{index}_SIZE {len(encoded)}U")
        print()


if __name__ == "__main__":
    main()
