#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
prefix="${ARM_PREFIX:-$PWD/.tools/gcc-arm-none-eabi-7-2018-q2-update/bin/arm-none-eabi-}"
cc="${prefix}gcc"
[[ -x "$cc" ]] || { echo 'Run make bootstrap first, or set ARM_PREFIX.' >&2; exit 1; }
[[ -f vendor/libbip/lib/libbip.a ]] || { echo 'Missing pinned libbip; run make bootstrap.' >&2; exit 1; }
mkdir -p build
python3 scripts/configure-tides.py
flags=(-std=c99 -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -mlittle-endian -Os -fno-math-errno -fpie -fpic -ffunction-sections -fdata-sections -fno-common -Wall -Wextra -Werror -Wno-main -Ibuild -Isrc -Ivendor/libbip)
"$cc" "${flags[@]}" -DNAV_SMALL_MATH=1 -c src/nav.c -o build/nav.o
"$cc" "${flags[@]}" -DNAV_SMALL_MATH=1 -flto -c src/nav.c -o build/nav.lto.o
"$cc" "${flags[@]}" -flto -c src/waypoints.c -o build/waypoints.o
"$cc" "${flags[@]}" -c src/waypoints.c -o build/waypoints.imports.o
"$cc" "${flags[@]}" -flto -c src/outdoor_data.c -o build/outdoor_data.o
# Non-LTO audit objects expose every source import to the compatibility checker.
"$cc" "${flags[@]}" -c src/outdoor_data.c -o build/outdoor_data.imports.o
for app in hello gps_diagnostic waypoint_navigator tides_sun barometer compass_diagnostic; do
    sources=("build/$app.o")
    link_flags=()
    if [[ "$app" == hello ]]; then
        "$cc" "${flags[@]}" -c src/hello.c -o "${sources[0]}"
    elif [[ "$app" == tides_sun ]]; then
        "$cc" "${flags[@]}" -flto -c src/tides_sun.c -o "${sources[0]}"
        "$cc" "${flags[@]}" -c src/tides_sun.c -o build/tides_sun.imports.o
        sources+=(build/outdoor_data.o)
        link_flags=(-flto)
    elif [[ "$app" == barometer || "$app" == compass_diagnostic ]]; then
        "$cc" "${flags[@]}" -c "src/$app.c" -o "${sources[0]}"
    elif [[ "$app" == waypoint_navigator ]]; then
        "$cc" "${flags[@]}" -flto -DDIAGNOSTIC=0 -c src/watch_app.c -o "${sources[0]}"
        "$cc" "${flags[@]}" -DDIAGNOSTIC=0 -c src/watch_app.c -o build/waypoint_navigator.imports.o
        sources+=(build/nav.lto.o build/waypoints.o)
        link_flags=(-flto)
    else
        mode=0; [[ "$app" != gps_diagnostic ]] || mode=1
        "$cc" "${flags[@]}" -DDIAGNOSTIC="$mode" -c src/watch_app.c -o "${sources[0]}"
        sources+=(build/nav.o)
    fi
    # Preserve upstream GNU ld PIE layout and libbip _start import trampoline.
    "$cc" "${link_flags[@]}" -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -nostdlib \
        -Wl,-EL,-N,-pie,--no-dynamic-linker,--gc-sections,--cref,-e,_start,-Map,"build/$app.map" \
        -o "build/$app.elf" "${sources[@]}" vendor/libbip/lib/libbip.a -lm -lc -lgcc
    label="$app"
    [[ "$app" != tides_sun ]] || label='Tides & Sun'
    [[ "$app" != barometer ]] || label='Barometer'
    [[ "$app" != compass_diagnostic ]] || label='Compass Check'
    printf '%s' "$label" > "build/$app.label"
    "${prefix}objcopy" --add-section ".elf.label=build/$app.label" "build/$app.elf"
    if [[ "$app" == waypoint_navigator ]]; then
        python3 -c 'from pathlib import Path; Path("build/waypoint.settings").write_bytes(bytes(114))'
        "${prefix}objcopy" --add-section .elf.settings=build/waypoint.settings "build/$app.elf"
    fi
    if [[ "$app" == barometer ]]; then
        python3 -c 'from pathlib import Path; Path("build/barometer.settings").write_bytes(bytes(10))'
        "${prefix}objcopy" --add-section .elf.settings=build/barometer.settings "build/$app.elf"
    fi
    if [[ "$app" == waypoint_navigator || "$app" == tides_sun || "$app" == barometer || "$app" == compass_diagnostic ]]; then
        cp "build/$app.elf" "build/$app.untrimmed.elf"
        "${prefix}objcopy" --strip-debug --discard-all "build/$app.elf"
        ARM_PREFIX="$prefix" python3 scripts/check-elf-trim.py "build/$app.untrimmed.elf" "build/$app.elf"
    fi
    "${prefix}size" "build/$app.elf"
done
python3 scripts/check_elf.py
