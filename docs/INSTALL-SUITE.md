# Install Bip Outdoor

Use this guide with the **original Amazfit Bip (A1608)** already running **BipOS 0.5.5 build 7 on English non-Latin firmware 1.1.2.05**. You'll need Python 3 on your computer, an Internet connection for the upstream downloads, and Gadgetbridge on the paired Android phone.

## Generate the resource file

1. [Download the app kit](../downloads/bip-outdoor-app-kit.zip) and extract it.
2. Open a terminal in the extracted folder. Run `python3 assemble-res.py` on macOS or Linux, or `py -3 assemble-res.py` on Windows.
3. The script checks the compiled apps against `SHA256SUMS`, downloads the checksum-pinned base resources, relocation data, and Flashlight app, and writes `bip-outdoor-suite.res` in the current folder. For offline assembly, pass `--upstream-dir` pointing to a local copy of the pinned upstream archive.

If you already [built from source](../README.md#build-from-source), use the RES from your build output and continue below.

## Transfer with Gadgetbridge

1. Confirm the watch model and BipOS version above. Save a copy of any waypoint coordinates you want to keep, since resource updates can reset app settings.
2. Exit BipOS apps on the watch, copy `bip-outdoor-suite.res` to your Android phone, and open the file using Gadgetbridge's **FW/App Installer**.
3. Confirm it is identified as a resource file, start the update, and wait for Gadgetbridge to finish.
4. Open the BipOS app launcher and check that the apps start. Try Navigator outdoors with a clear view of the sky. If Compass asks for calibration, calibrate the watch and check that the heading tracks when you turn it.

The kit includes solar events for Point Atkinson, BC. You can [build with a local CHS tide snapshot](TIDE-DATA.md) to add predictions and check its coverage in Tides & Sun.
