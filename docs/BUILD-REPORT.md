# Build and validation report — 0.2 film rework

Validation date: 2026-08-10 UTC

## Environment

- Linux container
- GCC and Clang available
- X server: Xvfb
- OpenGL renderer: Mesa llvmpipe compatibility profile
- Container has runtime `libGL.so.1`; the repository's minimal-header fallback
  was therefore exercised. Normal Debian/Ubuntu package builds use `libgl-dev`.

## Completed checks

- warning-clean optimized GCC build;
- warning-clean optimized Clang build;
- AddressSanitizer + UndefinedBehaviorSanitizer self-tests;
- internal deterministic simulation / geometry tests;
- CLI valid/invalid boundary tests;
- explicit rejection of a negative unsigned seed;
- deterministic software-GLX render through Xvfb;
- image validation for dimensions, coverage, green dominance, pale cursors and
  dark 4:3 side apertures;
- XML parsing of the XScreenSaver settings file;
- staged `make install` verification for executable/config/desktop/man paths.

The deterministic graphical test reports approximately 26% visible pixels in
its 1280x720 test frame and hundreds of pale cursor pixels; exact counts are
seed/profile dependent and are intentionally not treated as a visual benchmark.

## Packaging limitation in this container

`dpkg-buildpackage` is present, but `debhelper` and the normal OpenGL development
headers are not installed in this container, so a full binary Debian package
was not built here.  The Debian metadata and GitHub Actions workflow retain the
normal `libgl-dev`/debhelper build path for Ubuntu 24.04.
