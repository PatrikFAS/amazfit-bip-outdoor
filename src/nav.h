#ifndef BIP_OUTDOOR_NAV_H
#define BIP_OUTDOOR_NAV_H

#include <stddef.h>
#include <stdint.h>

/* Coordinates are decimal degrees; distances are metres unless noted. */
typedef struct {
    double latitude_deg;
    double longitude_deg;
} nav_coordinate_t;

typedef enum {
    NAV_UNITS_METRIC = 0,
    NAV_UNITS_NAUTICAL = 1
} nav_units_t;

/*
 * Fixes become stale after this duration. Supply a timestamp from the fix source,
 * not the time a cached fix was read. Unsigned subtraction handles one uint32
 * millisecond wrap; timestamps more than one full 2^32 ms period apart alias.
 */
#define NAV_MAX_FIX_AGE_MS UINT32_C(30000)

int nav_coordinate_valid(nav_coordinate_t coordinate);
/*
 * Build with NAV_SMALL_MATH=1 for the watch's float-only trig implementation.
 * It preserves the double API and E7 codec. Sampled route tests through 100 km
 * stay within 1 m and 0.1 degrees of the double implementation; bearings at
 * coincident/polar points are undefined and very long routes have lower
 * precision than the default implementation.
 */
int nav_distance_bearing(nav_coordinate_t from, nav_coordinate_t to,
                         double *distance_metres, double *bearing_deg);
/* At coincident coordinates the mathematically undefined bearing is returned as 0 degrees. */
/* Heading must be in [0, 360); result is signed in [-180, 180). */
int nav_relative_angle(double bearing_deg, double heading_deg,
                       double *relative_deg);
int nav_fix_is_fresh(uint32_t now_ms, uint32_t fix_time_ms);
int nav_convert_distance(double metres, nav_units_t units, double *converted);

/* Fixed 17-byte BIPW v1 record: magic, version, signed lat/lon E7 LE, CRC32 LE. */
#define NAV_WAYPOINT_RECORD_SIZE 17u
int nav_waypoint_encode(nav_coordinate_t coordinate,
                        uint8_t output[NAV_WAYPOINT_RECORD_SIZE]);
int nav_waypoint_decode(const uint8_t *record, size_t record_length,
                        nav_coordinate_t *coordinate);

#endif
