#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/preview
flags=(-DNAV_SMALL_MATH=1 -std=c99 -Wall -Wextra -Werror -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-cast-function-type -Itests/stubs -Isrc)
for mode in 0 1; do
    "${CC:-gcc}" "${flags[@]}" -DDIAGNOSTIC="$mode" tests/preview_watch.c src/nav.c src/waypoints.c -lm -o "build/preview/capture-$mode"
    "build/preview/capture-$mode" > "build/preview/screens-$mode.tsv"
done
"${CC:-gcc}" -std=c99 -Wall -Wextra -Werror -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Itests/stubs_tides -Isrc -Ibuild tests/preview_tides_sun.c src/outdoor_data.c -lm -o build/preview/capture-tides
build/preview/capture-tides > build/preview/screens-tides.tsv
"${CC:-gcc}" -std=c99 -Wall -Wextra -Werror -DWATCH_PREVIEW -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Itests/stubs_barometer tests/test_barometer.c -lm -o build/preview/capture-barometer
build/preview/capture-barometer > build/preview/screens-barometer.tsv
"${CC:-gcc}" -std=c99 -Wall -Wextra -Werror -DWATCH_PREVIEW -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Itests/stubs_compass tests/test_compass.c -o build/preview/capture-compass
build/preview/capture-compass > build/preview/screens-compass.tsv
python3 scripts/render-preview.py build/preview/screens-0.tsv build/preview/screens-1.tsv build/preview/screens-tides.tsv build/preview/screens-barometer.tsv build/preview/screens-compass.tsv
if command -v rsvg-convert >/dev/null; then
    rsvg-convert build/preview/screens.svg -o build/preview/screens.png
fi
echo 'Preview: build/preview/screens.svg (and screens.png when rsvg-convert is available)'
