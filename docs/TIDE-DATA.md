# Tidal and solar data

The combined screen computes sun events locally from the entered Gregorian date, latitude, longitude, and an explicit fixed UTC offset. Sunrise/sunset use a 90.833° zenith (solar disk and standard refraction), civil twilight uses 96°, and nautical twilight uses 102°. Times are rounded local clock minutes. Dates before 1583 are rejected; leap years follow Gregorian rules. The compact NOAA-style approximation uses fractional year, equation of time, declination, and the sunrise hour-angle relation. It is intended for outdoor planning, not precision astronomical work. A date without sunrise returns `OUTDOOR_SOLAR_NO_RISE_SET`; if a twilight threshold has no crossing, its dawn/dusk minute is `-1`.

Equations are derived from NOAA Global Monitoring Laboratory's [solar calculation details](https://www.gml.noaa.gov/grad/solcalc/solareqns.PDF) and [sunrise/sunset calculation](https://gml.noaa.gov/grad/solcalc/sunrise.html). No NOAA code is copied.

## Tide CSV import

`scripts/import-tides.py INPUT.csv OUTPUT.bipt` converts supplied predictions offline. Required metadata: `--station`, `--latitude`, `--longitude`, `--utc-offset-minutes`, `--source`, `--generated-utc`, and `--expires-utc`; `--note` is optional. CSV columns are `timestamp_utc,kind,height_m`. Timestamps must be ISO 8601 with `Z` or `+00:00`; kind is `high` or `low`. Rows must be strictly chronological, no later than expiry, and there may be at most 64. Heights are rounded to signed centimetres. The generated timestamp records when the bundle was assembled and may be later than the first event in the supplied prediction window. Import errors fail closed. The importer does not fetch or scrape predictions and does not invent them.

The file is explicit little-endian, with no native C structure casts:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | ASCII `BIPT` |
| 4 | 1 | version 1 |
| 5 | 1 | event count (0..64) |
| 6 | 2 | reserved zero bytes |
| 8 | 4 | generated UTC Unix seconds |
| 12 | 4 | expiry UTC Unix seconds |
| 16 | 4 | latitude signed E7 |
| 20 | 4 | longitude signed E7 |
| 24 | 2 | fixed UTC offset signed minutes |
| 26..28 | 1 each | station/source/note byte lengths |
| 29..31 | 3 | reserved zero bytes |
| 32 | 32 | printable ASCII station label, zero padded |
| 64 | 32 | printable ASCII source, zero padded |
| 96 | 48 | printable ASCII timezone/datum note, zero padded |
| 144 | 8 each | event: UTC u32, height signed cm i16, kind (1 high/2 low), reserved zero |
| end-4 | 4 | IEEE CRC-32 of every preceding byte, little-endian |

Exact file length is `148 + 8*event_count`. The decoder rejects bad version, length, CRC, bounds, nonchronological or malformed events and invalid metadata. A valid file with zero events returns EMPTY; a valid file after expiry returns EXPIRED. The screen should show an unavailable state for either, never fabricate a prediction. Include the fixed offset and prediction datum in the note (for example `BC UTC-420 permanent; datum CD; no DST`).

For Point Atkinson (CHS07795), use the officially supplied station coordinates 49.3375, -123.253583 and metadata provided with the prediction export. Configure UTC offset explicitly; this format does not model daylight-saving transitions. For the current Point Atkinson/BC permanent UTC-07 rule, use -420 minutes, as verified by the parent task; the importer itself deliberately does not infer timezone policy. Never use an unverified height datum or silently assume tide predictions are current.

## Optional Point Atkinson tide data

The public repository does not include a prediction snapshot. With no snapshot selected, `make tide-data` removes any old generated tide bundle, writes a manifest with `event_count: 0`, and configures the solar-only build. The app's station is pinned to CHS station `07795` (Point Atkinson), coordinates 49.3375, -123.253583, and fixed UTC-07 offset (-420 minutes). Those are the same coordinates and offset used by the tide screen. Daylight-saving changes are not modeled.

To opt in, fetch a new snapshot for Point Atkinson. Use UTC dates, a 1–16 day range, and an exclusive end date; the fetcher requests one extra day on both sides to classify extrema. Choose a fresh, unused directory because snapshots are never overwritten:

```sh
python3 scripts/fetch-chs-tides.py data/point-atkinson-YYYYMMDD \
  --station-code 07795 --from-date YYYY-MM-DD --to-date YYYY-MM-DD
BIP_TIDE_SNAPSHOT="$PWD/data/point-atkinson-YYYYMMDD" make check check-imports suite
BIP_TIDE_SNAPSHOT="$PWD/data/point-atkinson-YYYYMMDD" ./scripts/build.sh
```

The fetcher checks the CHS tidal station and official high/low prediction series. It rejects bad quality flags, ambiguous extrema, missing neighbours, unsorted times, wrong station metadata, or more than 64 events. It does not interpolate a tide curve or invent predictions. The build verifies snapshot checksums, station, UTC, chart datum, and metre units before embedding predictions. The bundle expires at the final supplied event in `manifest.json`; rebuild with a refreshed snapshot before then if predictions are needed beyond that time. The decoder treats expired data as unavailable. The app continues to calculate sun and twilight without tide data.

`make check` runs the synthetic tide decoder/import coverage and remains runnable without `data/`. Select a snapshot through `BIP_TIDE_SNAPSHOT` for `make check` and `scripts/build.sh`; tide data is never selected from a local directory implicitly.

Height reference: [CHS water-level application](https://www.tides.gc.ca/en/water-level-application-optimized-mobile-devices). Tide API: [CHS web services](https://www.tides.gc.ca/en/web-services-offered-canadian-hydrographic-service). Fixed UTC-07 policy: [B.C. government](https://www2.gov.bc.ca/gov/content/governments/celebrating-british-columbia/daylight-saving-time). Solar reference: [USNO one-day service](https://aa.usno.navy.mil/api/rstt/oneday?date=2026-11-08&coords=49.3375,-123.253583&tz=-7).
