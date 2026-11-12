# Compass and GPS course testing

The user has confirmed GPS acquisition, waypoint save/apparent return guidance, staying open and waypoint survival after watch restart. Open **Diagnostics → Compass** to check the firmware compass reading.

## What it does

Compass Check initializes the firmware compass task and enables compass sensor type 3 through the matching SDK imports, then polls raw heading and calibration status once per second. It does not enable GPS or read/modify Navigator waypoints. Return to Diagnostics or press the side button to disable the compass sensor. It uses the same periodic display-state refresh that kept Navigator open in the user's test, without forcing the backlight on.

Values outside 0–359 are suppressed. When calibration is requested, no degree is presented as a usable reading. The user confirmed the reading tracked a quarter turn and returned to its starting heading after calibration. **REF UNVERIFIED** describes the raw heading's north reference. With variation Off, Navigator uses the firmware heading directly and the red **N** represents that reference. If the firmware reports magnetic north, enter your local correction under **Waypoints → SET**. East is added and west subtracted from the compass heading. The corrected heading orients all three edge markers. [NOAA declination calculator](https://www.ngdc.noaa.gov/geomag/calculators/magcalc.shtml?model=wmm) The number labelled **BRG … T** is always the GPS-derived true bearing to the waypoint.

## Basis in existing code

The archived GoHome binary has `compassEnable` at ELF address 0x814c: it calls its compass-task wrapper, then its sensor wrapper with r0=3, r1=1. `compassDisable` at 0x8168 sends r0=3, r1=0. Its fixed Latin 1.1.6.48 pointers match that SDK's `compass_task_resume`, `sensors_enable`, `compass_get_degree` and `compass_need_calibration` declarations/imports. We reuse the call sequence through named SDK functions, not those incompatible fixed addresses.

The current English build-7 relocation table was checked against the matching non-Latin 1.1.2.05 SDK:

| Function | Alias | Thumb address |
| --- | --- | --- |
| compass_get_degree | FACE00CC | 08039AF5 |
| compass_need_calibration | FACE00CE | 08039D1D |
| compass_task_resume | FACE00C0 | 0803A405 |
| sensors_enable | FACE00C2 | 0805993D |

These comparisons establish import/call-sequence evidence, not execution on the physical watch. No hardcoded firmware address is used in the new app and no GoHome source was copied.

## Watch check

1. Before replacing resources, record any important saved waypoint coordinates; a RES update may reset settings even though ordinary reboot persistence is confirmed.
2. Install only the new RES on the working English BipOS base. No FW/font change is needed.
3. Open **Diagnostics → Compass**. If it requests calibration, exit, open the watch's original Compass app and follow its calibration prompts, then reopen Compass Check.
4. Turn the watch roughly a quarter turn while holding it level. Record whether the degree changes by roughly 90, remains stuck, reports unavailable, or requests calibration. Compare with the original Compass under similar conditions. The degree need not match true-north GPS bearing before the reference is established.
5. Leave it through the old inactivity timeout, then exit with the button and check ordinary watch operation.

## Navigator edge markers

1. Select a known waypoint outdoors and wait for a fix. The distance and numeric bearing stay visible on the main screen.
2. Hold the watch level and turn in place. Red **N** and the green waypoint arrow should move around the edge with your turn. If the firmware needs calibration, follow the original Compass app's calibration prompts and try again.
3. Walk 15–25 m in a straight line with a clear view of the sky. The yellow arrow should appear once accepted GPS fixes show about 15 m of displacement. Compare it with your actual direction of travel. When you walk toward the target, yellow and green should point to the same part of the edge.
4. Stop walking. Yellow should clear after about 15 seconds without enough new displacement. Check that red and green still turn with the watch while the fix remains usable.
5. Open Waypoints and return to the main screen. Check that the compass markers resume and GPS stays acquired. Press the side button to exit.

6. Walk away from the waypoint. Yellow should follow your direction of travel while green continues toward the waypoint. The two arrows should tilt diagonally when their directions lie between the cardinal directions.
7. In **WAYPOINTS → SET**, check that **OFF** is the initial value. Try a known correction, save it, and verify that all three markers rotate consistently while the numeric true bearing stays steady. Relaunch Navigator and check the saved value. Use **RESET OFF → SAVE** to clear the correction.
