#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="$(mktemp)"
trap 'rm -f "$out"' EXIT
${CC:-cc} -std=c99 -Wall -Wextra -Werror -DTIDES_SUN_TEST \
  -I"$root/tests/stubs_tides" -I"$root/src" -I"$root/build" \
  "$root/tests/test_tides_sun.c" "$root/src/outdoor_data.c" -lm -o "$out"
"$out"
