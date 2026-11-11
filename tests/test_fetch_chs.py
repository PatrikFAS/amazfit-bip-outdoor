import datetime as dt
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("chs", Path(__file__).resolve().parents[1] / "scripts/fetch-chs-tides.py")
chs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(chs)


class CHSTests(unittest.TestCase):
    def test_classifies_only_bounded_extrema(self):
        rows = [{"eventDate": f"2026-11-08T{hour:02d}:00:00Z", "value": value,
                 "qcFlagCode": "1", "timeSeriesId": "official"}
                for hour, value in ((0, 1), (6, 4), (12, 2), (18, 5))]
        start = chs.timestamp("2026-11-08T01:00:00Z")
        end = chs.timestamp("2026-11-08T17:00:00Z")
        result = chs.classify_events(rows, start, end, "official")
        self.assertEqual([r[1] for r in result], ["high", "low"])
        with self.assertRaises(ValueError):
            chs.classify_events(rows, chs.timestamp("2026-11-08T00:00:00Z"), end, "official")
        rows[1]["qcFlagCode"] = "3"
        with self.assertRaises(ValueError):
            chs.classify_events(rows, start, end, "official")

    def test_ambiguous_order_and_timezone_rejected(self):
        with self.assertRaises(ValueError):
            chs.timestamp("2026-11-08T01:00:00")
        rows = [{"eventDate": f"2026-11-08T{hour:02d}:00:00Z", "value": value,
                 "qcFlagCode": "1", "timeSeriesId": "official"}
                for hour, value in ((0, 1), (6, 2), (12, 3))]
        with self.assertRaises(ValueError):
            chs.classify_events(rows, chs.timestamp("2026-11-08T01:00:00Z"),
                                chs.timestamp("2026-11-08T08:00:00Z"), "official")


if __name__ == "__main__":
    unittest.main()
