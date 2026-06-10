# Contributing

Changes should preserve the project's clean-room boundary, lightweight
renderer, and sparse 2D visual target.

Before proposing a change:

```sh
make clean
make
make check
```

Please keep commits focused and explain any visual change with a deterministic
seed, geometry, and before/after screenshots. New glyphs must be original work;
do not submit extracted film assets, official font data, or copied textures.

Code should remain warning-clean with the flags in the Makefile. Avoid
per-frame allocations and do not make programmable shaders mandatory. An
optional modern rendering backend may be considered only if the fixed-function
path remains fully supported.
