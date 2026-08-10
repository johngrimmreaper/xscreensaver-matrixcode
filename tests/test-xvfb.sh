#!/bin/sh
set -eu
p=${1:-./matrixcode}
out=${TMPDIR:-/tmp}/matrixcode-film-test.ppm
rm -f "$out"
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a \
  "$p" -window -profile operator1999 -geometry 1280x720 \
  -frames 60 -fps 60 -seed 19990331 -no-vsync -screenshot "$out"
python3 tests/check-ppm.py "$out"
rm -f "$out"
