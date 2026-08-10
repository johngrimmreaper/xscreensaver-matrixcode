# Design

## Visual model

MatrixCode separates glyph state from light state.  Cells occupy a stable grid;
the apparent downward motion comes from independently timed illumination waves.
The 0.2 operator profile uses a thresholded, nearly flat luminous body and a
bright cursor at the wave discontinuity instead of a conventional smooth
head-to-tail gradient.

## Grid and typography

The operator profile uses an 80x60 reference density at 4:3, corresponding to
8x8 logical cells on a 640x480 reference CRT.  Target height sets the logical
cell size.  Wider windows add columns at that same size instead of stretching
the glyphs or leaving unused side areas; a 16:9 target is therefore about
107x60.  Increasing resolution at the same aspect keeps the logical grid count
and scales the glyphs proportionally.  The glyph quad itself is narrower than the cell,
approximately 1.35:1 height-to-width, leaving visible column separation.

The atlas contains 57 original 8x12 base glyphs:

- 33 katakana-inspired abstract forms;
- 10 digits;
- 14 punctuation / technical symbols.

Horizontal mirrors provide 114 addressable glyphs.  Selection remains weighted
toward the kana-inspired set.

## Rain rhythm

Each column receives a deterministic pseudo-random phase, speed multiplier and
small brightness multiplier.  A smoothly perturbed sawtooth-like phase creates
non-mechanical changes in trail length and spacing without allocating objects
per drop.

## Color

Ordinary code is saturated green.  Cursor cells add substantial red and some
blue so they approach a pale mint-white rather than simply becoming a brighter
version of the same green.

## CRT mode

`operator1999` uses a lightweight fixed-function CRT approximation:

- 640x480 virtual raster snapping;
- nearest core sampling and linear glow sampling;
- two additive glow scales;
- mild phosphor persistence;
- barrel curvature;
- scanline and vertical-mask overlays;
- vignette and overscan;
- 4:3 framing.

It deliberately avoids shader/FBO requirements to preserve compatibility with
old X11/GLX systems.
