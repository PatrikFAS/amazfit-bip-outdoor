#!/usr/bin/env python3
"""Check built app imports against a candidate BipOS LIBBIP table."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CANDIDATE = ROOT / "vendor/bip-web/public/files/libbip/BipOS_0.5.5_b7_not_latin_1.1.2.05_ENGLISH.bin"
DEFAULT_SDK = ROOT / "vendor/libbip/reloc/reloc_not_latin_1.1.2.05.bin"
DEFAULT_OBJECTS = [ROOT / f"build/{name}.o" for name in ("hello", "gps_diagnostic", "waypoint_navigator.imports", "nav", "waypoints.imports", "tides_sun.imports", "outdoor_data.imports", "barometer", "compass_diagnostic")]
DEFAULT_IMPORTS = ROOT / "vendor/libbip/src/not_latin_1.1.2.05/import.s"
DEFAULT_ALIASES = ROOT / "vendor/libbip/src/alias.s"
DEFAULT_PROXY = ROOT / "vendor/libbip/src/proxy/import.s"


class CheckError(Exception):
    pass


def choose_nm() -> str:
    prefix = os.environ.get("ARM_PREFIX")
    if prefix:
        arm_nm = Path(f"{prefix}nm")
        if arm_nm.is_file():
            return str(arm_nm)
    arm_nm = ROOT / ".tools/gcc-arm-none-eabi-7-2018-q2-update/bin/arm-none-eabi-nm"
    if arm_nm.is_file():
        return str(arm_nm)
    host_nm = shutil.which("nm")
    if host_nm:
        return host_nm
    raise CheckError("could not find arm-none-eabi-nm or host nm")


def run_nm(nm: str, args: list[str], paths: list[Path]) -> str:
    missing = [str(p) for p in paths if not p.is_file()]
    if missing:
        raise CheckError("missing input file(s): " + ", ".join(missing))
    result = subprocess.run([nm, *args, *(str(p) for p in paths)], check=False, capture_output=True, text=True)
    if result.returncode or "plugin needed" in result.stderr.lower():
        raise CheckError(f"nm could not inspect all symbols ({result.returncode}): {result.stderr.strip()}")
    return result.stdout


def parse_symbols(output: str, undefined: bool) -> set[str]:
    symbols: set[str] = set()
    for line in output.splitlines():
        if undefined:
            match = re.match(r"^\s+U\s+(\S+)\s*$", line)
            if match:
                symbols.add(match.group(1))
        else:
            match = re.match(r"^\S+\s+[A-Za-z]\s+(\S+)\s*$", line)
            if match:
                symbols.add(match.group(1))
    return symbols


def parse_aliases(path: Path) -> dict[str, int]:
    aliases: dict[str, int] = {}
    pattern = re.compile(r"^\s*\.set\s+alias_(\w+)\s*,\s*(0x[0-9a-fA-F]+)")
    for line in path.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if match:
            aliases[f"alias_{match.group(1)}"] = int(match.group(2), 16)
    if not aliases:
        raise CheckError(f"no aliases parsed from {path}")
    return aliases


def parse_sdk_imports(path: Path) -> dict[str, tuple[int, str]]:
    imports: dict[str, tuple[int, str]] = {}
    pattern = re.compile(r"^\s*import\s+(0x[0-9a-fA-F]+)\s+(\w+)\s+(alias_\w+)")
    for line in path.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if match:
            imports[match.group(2)] = (int(match.group(1), 16), match.group(3))
    if not imports:
        raise CheckError(f"no versioned imports parsed from {path}")
    return imports


def parse_fixed_imports(path: Path) -> set[str]:
    fixed: set[str] = set()
    pattern = re.compile(r"^\s*import\s+0x(?:FADE|BEAD)[0-9a-fA-F]+\s+(\w+)")
    for line in path.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if match:
            fixed.add(match.group(1))
    return fixed


def parse_relocation(path: Path) -> tuple[dict[int, list[int]], int, int]:
    data = path.read_bytes()
    if len(data) < 32 or data[:6] != b"LIBBIP":
        raise CheckError(f"{path}: invalid LIBBIP relocation header")
    alias_bytes = struct.unpack_from("<I", data, 8)[0]
    fw_id = struct.unpack_from("<I", data, 12)[0]
    if alias_bytes == 0 or alias_bytes % 4:
        raise CheckError(f"{path}: invalid alias table size {alias_bytes}")
    count = alias_bytes // 4
    expected_size = 32 + 2 * alias_bytes
    if len(data) < expected_size:
        raise CheckError(f"{path}: truncated table: expected at least {expected_size} bytes, got {len(data)}")
    keys = struct.unpack_from(f"<{count}I", data, 32)
    values = struct.unpack_from(f"<{count}I", data, 32 + alias_bytes)
    table: dict[int, list[int]] = {}
    for key, value in zip(keys, values):
        table.setdefault(key, []).append(value)
    return table, count, fw_id


def is_toolchain_symbol(name: str) -> bool:
    return name.startswith("__aeabi_") or name.startswith("__gnu_") or name in {
        "atan2f", "acosf", "cosf", "sinf", "sqrtf", "fabsf", "floorf", "powf", "memset", "memcpy", "memcmp", "_GLOBAL_OFFSET_TABLE_", "atan2", "cos", "fabs", "floor", "llround", "sin", "sqrt"
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", type=Path, default=DEFAULT_CANDIDATE, help="candidate LIBBIP relocation binary")
    parser.add_argument("--sdk", type=Path, default=DEFAULT_SDK, help="SDK relocation binary for the matching firmware")
    parser.add_argument("--imports", type=Path, default=DEFAULT_IMPORTS, help="firmware import.s used to validate SDK addresses")
    parser.add_argument("--aliases", type=Path, default=DEFAULT_ALIASES, help="shared alias.s")
    parser.add_argument("--proxy", type=Path, default=DEFAULT_PROXY, help="proxy import.s containing fixed BipOS aliases")
    parser.add_argument("--objects", nargs="+", type=Path, default=DEFAULT_OBJECTS, help="built application object files (default: all three project apps)")
    parser.add_argument("--nm", help="nm executable; defaults to ARM toolchain nm, then host nm")
    args = parser.parse_args()

    try:
        nm = args.nm or choose_nm()
        unresolved = parse_symbols(run_nm(nm, ["-u"], args.objects), undefined=True)
        defined = parse_symbols(run_nm(nm, ["-g", "--defined-only"], args.objects), undefined=False)
        used = unresolved - defined

        aliases = parse_aliases(args.aliases)
        imports = parse_sdk_imports(args.imports)
        fixed = parse_fixed_imports(args.proxy)
        sdk, sdk_count, sdk_fw_id = parse_relocation(args.sdk)
        candidate, candidate_count, candidate_fw_id = parse_relocation(args.candidate)

        failures: list[str] = []
        if sdk_fw_id != candidate_fw_id:
            failures.append(f"firmware ID mismatch: SDK 0x{sdk_fw_id:08x}, candidate 0x{candidate_fw_id:08x}")

        checked: list[str] = []
        fixed_used: list[str] = []
        toolchain: list[str] = []
        unclassified: list[str] = []
        required_aliases: set[int] = set()
        for name in sorted(used):
            if name in imports:
                address, alias_name = imports[name]
                key = aliases.get(alias_name)
                if key is None:
                    failures.append(f"{name}: alias {alias_name} missing from {args.aliases}")
                    continue
                required_aliases.add(key)
                expected = address + 1  # Thumb function pointer emitted by import.s
                sdk_values = sdk.get(key, [])
                candidate_values = candidate.get(key, [])
                if not sdk_values:
                    failures.append(f"{name}: alias 0x{key:08x} is absent from SDK table")
                elif len(set(sdk_values)) != 1:
                    failures.append(f"{name}: SDK alias 0x{key:08x} is duplicated with conflicting addresses")
                elif sdk_values[0] != expected:
                    failures.append(f"{name}: SDK alias 0x{key:08x} is 0x{sdk_values[0]:08x}, import.s expects 0x{expected:08x}")
                if not candidate_values:
                    failures.append(f"{name}: alias 0x{key:08x} is absent from candidate table")
                elif len(set(candidate_values)) != 1:
                    failures.append(f"{name}: candidate alias 0x{key:08x} is duplicated with conflicting addresses")
                elif sdk_values and candidate_values[0] != sdk_values[0]:
                    failures.append(f"{name}: candidate alias 0x{key:08x} is 0x{candidate_values[0]:08x}, SDK has 0x{sdk_values[0]:08x}")
                if sdk_values and candidate_values and len(set(sdk_values)) == len(set(candidate_values)) == 1 and sdk_values[0] == candidate_values[0] == expected:
                    checked.append(name)
            elif name in fixed:
                fixed_used.append(name)
            elif is_toolchain_symbol(name):
                toolchain.append(name)
            else:
                unclassified.append(name)

        print(f"Candidate: {args.candidate} ({candidate_count} alias entries, {len(candidate)} unique; FW ID 0x{candidate_fw_id:08x})")
        print(f"SDK:       {args.sdk} ({sdk_count} alias entries, {len(sdk)} unique; FW ID 0x{sdk_fw_id:08x})")
        print(f"Objects checked: {len(args.objects)}")
        sdk_only = sorted(set(sdk) - set(candidate) - required_aliases)
        candidate_only = sorted(set(candidate) - set(sdk) - required_aliases)
        all_keys = set(sdk) | set(candidate)
        duplicate_unused = sorted(key for key in all_keys if key not in required_aliases and (len(set(sdk.get(key, []))) > 1 or len(set(candidate.get(key, []))) > 1))
        print(f"Unreferenced table differences: {len(sdk_only)} SDK-only alias(es), {len(candidate_only)} candidate-only alias(es)")
        if duplicate_unused:
            print("Unreferenced duplicate aliases with conflicting values (not used by these objects): " + ", ".join(f"0x{k:08x}" for k in duplicate_unused))
        print(f"Versioned firmware imports verified in both tables: {len(checked)}")
        for name in checked:
            print(f"  OK {name}")
        print(f"Fixed BipOS imports (not in versioned relocation table): {len(fixed_used)}")
        for name in fixed_used:
            print(f"  FIXED {name}")
        print(f"Toolchain/libm symbols (not watch firmware imports): {len(toolchain)}")
        for name in toolchain:
            print(f"  TOOLCHAIN {name}")
        print(f"Unclassified unresolved symbols: {len(unclassified)}")
        for name in unclassified:
            print(f"  UNCLASSIFIED {name}")
        if unclassified:
            failures.append("unclassified unresolved symbols require review")
        if failures:
            print("FAIL: candidate does not pass the used-import comparison:", file=sys.stderr)
            for failure in failures:
                print(f"  - {failure}", file=sys.stderr)
            return 1
        print("PASS: all versioned firmware imports used by these objects match the SDK import addresses in the candidate table.")
        print("Scope: source/build import compatibility only; this does not prove runtime behavior or firmware installation safety.")
        return 0
    except (CheckError, OSError, ValueError, struct.error) as error:
        print(f"check-imports: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
