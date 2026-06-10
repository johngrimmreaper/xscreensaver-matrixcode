#!/bin/sh
set -eu

program=${1:-./matrixcode}

"$program" -version | grep -Eq '^matrixcode [0-9]+\.[0-9]+\.[0-9]+'
"$program" -help | grep -q -- '-window-id'
"$program" -self-test | grep -q 'all self-tests passed'

if "$program" -density 101 >/dev/null 2>&1; then
    echo 'invalid density was accepted' >&2
    exit 1
fi

if "$program" -geometry bad >/dev/null 2>&1; then
    echo 'invalid geometry was accepted' >&2
    exit 1
fi
