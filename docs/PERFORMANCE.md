# Performance architecture

MatrixCode keeps per-frame work bounded and predictable.

For the default operator profile, the logical grid is only about 108x81 cells.
Cells that are unoccupied or outside the current illumination phase are skipped.
The renderer performs immediate-mode quads from two small pre-generated texture
atlases and adds inexpensive line overlays for CRT scan structure.

The hot path has:

- no per-frame heap allocation;
- no font rasterization;
- no shaders;
- no VBO requirement;
- no framebuffer-object bloom chain;
- no depth buffer or 3D scene;
- no image convolution at screen resolution.

The strongest performance controls are:

1. `-fps 20`;
2. lower `-columns`;
3. lower `-density`;
4. lower or disable `-glow`;
5. `-no-crt` if line-overlay cost matters on an exceptionally weak driver.

Software Mesa/llvmpipe is supported for deterministic Xvfb validation.
