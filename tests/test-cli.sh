#!/bin/sh
set -eu
p=${1:-./matrixcode}

"$p" -version | grep -F 'matrixcode 0.2.0-film-rework' >/dev/null
"$p" -help | grep -F 'operator1999' >/dev/null
"$p" -help | grep -F 'neo-terminal' >/dev/null
"$p" -help | grep -F 'scene-time' >/dev/null
"$p" -self-test | grep -F 'all self-tests passed' >/dev/null

# Parsing should accept profile and useful boundaries without needing X when
# combined with self-test (parsing still happens before the test is selected).
"$p" -profile operator1999 -no-scene -columns 40 -density 10 -speed 25 -trail 6 \
     -cycle 0 -glow 0 -contrast 20 -curvature 0 -scanlines 0 \
     -phosphor-mask 0 -vignette 0 -persistence 0 -overscan 0 -self-test >/dev/null
"$p" -profile opening1999 -columns 240 -density 90 -speed 250 -trail 40 \
     -cycle 300 -glow 100 -contrast 100 -self-test >/dev/null

if "$p" -profile nope -self-test >/dev/null 2>&1; then
    echo 'invalid profile unexpectedly accepted' >&2; exit 1
fi
if "$p" -seed -1 -self-test >/dev/null 2>&1; then
    echo 'negative unsigned seed unexpectedly accepted' >&2; exit 1
fi
if "$p" -columns 39 -self-test >/dev/null 2>&1; then
    echo 'out-of-range columns unexpectedly accepted' >&2; exit 1
fi
if "$p" -aspect 5:4 -self-test >/dev/null 2>&1; then
    echo 'invalid aspect unexpectedly accepted' >&2; exit 1
fi

"$p" -scene neo-terminal -self-test >/dev/null
"$p" -scene neo-trace -scene-time 8.5 -self-test >/dev/null
"$p" -scene neo-terminal -scene-time 0 -self-test >/dev/null
"$p" -scene neo-terminal -scene-time 23.5 -self-test >/dev/null
"$p" -scene neo-terminal -scene-time 76 -self-test >/dev/null
"$p" -no-scene -self-test >/dev/null
if "$p" -scene bogus -self-test >/dev/null 2>&1; then
    echo 'invalid scene unexpectedly accepted' >&2; exit 1
fi
if "$p" -scene neo-terminal -scene-time -0.1 -self-test >/dev/null 2>&1; then
    echo 'negative scene time unexpectedly accepted' >&2; exit 1
fi
if "$p" -scene neo-terminal -scene-time 76.1 -self-test >/dev/null 2>&1; then
    echo 'scene time beyond scene duration unexpectedly accepted' >&2; exit 1
fi
if "$p" -scene neo-terminal -scene-time nan -self-test >/dev/null 2>&1; then
    echo 'non-finite scene time unexpectedly accepted' >&2; exit 1
fi
if "$p" -no-scene -scene-time 1 -self-test >/dev/null 2>&1; then
    echo 'scene time without a scene unexpectedly accepted' >&2; exit 1
fi
