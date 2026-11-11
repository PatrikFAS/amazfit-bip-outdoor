# Build a Bip Outdoor resource update

This guide is for an **original Amazfit Bip A1608** already running **BipOS 0.5.5 build 7 on English non-Latin 1.1.2.05**. See [BipOS sources](BIPOS-SOURCES.md) for the matching upstream distribution. Installing BipOS itself is a separate operation.

1. Run `make bootstrap package-tools candidate-tools` to fetch the pinned local build dependencies and matching upstream inputs. Review their applicable terms before using the assets.
2. Run `make check check-imports`, then `make suite`. The local output directory contains `bip-outdoor-suite.res` and `SHA256SUMS`. Keep the generated RES local unless you have permission to redistribute its bundled upstream content.
3. Record any important waypoint coordinates. A resource replacement may reset app settings. On Android, open the generated RES in Gadgetbridge's firmware/app installer. Check the model and resource type before sending it to the watch.
4. Open Navigator and follow the [short watch test](WAYPOINTS.md#watch-validation). Confirm launch, GPS, visibility outdoors, and waypoint persistence before relying on the new build.

The source distribution builds solar calculations with an empty tide table by default. For an optional local CHS snapshot, review the [CHS data terms](https://tides.gc.ca/en/licence-agreement) and see `python3 scripts/fetch-chs-tides.py --help`. Set `BIP_TIDE_SNAPSHOT` to the resulting directory when building and packaging.
