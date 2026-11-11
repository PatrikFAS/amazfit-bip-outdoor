# Compass Check

The app displays raw compass readings for hardware testing. Heading accuracy and the north reference still need verification on the watch.

## What it does

Compass Check initializes the firmware compass task and enables compass sensor type 3 through the matching SDK imports, then polls raw heading and calibration status once per second. It does not enable GPS or read/modify Navigator waypoints. Button/right-swipe exits, stops updates and disables the compass sensor. It uses the same periodic display-state refresh that kept Navigator open in the user's test, without forcing the backlight on.

Values outside 0–359 are suppressed. When calibration is requested, no degree is presented as a usable reading. A value within range is labelled RAW HEADING / REF UNVERIFIED: this does not establish source freshness, absolute accuracy, or magnetic-versus-true north. The navigation arrow remains disabled until these are verified.

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
3. Open **Compass Check**. If it requests calibration, exit, open the watch's original Compass app and follow its calibration prompts, then reopen Compass Check.
4. Turn the watch roughly a quarter turn while holding it level. Record whether the degree changes by roughly 90, remains stuck, reports unavailable, or requests calibration. Compare with the original Compass under similar conditions. The degree need not match true-north GPS bearing before the reference is established.
5. Leave it through the old inactivity timeout, then exit with the button and check ordinary watch operation.
