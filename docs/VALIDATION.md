# Validation

Validation date: 2026-08-10 UTC.

## Automated checks

`make check` runs:

1. `matrixcode -self-test`
   - validates the 57-base / 114-addressable clean-room glyph atlas;
   - validates deterministic simulation initialization;
   - checks 80x60 4:3 reference density and ~107x60 widescreen extension;
   - checks cursor population and glyph index bounds.
2. `tests/test-cli.sh`
   - verifies help/version/profile parsing;
   - verifies useful option boundaries;
   - checks malformed values, including rejection of negative unsigned seeds.
3. `tests/test-xvfb.sh`
   - renders a deterministic operator CRT frame through Xvfb + software GL;
   - checks dimensions, visible coverage, green dominance and bright cursor
     population;
   - confirms the default auto-aspect presentation populates the full 16:9 capture;
   - performs a KDE-like burst of real X11 resizes, verifies they debounce to one
     settled restart with added columns, then verifies maximize to 1920x1080
     restarts again with proportionally larger cells.

## Local container validation

The 0.2 rework was compiled with GCC using the repository's strict warning set
and completed warning-clean.  Internal self-tests passed.  A deterministic
1920x1080 frame was rendered through Mesa llvmpipe using:

```sh
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a \
  ./matrixcode -window -geometry 1920x1080 -frames 90 \
  -seed 19990331 -no-vsync -screenshot docs/film1999.ppm -verbose
```

The resulting reference image is `docs/film1999.png`.
