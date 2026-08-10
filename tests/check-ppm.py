#!/usr/bin/env python3
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = path.read_bytes()
pos = 0

def token():
    global pos
    while pos < len(data):
        if data[pos:pos+1] == b'#':
            pos = data.find(b'\n', pos)
            if pos < 0:
                raise ValueError('unterminated PPM comment')
        if pos < len(data) and data[pos] in b' \t\r\n':
            pos += 1
            continue
        break
    start = pos
    while pos < len(data) and data[pos] not in b' \t\r\n':
        pos += 1
    return data[start:pos]

if token() != b'P6':
    raise SystemExit('not a P6 PPM')
w, h, maxv = int(token()), int(token()), int(token())
while pos < len(data) and data[pos] in b' \t\r\n':
    pos += 1
pixels = data[pos:]
if maxv != 255 or len(pixels) != w*h*3:
    raise SystemExit('invalid PPM payload')
if (w, h) != (1280, 720):
    raise SystemExit(f'unexpected dimensions: {w}x{h}')

count = w*h
green = bright = visible = 0
sum_r = sum_g = sum_b = 0
left_g = right_g = center_g = 0
left_n = right_n = center_n = 0
for y in range(h):
    for x in range(w):
        q = (y*w+x)*3
        r,g,b = pixels[q:q+3]
        sum_r += r; sum_g += g; sum_b += b
        if g > r*1.20 and g > b*1.25 and g > 8:
            green += 1
        if g > 160 and r > 45 and b > 35:
            bright += 1
        if r+g+b > 15:
            visible += 1
        if x < 120:
            left_g += g; left_n += 1
        elif x >= w-120:
            right_g += g; right_n += 1
        elif w//2-120 <= x < w//2+120:
            center_g += g; center_n += 1

if visible < count * 0.035:
    raise SystemExit(f'image too dark/sparse: visible={visible/count:.3%}')
if green < visible * 0.65:
    raise SystemExit('visible image is not sufficiently green-dominant')
if sum_g <= sum_r * 2.0 or sum_g <= sum_b * 1.8:
    raise SystemExit('global color balance is not Matrix-green dominant')
if bright < 8:
    raise SystemExit(f'too few pale cursor pixels: {bright}')
edge = (left_g/left_n + right_g/right_n)/2
center = center_g/center_n
if edge < 0.25:
    raise SystemExit(f'auto-aspect edges unexpectedly empty: edge={edge:.2f}, center={center:.2f}')
print(f'PPM OK: {w}x{h}, visible={visible/count:.1%}, bright={bright}, edgeG={edge:.2f}, centerG={center:.2f}')
