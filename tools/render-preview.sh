#!/bin/sh
set -eu

program=${1:-./matrixcode}
out=${2:-docs/preview.ppm}

mkdir -p "$(dirname "$out")"
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a \
  "$program" -window -geometry 1280x720 -frames 70 -fps 60 \
  -seed 19990331 -no-vsync -screenshot "$out"
python3 tests/check-ppm.py "$out"
echo "Wrote $out"
