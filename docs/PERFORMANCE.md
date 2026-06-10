# Performance architecture

MatrixCode is optimized around bounded, predictable work.

## Rendering cost

For a 1920x1080 display at the default 18-pixel cell height, the logical grid
is roughly 166 by 62 cells. Only occupied cells in active columns with visible
illumination issue quads. The default 42 percent active-column setting keeps
actual drawing substantially below the full grid.

The render path uses:

- one core texture atlas and one glow atlas;
- two immediate-mode quad passes;
- orthographic projection;
- alpha/additive blending;
- one simulation update per frame;
- no per-frame allocation;
- no shaders, VBO requirement, FBO, depth buffer, or 3D transforms.

The atlas is generated once from compact 8x12 source bitmaps. Glyph lookup is a
texture-coordinate calculation, not text shaping or font rasterization.

## Scaling controls

The most useful controls on slow systems are, in order:

1. `-fps 20` or lower;
2. larger `-cell-size` values, which reduce grid dimensions;
3. lower `-density`;
4. lower `-glow`, including zero to omit the additive pass.

Software Mesa can render the deterministic Xvfb test, making the program
suitable for automated package validation without a physical GPU.

## Portability

The renderer intentionally stays within fixed-function OpenGL 1.x and baseline
GLX calls. Optional swap-interval functions are discovered at runtime and are
never required. Both double-buffered and single-buffered GLX visuals are
accepted.
