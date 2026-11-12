#!/usr/bin/env bash
# Resources-only update for the matching English BipOS installation.
set -euo pipefail
cd "$(dirname "$0")/.."
output="${1:-build/bip-outdoor-suite}"
repo=vendor/bip-web
reference="${2:-}"
[[ -z "$reference" || "$reference" == --navigation-reference ]] || { echo 'Unknown option' >&2; exit 1; }
sha256sum -c scripts/suite-inputs.sha256
python3 scripts/check_elf.py
python3 scripts/check-imports.py
[[ ! -e "$output" && ! -e "$output.zip" ]] || { echo 'Choose a new output directory; previous review artifacts are preserved.' >&2; exit 1; }
mkdir -p "$output"
apps=(
    build/waypoint_navigator.elf
    build/tides_sun.elf
    "$repo/public/files/app/utility-flashlight-by-MNVolkov.elf"
    build/barometer.elf
    build/gps_diagnostic.elf
)
if [[ "$reference" == --navigation-reference ]]; then
    sha256sum -c scripts/nav-reference-inputs.sha256
    prefix="${ARM_PREFIX:-$PWD/.tools/gcc-arm-none-eabi-7-2018-q2-update/bin/arm-none-eabi-}"
    "${prefix}objcopy" --strip-debug --discard-all \
        "$repo/public/files/app/utility-gps-returner-by-x27.elf" "$output/gps_returner.reference.elf"
    python3 scripts/check-elf-trim.py "$repo/public/files/app/utility-gps-returner-by-x27.elf" "$output/gps_returner.reference.elf"
    python3 scripts/check-reference-app.py "$output/gps_returner.reference.elf"
    apps+=("$output/gps_returner.reference.elf")
fi
args=()
for app in "${apps[@]}"; do args+=(--app "$app"); done
node scripts/package-res.mjs \
    --base "$repo/public/files/res/BipOS_0.5.5_b7_not_latin_1.1.2.05_ENGLISH.res" \
    --reloc "$repo/public/files/libbip/BipOS_0.5.5_b7_not_latin_1.1.2.05_ENGLISH.bin" \
    --out "$output/bip-outdoor-suite.res" "${args[@]}"
cp docs/INSTALL-SUITE.md "$output/INSTALL-SUITE.md"
cp docs/COMPASS-CHECK.md "$output/COMPASS-CHECK.md"
cp docs/WAYPOINTS.md "$output/WAYPOINTS.md"
cp build/tide_snapshot_manifest.json "$output/TIDE-PROVENANCE.json"
python3 - "$output" "${apps[@]}" <<'PY'
import hashlib
import json
from pathlib import Path
import struct
import sys
from zipfile import ZipFile, ZIP_DEFLATED
folder = Path(sys.argv[1])
manifest = []
for app in map(Path, sys.argv[2:]):
    data = app.read_bytes()
    phoff = struct.unpack_from('<I', data, 28)[0]
    size, count = struct.unpack_from('<HH', data, 42)
    headers = [struct.unpack_from('<8I', data, phoff + i * size) for i in range(count)]
    manifest.append({'app': app.name, 'sha256': hashlib.sha256(data).hexdigest(),
                     'file_bytes': len(data), 'pt_load_bytes': sum(h[5] for h in headers if h[0] == 1)})
(folder / 'APP-MANIFEST.json').write_text(json.dumps(manifest, indent=2) + '\n')
files = sorted(folder.iterdir())
(folder / 'SHA256SUMS').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest() + '  ' + f.name + '\n' for f in files))
archive = Path(str(folder) + '.zip')
with ZipFile(archive, 'x', ZIP_DEFLATED) as zipfile:
    for file in sorted(folder.iterdir()):
        zipfile.write(file, arcname=folder.name + '/' + file.name)
with ZipFile(archive) as zipfile:
    for file in folder.iterdir():
        assert zipfile.read(folder.name + '/' + file.name) == file.read_bytes()
print(f'Suite resources-only review archive: {archive}; ZIP byte roundtrip passed')
PY
