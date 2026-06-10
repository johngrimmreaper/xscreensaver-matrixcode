# Design and fidelity

## Visual target

The target is the readable 2D code rain from the original-film era, not a
literal 3D waterfall and not a wall filled uniformly with moving text. The
implementation therefore separates **glyph placement** from **light motion**:
characters occupy a fixed rectangular grid while pulses change their
brightness as they descend.

That distinction matters. Moving every character object creates mechanically
sliding strings; moving an illumination envelope through a stable grid creates
the impression that symbols are being awakened, replaced, and left to decay.

## Glyph language

The atlas contains 57 original 8x12 base glyphs:

- 33 narrow katakana-inspired abstract forms;
- 10 decimal digits;
- 14 punctuation and technical symbols.

Every base form also has a horizontal mirror, for 114 addressable glyphs. The
selection is weighted toward the kana-inspired set. Mirroring is common but not
universal so that the image looks designed rather than mechanically reflected.
The source patterns are deliberately stored as readable bitmaps in `glyphs.c`.

The forms are inspirations rather than a usable Japanese typeface. Several are
hybrids or deliberately malformed, matching the visual role of encoded symbols
rather than pretending to be normal written Japanese.

## Motion model

Each column is initialized with:

- an active/inactive state;
- one or occasionally two pulses;
- a speed selected within a restrained range;
- an independent period and phase;
- a varied trail length;
- a slight brightness multiplier.

The leading cell is made pale and relatively sharp. Behind it, a nonlinear
falloff produces a bright shoulder and a long dim tail. Small deterministic
noise prevents mathematically smooth gradients. Head passage can replace a
cell's glyph; a much slower global process changes occasional background cells.

## Color and glow

The core pass uses near-black through saturated green, with increasing red and
blue only near the head. This lets the leading glyph approach pale mint-white
without bleaching the entire trail.

The glow pass reuses a pre-expanded alpha atlas with additive blending. It is
not physically based bloom, but at normal screen-saver viewing distance it
provides the soft optical halo at a fraction of the cost of render-to-texture
blur chains.

## Deliberate exclusions

MatrixCode does not implement:

- 3D camera motion;
- tumbling characters;
- perspective columns;
- a shader-only renderer;
- post-processing framebuffers;
- copied film glyph sheets;
- random Unicode rendered from a system font.

Those choices keep the image closer to the intended flat code-rain aesthetic
and make the program practical on machines with old Mesa stacks.
