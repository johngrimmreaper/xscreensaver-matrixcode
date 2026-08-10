# Validation

Validation date: 2026-08-10 UTC.

## Automated checks

`make check` runs:

1. `matrixcode -self-test`
   - validates the 57-base / 114-addressable clean-room glyph atlas;
   - validates deterministic simulation initialization;
   - checks operator-profile 4:3 geometry and 108-column grid;
   - checks cursor population and glyph index bounds.
2. `tests/test-cli.sh`
   - verifies help/version/profile parsing;
   - verifies useful option boundaries;
   - checks malformed values, including rejection of negative unsigned seeds.
3. `tests/test-xvfb.sh`
   - renders a deterministic operator CRT frame through Xvfb + software GL;
   - checks dimensions, visible coverage, green dominance and bright cursor
     population;
   - confirms the default 4:3 presentation leaves dark side areas in a 16:9
     capture.

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
