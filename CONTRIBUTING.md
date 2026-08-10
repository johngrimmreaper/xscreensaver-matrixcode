# Contributing

Keep MatrixCode small, deterministic and usable on old X11/GLX systems.

Before submitting a change:

```sh
make clean
make
make check
```

Please preserve the clean-room artwork boundary: do not add extracted film
frames, the official Matrix font, promotional SWF glyph assets, or textures
copied from another recreation.

Visual changes should include a fixed-seed comparison frame and should explain
which aspect of the film/operator-display grammar they are trying to improve.
Geometry and motion fixes are preferred over simply increasing glow or green
saturation.
