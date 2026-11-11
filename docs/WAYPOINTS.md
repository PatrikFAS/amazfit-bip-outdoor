# Navigator waypoint collection

The six slots are HOME, CAR, CAMP, BOAT, POINT 1 and POINT 2. These are preset labels; there is no on-watch keyboard or phone synchronisation in this build.

- **SAVE A POINT** appears before your first save. After that, **NEW POINT** opens the first empty slot. Wait for **FIX READY**, then tap **SAVE HERE**. The first saved point becomes the active target. If all six slots are full, browse to the one you want to replace.
- **WAYPOINTS** opens the selected slot. Use **PREV** and **NEXT** to browse, or swipe left for the next slot. **SELECT** chooses the displayed point and returns to navigation. **BACK** or a right swipe returns without changing the target.
- **REPLACE** and **DELETE** need a second tap on the same button before the prompt expires. Changing slots, leaving the browser or using another action cancels confirmation.
- Tap **GPS ON/OFF** to control GPS. Tap **M/KM** or **M/NM** to change units. Both modes show metres below 1,000 m. The next waypoint write retains the units choice. The side button exits the app and requests sensor shutdown.

## Storage and compatibility

`src/waypoints.c` stores a 114-byte collection: BIPC magic, format version, selected slot, occupied mask, units, six existing 17-byte BIPW coordinate records, and outer CRC32. Unoccupied slots have zero bytes. Decoding validates lengths, fields, CRCs and coordinates before changing the caller's state. Legacy single-waypoint BIPW records migrate into HOME; 114-byte zero-padded legacy records are also accepted.

Writes use the BipOS settings API and require exact byte readback and successful decode before the UI confirms success. Failed save/select/delete leaves the running collection unchanged. This is corruption detection and verified writing, not an atomic transaction across power loss. No raw flash addresses are used.

A RES replacement may reset settings before this migration can run. Record any valuable coordinates separately before installing. The preceding single-waypoint build was confirmed to survive a watch restart; the new collection requires another hardware test. The host harness tests storage failures, migration, slot changes and drawing calls, but cannot establish physical flash behaviour or BipOS loader allocation.

## Watch validation

1. Open Navigator outside and wait for **FIX READY**. Tap **SAVE A POINT**, then **SAVE HERE** to mark the trailhead. Walk a short known distance and stop for a minute. Note whether you see **FIX UNCHANGED** or **WAITING FOR FIX**, and whether readings return as you start walking.
2. Walk back using glances at the large distance and numeric true bearing. Within a displayed 10 m, **NEAR TARGET** replaces the bearing. The GPS estimate can still move around near the point. Save your second location and switch between the two targets.
3. Check that the controls and status text are easy to read in daylight. Try cancelling a replace or delete prompt by changing slots.
4. Exit, restart the watch, and verify both points and the selected target. Confirm that the app still stays open during active GPS and exits with the side button.
