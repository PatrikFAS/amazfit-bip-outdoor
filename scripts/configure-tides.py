#!/usr/bin/env python3
"""Build an embedded tide table from an optional verified local CHS snapshot."""
import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def write_empty(output_dir):
    """Remove any old predictions and record that this build has none."""
    (output_dir / "point_atkinson.bipt").unlink(missing_ok=True)
    (output_dir / "point_atkinson_tides.h").write_text(
        "/* No tide snapshot selected; the app uses its solar-only path. */\n"
        "#ifndef BIP_POINT_ATKINSON_TIDES_H\n"
        "#define BIP_POINT_ATKINSON_TIDES_H\n"
        "#define TIDES_CONFIG_DATA_SIZE 0u\n"
        "#endif\n"
    )
    manifest = {
        "station_code": "07795",
        "station": "Point Atkinson",
        "event_count": 0,
        "snapshot": None,
        "expires_utc": None,
    }
    (output_dir / "tide_snapshot_manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n"
    )
    print("No tide snapshot selected; configured solar-only build (0 tide events)")


def configure_snapshot(snapshot, output_dir):
    for line in (snapshot / "SHA256SUMS").read_text().splitlines():
        expected, name = line.split("  ", 1)
        if Path(name).name != name:
            raise ValueError("snapshot checksum entry must be a filename")
        if hashlib.sha256((snapshot / name).read_bytes()).hexdigest() != expected:
            raise ValueError(f"snapshot checksum mismatch: {name}")
    metadata = json.loads((snapshot / "manifest.json").read_text())
    if metadata["station_code"] != "07795" or metadata["time_units"] != "UTC":
        raise ValueError("this build is configured for Point Atkinson UTC predictions")
    if metadata["datum"] != "Chart datum" or metadata["height_units"] != "metres":
        raise ValueError("unexpected prediction height reference")
    if not 0 < metadata["event_count"] <= 64:
        raise ValueError("snapshot must contain 1..64 tide events")
    output_dir.mkdir(parents=True, exist_ok=True)
    payload = output_dir / "point_atkinson.bipt"

    def epoch(value):
        date = dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
        if date.tzinfo is None or date.utcoffset() != dt.timedelta(0):
            raise ValueError("snapshot dates must be UTC")
        return str(int(date.timestamp()))

    subprocess.run([
        sys.executable, str(ROOT / "scripts/import-tides.py"),
        str(snapshot / "predictions.csv"), str(payload),
        "--station", "Point Atkinson", "--latitude", str(metadata["latitude"]),
        "--longitude", str(metadata["longitude"]), "--utc-offset-minutes", "-420",
        "--source", "CHS/IWLS", "--note", "Chart datum; UTC-07",
        "--generated-utc", epoch(metadata["generated_utc"]),
        "--expires-utc", epoch(metadata["expires_utc"]),
    ], check=True)
    (output_dir / "tide_snapshot_manifest.json").write_text(
        json.dumps(metadata, indent=2) + "\n"
    )
    data = payload.read_bytes()
    if data[5] != metadata["event_count"]:
        raise ValueError("event count differs from snapshot manifest")
    lines = ["/* Generated from a checksummed CHS snapshot; run make tide-data. */",
             "#ifndef BIP_POINT_ATKINSON_TIDES_H", "#define BIP_POINT_ATKINSON_TIDES_H",
             "static const unsigned char tides_config_data[] = {"]
    for index in range(0, len(data), 16):
        lines.append("    " + ", ".join(f"0x{value:02x}" for value in data[index:index + 16]) + ",")
    lines += ["};", f"#define TIDES_CONFIG_DATA_SIZE {len(data)}u", "#endif", ""]
    (output_dir / "point_atkinson_tides.h").write_text("\n".join(lines))
    print(f"Embedded {metadata['event_count']} CHS events ({len(data)} bytes), expiry {metadata['expires_utc']}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--snapshot", type=Path,
                        help="verified CHS snapshot directory (or set BIP_TIDE_SNAPSHOT)")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "build")
    args = parser.parse_args()
    snapshot = args.snapshot
    if snapshot is None and os.environ.get("BIP_TIDE_SNAPSHOT"):
        snapshot = Path(os.environ["BIP_TIDE_SNAPSHOT"])
    args.output_dir.mkdir(parents=True, exist_ok=True)
    if snapshot is None:
        write_empty(args.output_dir)
    else:
        configure_snapshot(snapshot, args.output_dir)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"configure-tides: {error}")
