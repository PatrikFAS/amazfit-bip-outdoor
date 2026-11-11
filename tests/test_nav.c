#include "../src/nav.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int near(double a, double b, double epsilon)
{
    return fabs(a - b) <= epsilon;
}

int main(void)
{
    nav_coordinate_t a = {0.0, 0.0}, b = {0.0, 1.0}, decoded;
    double distance, bearing, angle, converted;
    uint8_t record[NAV_WAYPOINT_RECORD_SIZE], damaged[NAV_WAYPOINT_RECORD_SIZE];
    static const uint8_t zero_golden[NAV_WAYPOINT_RECORD_SIZE] = {
        0x42, 0x49, 0x50, 0x57, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x55, 0xbb, 0x62, 0x4b
    };
    size_t i;

    assert(nav_coordinate_valid(a));
    assert(!nav_coordinate_valid((nav_coordinate_t){NAN, 0.0}));
    assert(!nav_coordinate_valid((nav_coordinate_t){91.0, 0.0}));
    assert(!nav_coordinate_valid((nav_coordinate_t){0.0, INFINITY}));
    assert(nav_distance_bearing(a, b, &distance, &bearing));
    assert(near(distance, 111195.0802, 0.02) && near(bearing, 90.0, 1e-10));
    assert(nav_distance_bearing((nav_coordinate_t){0.0, 179.5},
                                (nav_coordinate_t){0.0, -179.5}, &distance, &bearing));
    assert(near(distance, 111195.0802, 0.02) && near(bearing, 90.0, 1e-8));
    assert(nav_distance_bearing((nav_coordinate_t){90.0, 0.0},
                                (nav_coordinate_t){90.0, 90.0}, &distance, &bearing));
    assert(distance < 1e-8);
    assert(nav_distance_bearing(a, a, &distance, &bearing));
    assert(distance == 0.0 && bearing == 0.0);
    assert(!nav_distance_bearing((nav_coordinate_t){NAN, 0.0}, a, &distance, &bearing));
    assert(nav_distance_bearing((nav_coordinate_t){-41.2865, 174.7762},
                                (nav_coordinate_t){40.9701, -5.6635},
                                &distance, &bearing));
    assert(near(distance, 19964182.65, 0.1));
    assert(nav_distance_bearing((nav_coordinate_t){-20.0, 30.0},
                                (nav_coordinate_t){-19.0, 31.0}, &distance, &bearing));
    assert(bearing > 0.0 && bearing < 90.0);
    assert(nav_distance_bearing((nav_coordinate_t){45.0, 10.0},
                                (nav_coordinate_t){45.07, 10.1}, &distance, &bearing));
    assert(near(distance, 11060.35515, 1.0));
    assert(near(bearing, 45.23650, 0.1));
    assert(nav_distance_bearing((nav_coordinate_t){0.0, 0.0},
                                (nav_coordinate_t){0.8993216, 0.0}, &distance, &bearing));
    assert(near(distance, 100000.13747, 1.0));
    assert(nav_distance_bearing((nav_coordinate_t){50.0, -120.0},
                                (nav_coordinate_t){50.000001, -120.0},
                                &distance, &bearing));
    assert(near(distance, 0.111195, 0.01));
    assert(isfinite(distance) && isfinite(bearing));
    assert(nav_distance_bearing((nav_coordinate_t){-90.0, -180.0},
                                (nav_coordinate_t){-90.0, 180.0},
                                &distance, &bearing));
    assert(distance == 0.0 && bearing == 0.0);
    assert(nav_distance_bearing((nav_coordinate_t){0.0, 0.0},
                                (nav_coordinate_t){0.0, 180.0}, &distance, &bearing));
    assert(isfinite(distance) && isfinite(bearing));

    assert(nav_relative_angle(10.0, 350.0, &angle) && near(angle, 20.0, 1e-12));
    assert(nav_relative_angle(0.0, 180.0, &angle) && near(angle, -180.0, 1e-12));
    assert(!nav_relative_angle(360.0, 0.0, &angle));
    assert(nav_fix_is_fresh(1000u, 1000u));
    assert(nav_fix_is_fresh(31000u, 1000u));
    assert(!nav_fix_is_fresh(31001u, 1000u));
    assert(nav_fix_is_fresh(10u, UINT32_C(0xfffffff0)));
    assert(nav_convert_distance(1852.0, NAV_UNITS_NAUTICAL, &converted));
    assert(near(converted, 1.0, 1e-12));
    assert(!nav_convert_distance(-1.0, NAV_UNITS_METRIC, &converted));

    a.latitude_deg = 49.2827; a.longitude_deg = -123.1207;
    assert(!nav_waypoint_encode(a, NULL));
    assert(!nav_waypoint_encode((nav_coordinate_t){NAN, 0.0}, record));
    assert(!nav_waypoint_decode(NULL, NAV_WAYPOINT_RECORD_SIZE, &decoded));
    assert(!nav_waypoint_decode(record, NAV_WAYPOINT_RECORD_SIZE, NULL));
    assert(nav_waypoint_encode(a, record));
    assert(nav_waypoint_decode(record, sizeof record, &decoded));
    assert(near(decoded.latitude_deg, a.latitude_deg, 1e-7));
    assert(near(decoded.longitude_deg, a.longitude_deg, 1e-7));
    assert(!nav_waypoint_decode(record, sizeof record - 1, &decoded));
    assert(!nav_waypoint_decode(record, sizeof record + 1, &decoded));
    for (i = 0; i < sizeof record; ++i) {
        memcpy(damaged, record, sizeof record);
        damaged[i] ^= 1u;
        assert(!nav_waypoint_decode(damaged, sizeof damaged, &decoded));
    }
    memcpy(damaged, record, sizeof record);
    damaged[5] = 0xff; damaged[6] = 0xff; damaged[7] = 0xff; damaged[8] = 0x7f;
    /* Recompute CRC to ensure the coordinate range check is exercised. */
    {
        uint32_t crc = UINT32_C(0xffffffff);
        unsigned i, bit;
        for (i = 0; i < 13; ++i) {
            crc ^= damaged[i];
            for (bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0u);
        }
        crc ^= UINT32_C(0xffffffff);
        damaged[13] = (uint8_t)crc; damaged[14] = (uint8_t)(crc >> 8);
        damaged[15] = (uint8_t)(crc >> 16); damaged[16] = (uint8_t)(crc >> 24);
    }
    assert(!nav_waypoint_decode(damaged, sizeof damaged, &decoded));
    assert(nav_waypoint_encode((nav_coordinate_t){0.0, 0.0}, record));
    assert(memcmp(record, zero_golden, sizeof record) == 0);
    assert(nav_waypoint_decode(zero_golden, sizeof zero_golden, &decoded));
    assert(decoded.latitude_deg == 0.0 && decoded.longitude_deg == 0.0);
    assert(nav_waypoint_encode((nav_coordinate_t){90.0, 180.0}, record));
    assert(nav_waypoint_decode(record, sizeof record, &decoded));
    assert(decoded.latitude_deg == 90.0 && decoded.longitude_deg == 180.0);
    assert(nav_waypoint_encode((nav_coordinate_t){-90.0, -180.0}, record));
    assert(nav_waypoint_decode(record, sizeof record, &decoded));
    assert(decoded.latitude_deg == -90.0 && decoded.longitude_deg == -180.0);
    assert(nav_waypoint_encode((nav_coordinate_t){-12.3456789, 145.1234567}, record));
    assert(nav_waypoint_decode(record, sizeof record, &decoded));
    assert(near(decoded.latitude_deg, -12.3456789, 1e-7));
    assert(near(decoded.longitude_deg, 145.1234567, 1e-7));
    puts("navigation tests passed");
    return 0;
}
