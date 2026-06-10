#!/bin/sh
set -eu

program=${1:-./matrixcode}
frames=${FRAMES:-600}
geometry=${GEOMETRY:-1920x1080}

if ! command -v xvfb-run >/dev/null 2>&1; then
    echo "xvfb-run is required" >&2
    exit 1
fi

start=$(date +%s%N)
LIBGL_ALWAYS_SOFTWARE=${LIBGL_ALWAYS_SOFTWARE:-1} xvfb-run -a \
  "$program" -window -geometry "$geometry" -frames "$frames" \
  -fps 120 -no-vsync >/dev/null
end=$(date +%s%N)
elapsed_ns=$((end - start))
python3 - "$frames" "$elapsed_ns" <<'PY'
import sys
frames = int(sys.argv[1])
seconds = int(sys.argv[2]) / 1_000_000_000
print(f"{frames} frames in {seconds:.3f}s ({frames / seconds:.1f} fps including startup)")
PY
