# Bip Outdoor

Offline outdoor apps for the **original Amazfit Bip (A1608)** running **BipOS 0.5.5 build 7 on English non-Latin firmware 1.1.2.05**.

- **Navigator** saves your current location or accepts coordinates. Swipe between the navigation display, compass, and legend. Set magnetic variation from the Waypoints menu.
- **Tides & Sun** calculates sun and twilight times and can display a locally supplied tide table. Swipe between pages.
- **Barometer** displays pressure and pressure-derived altitude.
- **Diagnostics** includes GPS, compass, and watch information.
- **Flashlight** comes from the archived MNVolkov app release.

## Install with Gadgetbridge

[Download the app kit](downloads/bip-outdoor-app-kit.zip) and extract it. Open a terminal in the extracted folder and run `python3 assemble-res.py` on macOS or Linux, or `py -3 assemble-res.py` on Windows. The kit contains the compiled apps. Python downloads the matching upstream files, verifies their checksums, and writes `bip-outdoor-suite.res`.

Transfer the generated RES to your Android phone and install it using Gadgetbridge. Follow the [installation guide](docs/INSTALL-SUITE.md) for compatibility, transfer steps, and checks.

The download includes sun and twilight calculations for Point Atkinson, BC. To add Point Atkinson tide predictions, create a local snapshot using the [tide data guide](docs/TIDE-DATA.md) and build from source.

## Build from source

On x86-64 Linux or WSL with GNU Make, a C compiler, Python 3, Git, curl, and Node.js 22.15 or newer with npm, run these commands from the repository root.

```sh
make bootstrap package-tools candidate-tools
make check check-imports
make suite
```

The resource update is written to `build/bip-outdoor-suite/bip-outdoor-suite.res`. The bootstrap scripts pin the ARM toolchain, libbip, and the upstream packaging inputs. [Tide data setup](docs/TIDE-DATA.md) describes the optional snapshot environment variable.

## License and credits

Bip Outdoor is licensed under [Apache 2.0](LICENSE). It links [libbip by Maxim Volkov](docs/LICENSE.libbip) and uses tools and archived components maintained by ngxson. See the [upstream notices](NOTICE.md).
