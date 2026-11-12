#!/usr/bin/env python3
"""Assemble a compiler-free Bip Outdoor RES update from a released app bundle."""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
from typing import Optional
import struct
import sys
import urllib.error
import urllib.request

UPSTREAM_REPO = "ngxson/amazfit-bip-web"
UPSTREAM_COMMIT = "74ee2ba142f88d1a7d08babe61e8ff372b431661"
BASE_REL = "public/files/res/BipOS_0.5.5_b7_not_latin_1.1.2.05_ENGLISH.res"
RELOC_REL = "public/files/libbip/BipOS_0.5.5_b7_not_latin_1.1.2.05_ENGLISH.bin"
FLASHLIGHT_REL = "public/files/app/utility-flashlight-by-MNVolkov.elf"
# These pins are mirrored in the repository's candidate-inputs.sha256
# and suite-inputs.sha256. They protect downloads and local vendor archives.
PINS = {
    BASE_REL: "b886a0706898b33da36a4c4afe814b0a622b82279b59c3c745076eb6bf01eb26",
    RELOC_REL: "c1ee4044c3e1155150e312b6c6af3e2f8b93cdd7bdf2cec1074b099262959139",
    FLASHLIGHT_REL: "7c7cd652f4617edb20f40a768ea70ddc467381b2c1ed6a0fabd254239f6e2739",
}
APP_NAMES = ("waypoint_navigator", "tides_sun", "barometer", "gps_diagnostic")
FIRST_APP_RESOURCE = 930
MAX_RELOC_BYTES = 64 * 1024


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fetch_inputs(upstream_dir: Optional[Path]) -> dict[str, bytes]:
    result: dict[str, bytes] = {}
    for relative, expected in PINS.items():
        if upstream_dir is not None:
            path = upstream_dir / relative
            try:
                data = path.read_bytes()
            except OSError as exc:
                raise ValueError(f"cannot read upstream input {path}: {exc}") from exc
        else:
            url = f"https://raw.githubusercontent.com/{UPSTREAM_REPO}/{UPSTREAM_COMMIT}/{relative}"
            request = urllib.request.Request(url, headers={"User-Agent": "bip-outdoor-offline-packer/1"})
            try:
                with urllib.request.urlopen(request, timeout=30) as response:
                    data = response.read()
            except (OSError, urllib.error.URLError) as exc:
                raise ValueError(f"download failed for pinned upstream input {relative}: {exc}") from exc
        actual = sha256(data)
        if actual != expected:
            raise ValueError(f"SHA256 mismatch for {relative}: expected {expected}, got {actual}")
        result[relative] = data
    return result


def parse_res(data: bytes, label: str) -> list[bytes]:
    if len(data) < 0x24:
        raise ValueError(f"{label}: RES is shorter than its 36-byte header")
    if data[:5] != b"NERES":
        raise ValueError(f"{label}: missing NERES signature")
    count = struct.unpack_from("<I", data, 0x20)[0]
    table_end = 0x24 + count * 4
    if count == 0 or table_end > len(data):
        raise ValueError(f"{label}: invalid resource count or address table")
    offsets = struct.unpack_from(f"<{count}I", data, 0x24)
    if offsets[0] != 0:
        raise ValueError(f"{label}: first resource offset must be zero")
    payload_start = table_end
    if offsets[-1] > len(data) - payload_start:
        raise ValueError(f"{label}: last resource offset exceeds file length")
    resources = []
    previous = 0
    for index, start in enumerate(offsets):
        if start < previous:
            raise ValueError(f"{label}: resource offsets are not monotonic at index {index}")
        end = offsets[index + 1] if index + 1 < count else len(data) - payload_start
        if end < start or end > len(data) - payload_start:
            raise ValueError(f"{label}: resource {index} range is outside file bounds")
        resources.append(data[payload_start + start:payload_start + end])
        previous = start
    return resources


def make_res(header: bytes, resources: list[bytes]) -> bytes:
    if len(header) != 0x20:
        raise ValueError("baseline header must be exactly 32 bytes")
    if not resources:
        raise ValueError("resource count cannot be zero")
    offsets = []
    position = 0
    for resource in resources:
        if position > 0xFFFFFFFF:
            raise ValueError("resource payload exceeds NERES 32-bit offset range")
        offsets.append(position)
        position += len(resource)
    if position > 0xFFFFFFFF:
        raise ValueError("resource payload exceeds NERES 32-bit offset range")
    return header + struct.pack("<I", len(resources)) + struct.pack(f"<{len(offsets)}I", *offsets) + b"".join(resources)


def validate_elf(data: bytes, path: Path) -> None:
    if (len(data) < 52 or data[:4] != b"\x7fELF" or data[4:7] != b"\x01\x01\x01"
            or struct.unpack_from("<HHI", data, 16) != (2, 40, 1)):
        raise ValueError(f"{path}: expected 32-bit little-endian ARM ET_EXEC ELF")


def assemble(inputs: dict[str, bytes], apps: list[Path]) -> bytes:
    base = inputs[BASE_REL]
    original = parse_res(base, "baseline")
    if len(original) < FIRST_APP_RESOURCE:
        raise ValueError(f"baseline has {len(original)} resources; app scanning requires {FIRST_APP_RESOURCE}")
    reloc = inputs[RELOC_REL]
    if len(reloc) > MAX_RELOC_BYTES or not reloc.startswith(b"LIBBIP"):
        raise ValueError("relocation bin must begin with LIBBIP and be at most 65536 bytes")
    if any(resource.startswith(b"LIBBIP") for resource in original):
        raise ValueError("baseline already contains a LIBBIP relocation asset")
    app_data = []
    for path in apps:
        data = path.read_bytes()
        validate_elf(data, path)
        app_data.append(data)
    # Match prepare-suite.sh order, including the archived Flashlight.
    ordered_apps = app_data[:2] + [inputs[FLASHLIGHT_REL]] + app_data[2:]
    validate_elf(ordered_apps[2], Path("archived flashlight"))
    packed = make_res(base[:0x20], original + [reloc] + ordered_apps)
    reparsed = parse_res(packed, "packed output")
    if packed[:0x20] != base[:0x20] or reparsed[:len(original)] != original:
        raise ValueError("packed RES did not preserve baseline header and resources byte-for-byte")
    if reparsed[len(original)] != reloc or reparsed[len(original) + 1:] != ordered_apps:
        raise ValueError("packed RES failed relocation/app byte verification")
    return packed


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream-dir", type=Path, help="local vendor/bip-web archive directory (contains public/files)")
    parser.add_argument("--output", "-o", type=Path, default=Path("./bip-outdoor-suite.res"), help="new RES path (existing files are preserved)")
    args = parser.parse_args(argv)
    try:
        bundle = Path(__file__).resolve().parent
        app_paths = [bundle / "apps" / f"{name}.elf" for name in APP_NAMES]
        for path in app_paths:
            if not path.is_file():
                raise ValueError(f"missing app ELF: {path}")
        sums_path = bundle / "SHA256SUMS"
        try:
            lines = sums_path.read_text(encoding="ascii").splitlines()
        except OSError as exc:
            raise ValueError(f"cannot read {sums_path}: {exc}") from exc
        expected_sums = {f"apps/{name}.elf" for name in APP_NAMES}
        parsed_sums = {}
        for line in lines:
            if len(line) < 67 or line[64:66] != "  ":
                raise ValueError(f"invalid SHA256SUMS line: {line!r}")
            digest, filename = line[:64].lower(), line[66:]
            if any(char not in "0123456789abcdef" for char in digest) or filename in parsed_sums:
                raise ValueError(f"invalid or duplicate SHA256SUMS entry: {line!r}")
            parsed_sums[filename] = digest
        if set(parsed_sums) != expected_sums:
            raise ValueError("SHA256SUMS must contain exactly the four apps/*.elf files")
        for path in app_paths:
            relative = f"apps/{path.name}"
            actual = sha256(path.read_bytes())
            if actual != parsed_sums[relative]:
                raise ValueError(f"SHA256 mismatch for {relative}: expected {parsed_sums[relative]}, got {actual}")
        inputs = fetch_inputs(args.upstream_dir)
        packed = assemble(inputs, app_paths)
        output = args.output
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("xb") as stream:
            stream.write(packed)
        print(f"Wrote {output}: preserved 930+ baseline resources; added relocation, archived Flashlight, and {len(app_paths)} suite apps; {len(packed)} bytes.")
        return 0
    except (OSError, ValueError) as exc:
        print(f"assemble-res: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
