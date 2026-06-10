#!/bin/sh
set -eu

program=${1:-./matrixcode}
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/matrixcode-test.XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

if ! command -v xvfb-run >/dev/null 2>&1; then
    echo 'SKIP: xvfb-run is not installed'
    exit 0
fi

run_owned()
{
    output=$1
    LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a \
        "$program" -window -geometry 800x600 -frames 45 -fps 60 \
        -seed 19990331 -no-vsync -screenshot "$output"
    python3 "$(dirname "$0")/check-ppm.py" "$output"
}

run_owned "$tmpdir/owned-a.ppm"
run_owned "$tmpdir/owned-b.ppm"
cmp "$tmpdir/owned-a.ppm" "$tmpdir/owned-b.ppm"

LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a \
    "$program" -root -frames 8 -fps 60 -seed 19990331 \
    -no-vsync -screenshot "$tmpdir/root.ppm"
python3 "$(dirname "$0")/check-ppm.py" "$tmpdir/root.ppm"

if command -v xmessage >/dev/null 2>&1 && command -v xwininfo >/dev/null 2>&1; then
    xvfb-run -a sh -eu -c '
        program=$1
        output=$2
        xmessage -name matrixcode-target -buttons "" -geometry 640x480+0+0 \
            target >/dev/null 2>&1 &
        target_pid=$!
        trap '\''kill "$target_pid" 2>/dev/null || true'\'' EXIT HUP INT TERM
        sleep 1
        window_id=$(xwininfo -name matrixcode-target |
            awk '\''/Window id:/{print $4; exit}'\'')
        test -n "$window_id"
        XSCREENSAVER_WINDOW=$window_id LIBGL_ALWAYS_SOFTWARE=1 \
            "$program" -frames 8 -fps 60 -seed 19990331 \
            -no-vsync -screenshot "$output"
    ' sh "$program" "$tmpdir/embedded.ppm"
    python3 "$(dirname "$0")/check-ppm.py" "$tmpdir/embedded.ppm"
else
    echo 'SKIP: xmessage or xwininfo is not installed; embedded-window test omitted'
fi
