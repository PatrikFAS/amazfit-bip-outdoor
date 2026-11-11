#!/usr/bin/env python3
"""Fetch CHS high/low predictions, or validate saved responses, into a dated CSV.

No device access. UTC input/output avoids local DST conversion. Event kinds are
classified only where both neighbouring CHS extrema agree on high versus low.
"""
import argparse
import csv
import datetime as dt
import hashlib
import json
import math
from pathlib import Path
import urllib.parse
import urllib.request

BASE = "https://api-iwls.dfo-mpo.gc.ca/api/v1"
UTC = dt.timezone.utc


def timestamp(value):
    date = dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
    if date.tzinfo is None or date.utcoffset() != dt.timedelta(0):
        raise ValueError("CHS eventDate must be explicitly UTC")
    return date


def request_json(path):
    request = urllib.request.Request(BASE + path, headers={"User-Agent": "BipOutdoor/0.2"})
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def classify_events(rows, start, end, series_id):
    if not isinstance(rows, list) or len(rows) < 3:
        raise ValueError("CHS response needs neighbouring high/low predictions")
    parsed = []
    for row in rows:
        date = timestamp(row["eventDate"])
        value = float(row["value"])
        if row.get("timeSeriesId") != series_id or str(row.get("qcFlagCode")) != "1":
            raise ValueError("wrong prediction series or unsupported quality flag")
        if not math.isfinite(value) or not -20 <= value <= 30:
            raise ValueError("implausible tide height")
        if parsed and date <= parsed[-1][0]:
            raise ValueError("CHS event dates must be strictly increasing")
        parsed.append((date, value))
    result = []
    for index, (date, height) in enumerate(parsed):
        if not start <= date < end:
            continue
        if index == 0 or index == len(parsed) - 1:
            raise ValueError("prediction window lacks neighbouring extrema")
        before, after = parsed[index - 1][1], parsed[index + 1][1]
        if height > before and height > after:
            kind = "high"
        elif height < before and height < after:
            kind = "low"
        else:
            raise ValueError("ambiguous high/low classification; no events fabricated")
        result.append((date.isoformat().replace("+00:00", "Z"), kind, height))
    if not result or len(result) > 64:
        raise ValueError("need 1..64 events; shorten prediction window if necessary")
    if any(result[i][1] == result[i + 1][1] for i in range(len(result) - 1)):
        raise ValueError("predicted extrema do not alternate")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="new snapshot directory (never overwritten)")
    parser.add_argument("--from-date", required=True, type=dt.date.fromisoformat)
    parser.add_argument("--to-date", required=True, type=dt.date.fromisoformat, help="exclusive UTC date")
    parser.add_argument("--station-code", default="07795")
    parser.add_argument("--metadata-json", type=Path, help="offline station metadata response")
    parser.add_argument("--events-json", type=Path, help="offline padded wlp-hilo response")
    args = parser.parse_args()
    start = dt.datetime.combine(args.from_date, dt.time(), UTC)
    end = dt.datetime.combine(args.to_date, dt.time(), UTC)
    if not 0 < (end - start).days <= 16:
        raise ValueError("choose a 1..16 day UTC window")
    if bool(args.metadata_json) != bool(args.events_json):
        raise ValueError("both offline JSON responses are required together")
    if args.metadata_json:
        metadata = json.loads(args.metadata_json.read_text())
        events = json.loads(args.events_json.read_text())
    else:
        stations = request_json("/stations?" + urllib.parse.urlencode({"code": args.station_code}))
        if len(stations) != 1 or stations[0].get("code") != args.station_code:
            raise ValueError("station lookup is ambiguous")
        station_id = stations[0]["id"]
        metadata = request_json(f"/stations/{station_id}/metadata")
        params = {"time-series-code": "wlp-hilo",
                  "from": (start - dt.timedelta(days=1)).isoformat().replace("+00:00", "Z"),
                  "to": (end + dt.timedelta(days=1)).isoformat().replace("+00:00", "Z")}
        events = request_json(f"/stations/{station_id}/data?" + urllib.parse.urlencode(params))
    if metadata.get("code") != args.station_code or not metadata.get("isTidal"):
        raise ValueError("station metadata does not match requested tidal station")
    series = [s for s in metadata["timeSeries"] if s.get("code") == "wlp-hilo"]
    if len(series) != 1 or series[0].get("owner") != "CHS-SHC":
        raise ValueError("official CHS high/low prediction series missing")
    result = classify_events(events, start, end, series[0]["id"])
    generated = dt.datetime.now(UTC).replace(microsecond=0)
    manifest = {"station": metadata["officialName"], "station_code": args.station_code,
                "latitude": metadata["latitude"], "longitude": metadata["longitude"],
                "source": "CHS/IWLS wlp-hilo", "generated_utc": generated.isoformat(),
                "coverage_start_utc": start.isoformat(), "coverage_end_utc": end.isoformat(),
                "expires_utc": result[-1][0], "event_count": len(result),
                "datum": "Chart datum", "height_units": "metres",
                "time_units": "UTC", "station_metadata_url": BASE + f"/stations/{metadata['id']}/metadata",
                "data_series": series[0]["id"],
                "classification": "Each CHS wlp-hilo event compared with both adjacent extrema",
                "datum_reference": "https://www.tides.gc.ca/en/water-level-application-optimized-mobile-devices"}
    args.output.mkdir(parents=True, exist_ok=False)
    with (args.output / "predictions.csv").open("x", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(["timestamp_utc", "kind", "height_m"])
        writer.writerows(result)
    for name, data in (("station.json", metadata), ("events-padded.json", events), ("manifest.json", manifest)):
        (args.output / name).write_text(json.dumps(data, indent=2) + "\n")
    files = sorted(args.output.iterdir())
    (args.output / "SHA256SUMS").write_text("".join(
        hashlib.sha256(p.read_bytes()).hexdigest() + "  " + p.name + "\n" for p in files))
    print(f"Saved {len(result)} official high/low predictions to {args.output}; all event times UTC")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, OSError) as error:
        raise SystemExit(f"fetch-chs-tides: {error}")
