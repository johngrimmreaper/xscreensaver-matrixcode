#!/usr/bin/env python3
from pathlib import Path
import sys


def read_token(handle):
    token = bytearray()
    while True:
        ch = handle.read(1)
        if not ch:
            raise ValueError("unexpected end of PPM header")
        if ch == b"#":
            handle.readline()
            continue
        if not ch.isspace():
            token.extend(ch)
            break
    while True:
        ch = handle.read(1)
        if not ch or ch.isspace():
            return bytes(token)
        token.extend(ch)


def main(path):
    with Path(path).open("rb") as handle:
        magic = read_token(handle)
        width = int(read_token(handle))
        height = int(read_token(handle))
        maximum = int(read_token(handle))
        data = handle.read()
    if magic != b"P6" or maximum != 255:
        raise SystemExit("invalid PPM format")
    if len(data) != width * height * 3:
        raise SystemExit("incorrect PPM payload size")

    pixels = memoryview(data)
    non_black = 0
    green_dominant = 0
    bright_heads = 0
    for index in range(0, len(pixels), 3):
        red, green, blue = pixels[index:index + 3]
        if red + green + blue > 18:
            non_black += 1
            if green > red * 1.25 and green > blue * 1.15:
                green_dominant += 1
            if red > 90 and green > 150 and blue > 70:
                bright_heads += 1

    total = width * height
    if non_black < total * 0.003:
        raise SystemExit(f"render is too dark: {non_black}/{total} lit pixels")
    if non_black > total * 0.45:
        raise SystemExit(f"render is too dense: {non_black}/{total} lit pixels")
    if green_dominant < non_black * 0.55:
        raise SystemExit("render is not predominantly green")
    if bright_heads < 8:
        raise SystemExit("render lacks pale leading glyphs")
    print(
        f"PPM validation passed: {width}x{height}, "
        f"{non_black} lit pixels, {bright_heads} bright head pixels"
    )


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} FILE.ppm")
    main(sys.argv[1])
