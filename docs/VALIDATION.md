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
invokes `dpkg-buildpackage`, and runs Lintian.

## Local validation record

The initial 0.1.0 source was compiled with both GCC and Clang under warning
flags covering conversion, shadow, format, prototypes, cast qualification, and
undefined macros. AddressSanitizer, UndefinedBehaviorSanitizer, Clang static
analysis, deterministic CLI tests, root-window tests, embedded-window tests,
hardened staging, and a Debian source-package round trip all completed without
findings or failures. The generated 1280x720 reference image is stored as
`docs/preview.png`.

The exact local environment, results, performance sample, and one acknowledged
container limitation are recorded in [BUILD-REPORT.md](BUILD-REPORT.md).
