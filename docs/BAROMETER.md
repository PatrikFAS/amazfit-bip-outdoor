# Barometer and altimeter app

The app reads pressure through the combined GPS and pressure sensor switch, polls every two seconds, and disables that sensor service when it exits. The vendor `navi-demo` checks `IS_NAVI_PRESS_READY` before using `navi_data.pressure`, whose unit is Pascals; this app applies the same readiness bit and converts Pa to hPa by dividing by 100. Values outside 300–1200 hPa are treated as unavailable.

Altitude is calculated from pressure using the standard barometric formula and a user adjustable reference pressure. The default reference is 1013.25 hPa and the screen marks the result `STANDARD REF`. Until calibrated against a known elevation or local pressure setting, displayed altitude is uncalibrated. Changes in weather pressure can therefore change the displayed altitude even when the watch has not moved. The app does not present GPS altitude.

Tap the lower left or right control to adjust reference pressure by 1 hPa. A versioned settings record stores the reference in centi-hPa with CRC16 validation and a write/readback check. Invalid or absent settings fall back to the standard reference.

The local host test validates pressure readiness and range handling, the pressure conversion and altitude calculation, settings persistence, and sensor shutdown. It cannot validate actual watch pressure availability, sensor freshness, firmware scheduling, display font fit, storage durability, or the on-watch calibration behavior. Those require validation on an original Amazfit Bip.
