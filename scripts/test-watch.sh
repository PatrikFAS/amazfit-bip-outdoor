#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN=$(mktemp "${TMPDIR:-/tmp}/bip-watch-test.XXXXXX")
DIAG_BIN=$(mktemp "${TMPDIR:-/tmp}/bip-watch-diag-test.XXXXXX")
trap 'rm -f "$BIN" "$DIAG_BIN"' EXIT HUP INT TERM

${CC:-gcc} -DNAV_SMALL_MATH=1 -std=c99 -Wall -Wextra -Werror -pedantic \
    -Wno-pedantic -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -Wno-cast-function-type -I"$ROOT/tests/stubs" -I"$ROOT/src" \
    "$ROOT/tests/test_watch.c" "$ROOT/src/nav.c" "$ROOT/src/waypoints.c" -lm -o "$BIN"
"$BIN"

${CC:-gcc} -DNAV_SMALL_MATH=1 -std=c99 -Wall -Wextra -Werror -pedantic -DDIAGNOSTIC=1 \
    -Wno-pedantic -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -Wno-cast-function-type -I"$ROOT/tests/stubs" -I"$ROOT/src" \
    "$ROOT/tests/test_watch.c" "$ROOT/src/nav.c" "$ROOT/src/waypoints.c" -lm -o "$DIAG_BIN"
"$DIAG_BIN"
