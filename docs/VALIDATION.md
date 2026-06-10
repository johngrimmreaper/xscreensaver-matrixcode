# Validation

## Automated checks

`make check` runs three layers:

1. `matrixcode -self-test`
   - verifies glyph-table dimensions and names;
   - builds and validates both texture atlases;
   - checks deterministic pseudo-random simulation behavior;
   - exercises resize/reinitialization invariants.
2. `tests/test-cli.sh`
   - checks help and version output;
   - verifies accepted parameter boundaries;
   - confirms malformed or out-of-range options fail.
3. `tests/test-xvfb.sh`
   - starts a software-rendered GLX X server;
   - runs a fixed-seed 800x600 scene for 45 frames;
   - captures the final framebuffer as PPM;
   - validates dimensions, non-black coverage, green dominance, and a minimum
     population of bright leading pixels.

## Additional package checks

The Debian autopkgtest repeats the internal test and software-GLX screenshot
check against the installed `/usr/libexec/xscreensaver/matrixcode` executable.
The GitHub Actions workflow builds on Ubuntu 24.04, runs the source test suite,
and invokes `dpkg-buildpackage`.

## Local validation record

The initial 0.1.0 source was compiled with GCC-compatible warning flags that
include conversion, shadow, format, prototype, cast-qual and undefined-macro
checks. It completed the deterministic, CLI and software-GLX rendering tests
without warnings or test failures. The generated 1280x720 reference image is
stored as `docs/preview.png`.
