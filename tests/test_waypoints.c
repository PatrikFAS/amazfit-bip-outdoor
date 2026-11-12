#include "waypoints.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t crc32_bytes(const uint8_t *p, size_t n)
{
    uint32_t crc = UINT32_C(0xffffffff);
    size_t i;
    for (i = 0; i < n; ++i) {
        unsigned bit;
        crc ^= p[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0u);
    }
    return crc ^ UINT32_C(0xffffffff);
}

static void refresh_crc(uint8_t *p)
{
    uint32_t crc = crc32_bytes(p, WAYPOINTS_RECORD_SIZE - 4u);
    size_t at = WAYPOINTS_RECORD_SIZE - 4u;
    p[at] = (uint8_t)crc; p[at + 1] = (uint8_t)(crc >> 8);
    p[at + 2] = (uint8_t)(crc >> 16); p[at + 3] = (uint8_t)(crc >> 24);
}

static int near(double a, double b) { return a - b < 0.00000011 && b - a < 0.00000011; }

static void assert_decode_rejected_unchanged(const uint8_t *record, size_t length)
{
    waypoints_t output, before;
    memset(&output, 0xa5, sizeof(output));
    output.selected = 4;
    output.occupied_mask = 0x12;
    output.units = NAV_UNITS_NAUTICAL;
    memcpy(&before, &output, sizeof(before));
    assert(!waypoints_decode(record, length, &output));
    assert(memcmp(&output, &before, sizeof(output)) == 0);
}

int main(void)
{
    waypoints_t c, decoded;
    nav_coordinate_t point;
    uint8_t encoded[WAYPOINTS_RECORD_SIZE], legacy[WAYPOINTS_RECORD_SIZE];
    uint8_t damaged[WAYPOINTS_RECORD_SIZE];

    waypoints_init(&c);
    assert(c.occupied_mask == 0 && c.selected == 0 && c.units == NAV_UNITS_METRIC && c.variation == 0);
    assert(!waypoints_get_selected(&c, &point));
    assert(waypoints_encode(&c, encoded));
    assert(waypoints_decode(encoded, sizeof(encoded), &decoded));
    assert(decoded.occupied_mask == 0 && decoded.units == NAV_UNITS_METRIC && decoded.variation == 0);

    /* Variation is persisted in version 2 without changing the record size. */
    assert(WAYPOINTS_RECORD_SIZE == 114u);
    c.variation = -180;
    assert(waypoints_encode(&c, encoded));
    assert(encoded[4] == 2);
    assert(waypoints_decode(encoded, sizeof(encoded), &decoded) && decoded.variation == -180);
    c.variation = -1;
    assert(waypoints_encode(&c, encoded));
    assert(waypoints_decode(encoded, sizeof(encoded), &decoded) && decoded.variation == -1);
    c.variation = 1;
    assert(waypoints_encode(&c, encoded));
    assert(waypoints_decode(encoded, sizeof(encoded), &decoded) && decoded.variation == 1);
    c.variation = 180;
    assert(waypoints_encode(&c, encoded));
    assert(waypoints_decode(encoded, sizeof(encoded), &decoded) && decoded.variation == 180);
    c.variation = 0;

    /* Version 1 used only the low occupancy and units bits and defaults variation Off. */
    assert(waypoints_encode(&c, encoded));
    encoded[4] = 1;
    encoded[6] &= 0x3f;
    encoded[7] &= 0x01;
    refresh_crc(encoded);
    assert(waypoints_decode(encoded, sizeof(encoded), &decoded));
    assert(decoded.variation == 0 && decoded.units == NAV_UNITS_METRIC);

    assert(waypoints_set(&c, 0, (nav_coordinate_t){49.287, -123.1234567}));
    assert(waypoints_set(&c, 5, (nav_coordinate_t){-33.8568, 151.2153}));
    assert(waypoints_select(&c, 5));
    assert(waypoints_set_units(&c, NAV_UNITS_NAUTICAL));
    c.variation = -12;
    assert(waypoints_encode(&c, encoded));
    assert(waypoints_decode(encoded, sizeof(encoded), &decoded));
    assert(decoded.occupied_mask == 0x21 && decoded.selected == 5 && decoded.units == NAV_UNITS_NAUTICAL && decoded.variation == -12);
    assert(waypoints_get_selected(&decoded, &point));
    assert(near(point.latitude_deg, -33.8568) && near(point.longitude_deg, 151.2153));
    assert(!waypoints_select(&decoded, 1));

    /* Every fixed slot participates in a complete collection roundtrip. */
    for (uint8_t i = 1; i < WAYPOINTS_SLOT_COUNT; ++i)
        assert(waypoints_set(&c, i, (nav_coordinate_t){(double)i, (double)-i}));
    assert(waypoints_encode(&c, encoded));
    assert(waypoints_decode(encoded, sizeof(encoded), &decoded));
    assert(decoded.occupied_mask == 0x3f && decoded.selected == 5);
    for (uint8_t i = 0; i < WAYPOINTS_SLOT_COUNT; ++i) {
        assert(near(decoded.coordinates[i].latitude_deg, c.coordinates[i].latitude_deg));
        assert(near(decoded.coordinates[i].longitude_deg, c.coordinates[i].longitude_deg));
    }
    /* Version 1 retains all six slots, selection, and nautical units. */
    memcpy(damaged, encoded, sizeof(damaged));
    damaged[4] = 1; damaged[6] &= 0x3f; damaged[7] &= 0x01; refresh_crc(damaged);
    assert(waypoints_decode(damaged, sizeof(damaged), &decoded));
    assert(decoded.occupied_mask == 0x3f && decoded.selected == 5 &&
           decoded.units == NAV_UNITS_NAUTICAL && decoded.variation == 0);

    assert(waypoints_remove(&decoded, 5));
    assert(decoded.selected == 0 && decoded.occupied_mask == 0x1f);
    assert(!waypoints_remove(&decoded, 5));
    assert(waypoints_remove(&decoded, 0));
    assert(decoded.selected == 1 && decoded.occupied_mask == 0x1e);

    /* A historical BIPW record and zero-padded settings block migrate to slot 0. */
    assert(nav_waypoint_encode((nav_coordinate_t){0, 0}, legacy));
    memset(legacy + NAV_WAYPOINT_RECORD_SIZE, 0, sizeof(legacy) - NAV_WAYPOINT_RECORD_SIZE);
    assert(waypoints_decode(legacy, NAV_WAYPOINT_RECORD_SIZE, &decoded));
    assert(decoded.occupied_mask == 1 && decoded.selected == 0 && decoded.units == NAV_UNITS_METRIC && decoded.variation == 0);
    assert(waypoints_decode(legacy, sizeof(legacy), &decoded));
    legacy[40] = 1;
    assert(!waypoints_decode(legacy, sizeof(legacy), &decoded));

    /* Empty slots are canonical zeroes, even if the outer CRC is valid. */
    waypoints_init(&c);
    assert(waypoints_encode(&c, encoded));
    memcpy(damaged, encoded, sizeof(damaged)); damaged[8 + NAV_WAYPOINT_RECORD_SIZE] = 1;
    refresh_crc(damaged);
    assert_decode_rejected_unchanged(damaged, sizeof(damaged));

    /* Outer integrity alone cannot hide a damaged occupied BIPW inner CRC. */
    assert(waypoints_set(&c, 2, (nav_coordinate_t){10, -20}));
    assert(waypoints_encode(&c, encoded));
    memcpy(damaged, encoded, sizeof(damaged));
    damaged[8 + 2 * NAV_WAYPOINT_RECORD_SIZE + 13] ^= 1;
    refresh_crc(damaged);
    assert_decode_rejected_unchanged(damaged, sizeof(damaged));

    memcpy(damaged, encoded, sizeof(damaged)); damaged[20] ^= 1;
    assert_decode_rejected_unchanged(damaged, sizeof(damaged));
    memcpy(damaged, encoded, sizeof(damaged)); damaged[0] = 'X'; refresh_crc(damaged);
    assert_decode_rejected_unchanged(damaged, sizeof(damaged));
    memcpy(damaged, encoded, sizeof(damaged)); damaged[4] = 3; refresh_crc(damaged);
    assert_decode_rejected_unchanged(damaged, sizeof(damaged));
    memcpy(damaged, encoded, sizeof(damaged)); damaged[5] = WAYPOINTS_SLOT_COUNT; refresh_crc(damaged);
    assert_decode_rejected_unchanged(damaged, sizeof(damaged));
    memcpy(damaged, encoded, sizeof(damaged)); damaged[6] = (uint8_t)((damaged[6] & 0x3f) | 0xc0); damaged[7] = (uint8_t)((damaged[7] & 0x01) | 0xfe); refresh_crc(damaged);
    assert_decode_rejected_unchanged(damaged, sizeof(damaged));
    /* Version 1 rejects spare bits and invalid units even with a valid CRC. */
    memcpy(damaged, encoded, sizeof(damaged)); damaged[4] = 1; damaged[6] |= 0x40; refresh_crc(damaged);
    assert_decode_rejected_unchanged(damaged, sizeof(damaged));
    memcpy(damaged, encoded, sizeof(damaged)); damaged[4] = 1; damaged[7] = 2; refresh_crc(damaged);
    assert_decode_rejected_unchanged(damaged, sizeof(damaged));
    memcpy(damaged, encoded, sizeof(damaged)); damaged[4] = 3; refresh_crc(damaged);
    assert_decode_rejected_unchanged(damaged, sizeof(damaged));
    assert_decode_rejected_unchanged(encoded, sizeof(encoded) - 1);

    assert(!waypoints_set(&c, 6, (nav_coordinate_t){1, 2}));
    assert(!waypoints_set(&c, 2, (nav_coordinate_t){91, 0}));
    assert(!waypoints_set_units(&c, (nav_units_t)2));
    c.variation = 181;
    assert(!waypoints_encode(&c, encoded));
    c.variation = -181;
    assert(!waypoints_encode(&c, encoded));
    puts("waypoints tests passed");
    return 0;
}
