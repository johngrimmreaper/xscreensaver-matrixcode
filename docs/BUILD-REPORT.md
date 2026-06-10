# Build report for 0.1.0

Validation date: 2026-06-10 UTC

## Environment

- Host distribution: Debian 13 (trixie)
- C compilers: GCC 14.2 and Clang
- X server for tests: Xvfb
- OpenGL renderer: Mesa llvmpipe, OpenGL 4.5 compatibility profile
- Package target: Debian/Ubuntu, with the changelog prepared for Ubuntu 24.04
  Noble

The validation container had the OpenGL runtime libraries but not the normal
`libgl-dev` headers. The project therefore exercised its small compatibility
header path while linking to the real GLVND `libGL.so.1`. Production Debian and
Ubuntu package builds declare `libgl-dev` and use the normal system headers.

## Completed checks

- Warning-clean optimized GCC build
- Warning-clean optimized Clang build
- AddressSanitizer and UndefinedBehaviorSanitizer self-tests
- Clang static analyzer with no findings after fixes
- Deterministic simulation and glyph-atlas self-tests
- Command-line valid/invalid boundary tests
- GLX preview-window rendering through Xvfb and llvmpipe
- GLX root-window rendering
- Embedded rendering through `XSCREENSAVER_WINDOW`
- Byte-for-byte reproducibility of two fixed-seed frame captures
- PPM image checks for dimensions, visible coverage, green dominance, and
  pale leading pixels
- Hardened build using `dpkg-buildflags` with PIE, stack protection, RELRO,
  and immediate binding
- Staged installation path and mode validation
- Debian `3.0 (quilt)` source-package generation, extraction, rebuild, and
  complete test-suite round trip
- XML parsing of the XScreenSaver configuration before and after installation

A 1920x1080 llvmpipe run rendered 300 frames in 2.975 seconds, approximately
100.8 frames per second including Xvfb and process startup. This is not a
portable benchmark, but it confirms ample headroom for the default 30 FPS cap
using software rendering in this environment.

## Source package result

The following source-package components were generated successfully outside the
Git working tree:

```text
xscreensaver-matrixcode_0.1.0.orig.tar.gz
xscreensaver-matrixcode_0.1.0-1~noble1.debian.tar.xz
xscreensaver-matrixcode_0.1.0-1~noble1.dsc
```

They were extracted with `dpkg-source -x`; the extracted tree compiled and
passed all tests again.

## Local limitation

A complete `dpkg-buildpackage -b` run was not possible in this particular
container because `debhelper` and the OpenGL development package were not
installed and its APT repositories were unavailable. The repository includes a
Ubuntu 24.04 GitHub Actions job that installs those declared build dependencies,
runs `make check`, builds the binary package with `dpkg-buildpackage`, and runs
Lintian. The source package and exact staged payload were both validated
locally.
