#!/bin/sh
set -eu
p=${1:-./matrixcode}
tmp=${TMPDIR:-/tmp}
out=$tmp/matrixcode-film-test.ppm
helper=$tmp/matrixcode-resize-window.$$
log=$tmp/matrixcode-resize-test.$$.log
rm -f "$out" "$helper" "$log"
trap 'rm -f "$out" "$helper" "$log"' EXIT HUP INT TERM

LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a \
  "$p" -window -profile operator1999 -no-scene -geometry 1280x720 \
  -frames 60 -fps 60 -seed 19990331 -no-vsync -screenshot "$out"
python3 tests/check-ppm.py "$out"

# Exercise the real ConfigureNotify path.  The helper finds the owned test
# window by name and asks X11 to resize it, just as a window manager would.
${CC:-cc} ${CFLAGS:-} -Wall -Wextra -Werror \
  -o "$helper" tests/resize-window.c $(pkg-config --cflags --libs x11)
MATRIXCODE_TEST_PROGRAM="$p" MATRIXCODE_RESIZE_HELPER="$helper" \
  LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a sh -eu -c '
    "$MATRIXCODE_TEST_PROGRAM" -window -no-scene -geometry 960x720 -speed 40 \
      -verbose -no-vsync 2>"$1" &
    pid=$!
    trap "kill $pid 2>/dev/null || true" EXIT HUP INT TERM
    sleep 0.2
    # Mimic a KDE interactive horizontal resize.  The renderer should debounce
    # this storm and rebuild once at the final width, adding columns while the
    # 60-row reference density and glyph height stay stable.
    for w in 980 1005 1064 1118 1175 1209 1239 1436; do
      "$MATRIXCODE_RESIZE_HELPER" "$w" 720
      sleep 0.02
    done
    sleep 0.35
    # Then mimic maximize to a larger 16:9 drawable: logical density remains
    # ~107x60 while physical glyphs grow with the larger height.
    "$MATRIXCODE_RESIZE_HELPER" 1920 1080
    sleep 0.35
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  ' sh "$log"
grep -F 'profile: operator1999, grid: 80x60' "$log" >/dev/null
grep -F 'resize restart 1: 1436x720, grid 120x60' "$log" >/dev/null
grep -F 'resize restart 2: 1920x1080, grid 107x60' "$log" >/dev/null
test "$(grep -c '^resize restart ' "$log")" -eq 2
