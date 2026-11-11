#!/usr/bin/env python3
"""Build an embedded offline tide table from a verified local CHS snapshot."""
import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--snapshot", type=Path, default=os.environ.get("BIP_TIDE_SNAPSHOT"))
    parser.add_argument("--output-dir", type=Path, default=ROOT / "build")
    args = parser.parse_args()
    if args.snapshot is None:
        args.output_dir.mkdir(parents=True, exist_ok=True)
        (args.output_dir / "point_atkinson_tides.h").unlink(missing_ok=True)
        (args.output_dir / "point_atkinson.bipt").unlink(missing_ok=True)
        (args.output_dir / "tide_snapshot_manifest.json").write_text(
            json.dumps({"event_count": 0, "source": None}, indent=2) + "\n")
        print("No tide snapshot selected. Solar calculations remain available.")
        return
    for line in (args.snapshot / "SHA256SUMS").read_text().splitlines():
        expected, name = line.split("  ", 1)
        if Path(name).name != name:
            raise ValueError("snapshot checksum entry must be a filename")
        if hashlib.sha256((args.snapshot / name).read_bytes()).hexdigest() != expected:
            raise ValueError(f"snapshot checksum mismatch: {name}")
    metadata = json.loads((args.snapshot / "manifest.json").read_text())
    if metadata["station_code"] != "07795" or metadata["time_units"] != "UTC":
        raise ValueError("this build is configured for Point Atkinson UTC predictions")
    if metadata["datum"] != "Chart datum" or metadata["height_units"] != "metres":
        raise ValueError("unexpected prediction height reference")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    payload = args.output_dir / "point_atkinson.bipt"
    def epoch(value):
        date = dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
        if date.tzinfo is None or date.utcoffset() != dt.timedelta(0):
            raise ValueError("snapshot dates must be UTC")
        return str(int(date.timestamp()))
    subprocess.run([
        sys.executable, str(ROOT / "scripts/import-tides.py"),
        str(args.snapshot / "predictions.csv"), str(payload),
        "--station", "Point Atkinson", "--latitude", str(metadata["latitude"]),
        "--longitude", str(metadata["longitude"]), "--utc-offset-minutes", "-420",
        "--source", "CHS/IWLS", "--note", "Chart datum; UTC-07",
        "--generated-utc", epoch(metadata["generated_utc"]),
        "--expires-utc", epoch(metadata["expires_utc"]),
    ], check=True)
    (args.output_dir / "tide_snapshot_manifest.json").write_text(json.dumps(metadata, indent=2) + "\n")
    data = payload.read_bytes()
    if data[5] != metadata["event_count"]:
        raise ValueError("event count differs from snapshot manifest")
    lines = ["/* Generated from a checksummed CHS snapshot; run make tide-data. */",
             "#ifndef BIP_POINT_ATKINSON_TIDES_H", "#define BIP_POINT_ATKINSON_TIDES_H",
             "static const unsigned char tides_config_data[] = {"]
    for index in range(0, len(data), 16):
        lines.append("    " + ", ".join(f"0x{value:02x}" for value in data[index:index + 16]) + ",")
    lines += ["};", f"#define TIDES_CONFIG_DATA_SIZE {len(data)}u", "#endif", ""]
    (args.output_dir / "point_atkinson_tides.h").write_text("\n".join(lines))
    print(f"Embedded {metadata['event_count']} CHS events ({len(data)} bytes), expiry {metadata['expires_utc']}")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"configure-tides: {error}")
