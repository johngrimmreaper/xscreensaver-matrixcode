# Film-fidelity research for MatrixCode 0.2

This document records the reasoning behind the 0.2 visual rework.  The goal is
not "green text falling down a screen".  The goal is to reproduce the visual
*grammar* of the original Matrix-era code closely enough that it reads like a
photographed prop display rather than a modern screensaver.

## 1. Separate the visual targets

There is no single canonical Matrix rain shot.  Two useful targets are:

1. **operator-screen code** — the flatter code imagery seen on workstation CRTs
   in the first film;
2. **opening/title code** — a more cinematic full-frame composition.

The default `operator1999` profile targets the first category because that is the
look that benefits most from reproducing a period display chain.  `opening1999`
keeps a separate cleaner presentation.

## 2. Typography is geometry, not merely a font choice

Simon Whiteley has described the source design as hand-made Japanese-inspired
letterforms selected to feel traditional and old-fashioned rather than as
ordinary computer text.  The important practical consequences are:

- do not render normal Unicode from a desktop font;
- keep a curated, irregular glyph vocabulary;
- use many reflected / deliberately malformed forms;
- make symbols tall and narrow;
- preserve obvious space between neighboring columns.

The initial MatrixCode atlas already satisfied the clean-room artwork constraint,
but its final screen quads were too broad.  The rework keeps the 8x12 source
bitmaps yet displays their cores at approximately **1.35:1 height-to-width**.

## 3. The grid should be stable

High-fidelity recreations agree on a counter-intuitive behavior: the characters
belong to a fixed grid.  The visible "fall" is primarily an illumination wave
moving through stationary cells.

That is different from scrolling strings of text.  It creates the impression
that the code is being electrically awakened and extinguished in place.
MatrixCode already used a fixed grid; 0.2 keeps it and changes the illumination
model around it.

## 4. First-film/operator geometry

The best-documented public reconstruction provides a dedicated `operator`
configuration intended to resemble first-film opening/operator imagery.  Its
useful geometry clues include:

- 108 logical columns;
- 1.35 glyph height-to-width ratio;
- relatively long rain cycles;
- fast, bright isolated cursor cells;
- slow symbol cycling.

MatrixCode therefore uses **108 columns** as the operator default.  The row
count is derived from the framed display area so the logical cell grid remains
square in screen space; on 4:3 that produces approximately 81 rows.

This is a much better invariant than "18 screen pixels per glyph": a modern
4K panel and a 1080p panel now show the same composition instead of different
amounts of code.

## 5. Flatter rain, brighter cursors

The old MatrixCode trail was a conventional nonlinear fade: bright head,
progressively dimmer tail.  That looks pleasant but reads like a modern Matrix
clone.

The operator reconstruction instead uses a brightness override for ordinary lit
cells and isolates the cursor.  The practical result is a flatter, more
constant-brightness body with a conspicuously luminous leading cell.

0.2 mirrors that *behavior* independently:

- per-column phase and speed remain irregular;
- a warped sawtooth-like wave decides which cells are lit;
- cells above the visibility threshold are rendered at a nearly constant green;
- the wrap discontinuity becomes the pale green-white cursor;
- symbol cycling happens independently from the falling illumination.

## 6. Why a CRT layer matters

The workstation imagery is not just source graphics.  It is source graphics
seen through a period display chain.  A modern panel removes several cues that
our visual system associates with that image:

- finite raster resolution;
- scan structure;
- phosphor-mask/grille texture;
- optical bloom/halation around bright green strokes;
- small phosphor persistence;
- tube curvature and rounded edges;
- edge falloff / vignette;
- 4:3 framing.

The `operator1999` profile therefore first snaps geometry to a **640x480 virtual
raster**, then applies a deliberately restrained fixed-function CRT treatment.
The goal is not an exaggerated emulator shader.  The effect should mostly be
felt at normal viewing distance and become obvious only when toggled with
`-no-crt`.

The CRT pass remains shaderless so the screensaver continues to run on old GLX
and fixed-function OpenGL stacks.  It consists of:

- nearest-filtered core glyph atlas;
- linearly filtered glow atlas;
- broad and tight additive glow passes;
- one low-opacity persistence echo;
- mild barrel warp;
- raster-scale scanline overlay;
- raster-scale vertical phosphor grille;
- radial edge darkening;
- small overscan inset.

## 7. 4:3 inside a widescreen desktop is intentional

Stretching the composition to fill a 16:9 or ultrawide desktop changes both the
number of visible columns and the spatial rhythm.  For the operator profile,
MatrixCode therefore creates a centered 4:3 aperture with black side areas.

That may initially look surprising on a modern monitor, but it preserves the
prop-display composition.  Users who prefer full-panel presentation can use:

```sh
./matrixcode -aspect auto
```

## 8. Clean-room compromise

A literal pixel-for-pixel reproduction would ultimately require the exact film
assets and exact photographed/display pipeline.  MatrixCode intentionally does
not include extracted film glyphs or the official-derived assets used by some
other projects.

The 0.2 goal is therefore: **match geometry, rhythm, light behavior, display
texture and composition while keeping the glyph artwork independent**.

This is also why visual tuning should happen in this order:

1. framing;
2. columns / glyph proportions;
3. rain rhythm and occupancy;
4. cursor/trail brightness relationship;
5. glow;
6. CRT texture;
7. only then color tweaks.

## Public references consulted

- WIRED, *The Matrix Code Came From Sushi Recipes—but Which?* (2019):
  https://www.wired.com/story/the-matrix-code-sushi-recipe
- Rezmason/matrix, project notes and `operator` configuration:
  https://github.com/Rezmason/matrix
- Rezmason rain state shader (fixed grid / illumination wave model):
  https://github.com/Rezmason/matrix/blob/master/shaders/glsl/rainPass.raindrop.frag.glsl
- Rezmason symbol-state shader (independent glyph cycling):
  https://github.com/Rezmason/matrix/blob/master/shaders/glsl/rainPass.symbol.frag.glsl
- CRT-Royale family, used only as architectural background for common CRT
  components such as scanlines, masks and bloom; MatrixCode does not copy its
  shader code.
