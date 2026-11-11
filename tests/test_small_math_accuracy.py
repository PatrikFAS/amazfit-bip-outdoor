#!/usr/bin/env python3
"""Compare the watch's float-math C path against the precise C implementation."""

import ctypes
import math
import pathlib
import random
import subprocess
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
RADIUS_M = 6371008.8
DISTANCES_M = (0.1, 1.0, 10.0, 100.0, 1000.0, 10000.0, 100000.0)
SAMPLES_PER_DISTANCE = 80


class Coordinate(ctypes.Structure):
    _fields_ = [("latitude_deg", ctypes.c_double), ("longitude_deg", ctypes.c_double)]


def build_library(output: pathlib.Path, small_math: bool) -> None:
    command = [
        "gcc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic",
        "-fPIC", "-shared", "-Isrc", "src/nav.c", "-lm", "-o", str(output),
    ]
    if small_math:
        command.insert(1, "-DNAV_SMALL_MATH=1")
    subprocess.run(command, cwd=ROOT, check=True)


def load_function(library_path: pathlib.Path):
    library = ctypes.CDLL(str(library_path))
    function = library.nav_distance_bearing
    function.argtypes = [
        Coordinate, Coordinate,
        ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double),
    ]
    function.restype = ctypes.c_int
    return function


def call(function, start, end):
    distance = ctypes.c_double()
    bearing = ctypes.c_double()
    ok = function(start, end, ctypes.byref(distance), ctypes.byref(bearing))
    assert ok == 1
    assert math.isfinite(distance.value) and 0.0 <= distance.value <= math.pi * RADIUS_M + 0.01
    assert math.isfinite(bearing.value) and 0.0 <= bearing.value < 360.0
    return distance.value, bearing.value


def destination(lat_deg, lon_deg, distance_m, bearing_deg):
    lat1 = math.radians(lat_deg)
    lon1 = math.radians(lon_deg)
    bearing = math.radians(bearing_deg)
    arc = distance_m / RADIUS_M
    lat2 = math.asin(
        math.sin(lat1) * math.cos(arc)
        + math.cos(lat1) * math.sin(arc) * math.cos(bearing)
    )
    lon2 = lon1 + math.atan2(
        math.sin(bearing) * math.sin(arc) * math.cos(lat1),
        math.cos(arc) - math.sin(lat1) * math.sin(lat2),
    )
    lon_deg = (math.degrees(lon2) + 180.0) % 360.0 - 180.0
    return Coordinate(math.degrees(lat2), lon_deg)


def bearing_delta(first, second):
    return abs((first - second + 180.0) % 360.0 - 180.0)


def main():
    rng = random.Random(0xB1B0_2026)
    with tempfile.TemporaryDirectory(prefix="bip-nav-accuracy-") as temp_name:
        temp = pathlib.Path(temp_name)
        precise_path = temp / "nav_precise.so"
        small_path = temp / "nav_small.so"
        build_library(precise_path, False)
        build_library(small_path, True)
        precise = load_function(precise_path)
        small = load_function(small_path)

        max_distance_error = 0.0
        max_bearing_error = 0.0
        checked = 0
        for distance_m in DISTANCES_M:
            for _ in range(SAMPLES_PER_DISTANCE):
                start = Coordinate(rng.uniform(-85.0, 85.0), rng.uniform(-180.0, 180.0))
                initial_bearing = rng.uniform(0.0, 360.0)
                end = destination(start.latitude_deg, start.longitude_deg,
                                  distance_m, initial_bearing)
                precise_distance, precise_bearing = call(precise, start, end)
                small_distance, small_bearing = call(small, start, end)
                distance_error = abs(small_distance - precise_distance)
                max_distance_error = max(max_distance_error, distance_error)
                if distance_m >= 0.1 and abs(start.latitude_deg) < 89.999999 and abs(end.latitude_deg) < 89.999999:
                    bearing_error = bearing_delta(small_bearing, precise_bearing)
                    max_bearing_error = max(max_bearing_error, bearing_error)
                    assert bearing_error <= 0.1, (
                        f"bearing error {bearing_error:.6f} deg at {distance_m:g} m; "
                        f"start={start.latitude_deg:.8f},{start.longitude_deg:.8f} "
                        f"end={end.latitude_deg:.8f},{end.longitude_deg:.8f}"
                    )
                if distance_m <= 100000.0:
                    assert distance_error <= 1.0, (
                        f"distance error {distance_error:.6f} m at {distance_m:g} m; "
                        f"start={start.latitude_deg:.8f},{start.longitude_deg:.8f} "
                        f"end={end.latitude_deg:.8f},{end.longitude_deg:.8f}"
                    )
                checked += 1

        # Force one date-line crossing at every requested distance band.
        for distance_m in DISTANCES_M:
            latitude = rng.uniform(-85.0, 85.0)
            probe = destination(latitude, 0.0, distance_m, 90.0)
            start_lon = 180.0 - probe.longitude_deg / 2.0
            start = Coordinate(latitude, start_lon)
            end = destination(latitude, start_lon, distance_m, 90.0)
            assert start.longitude_deg > 0.0 and end.longitude_deg < 0.0, "expected eastbound date-line crossing"
            precise_distance, precise_bearing = call(precise, start, end)
            small_distance, small_bearing = call(small, start, end)
            distance_error = abs(small_distance - precise_distance)
            bearing_error = bearing_delta(small_bearing, precise_bearing)
            max_distance_error = max(max_distance_error, distance_error)
            max_bearing_error = max(max_bearing_error, bearing_error)
            assert distance_error <= 1.0, f"date-line distance error {distance_error:.6f} m at {distance_m:g} m"
            assert bearing_error <= 0.1, f"date-line bearing error {bearing_error:.6f} deg at {distance_m:g} m"
            checked += 1

        # Explicit long-range and near-antipodal probes are finite/range checked
        # above and reported separately; their errors are not hidden by the local bound.
        long_cases = [
            (Coordinate(0.0, 0.0), Coordinate(0.0, 180.0)),
            (Coordinate(-41.2865, 174.7762), Coordinate(40.9701, -5.6635)),
            (Coordinate(12.345, -45.678), Coordinate(-12.3449, 134.3219)),
            (Coordinate(84.9, 10.0), Coordinate(-84.8, -169.8)),
        ]
        long_max_distance_error = 0.0
        long_max_bearing_error = 0.0
        undefined_long_bearings = 0
        for start, end in long_cases:
            pd, pb = call(precise, start, end)
            sd, sb = call(small, start, end)
            long_max_distance_error = max(long_max_distance_error, abs(sd - pd))
            if math.pi * RADIUS_M - pd < 0.001:
                undefined_long_bearings += 1
            else:
                long_max_bearing_error = max(long_max_bearing_error, bearing_delta(sb, pb))
        assert long_max_distance_error <= 2.0, (
            f"unexpected long-range distance divergence: {long_max_distance_error:.6f} m"
        )

        print(f"PASS: compared {checked} generated routes over {len(DISTANCES_M)} distance bands")
        print(f"MAX sampled <=100 km error: {max_distance_error:.6f} m, {max_bearing_error:.6f} deg")
        print(f"MAX explicit long/near-antipodal error: {long_max_distance_error:.6f} m, "
              f"{long_max_bearing_error:.6f} deg ({undefined_long_bearings} undefined antipodal bearing skipped)")


if __name__ == "__main__":
    main()
