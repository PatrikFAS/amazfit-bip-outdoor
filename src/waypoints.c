#include "waypoints.h"

#include <string.h>

#define HEADER_SIZE 8u
#define CRC_OFFSET (WAYPOINTS_RECORD_SIZE - 4u)
#define SLOT_OFFSET 8u
#define SLOT_MASK ((1u << WAYPOINTS_SLOT_COUNT) - 1u)

static void put_u32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static uint32_t get_u32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

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

static int collection_valid(const waypoints_t *c)
{
    unsigned i;
    if (c == NULL || (c->occupied_mask & (uint8_t)~SLOT_MASK) != 0 ||
        c->selected >= WAYPOINTS_SLOT_COUNT ||
        (c->units != NAV_UNITS_METRIC && c->units != NAV_UNITS_NAUTICAL)) return 0;
    for (i = 0; i < WAYPOINTS_SLOT_COUNT; ++i)
        if ((c->occupied_mask & (1u << i)) && !nav_coordinate_valid(c->coordinates[i])) return 0;
    return 1;
}

void waypoints_init(waypoints_t *c)
{
    if (c == NULL) return;
    memset(c, 0, sizeof(*c));
    c->units = NAV_UNITS_METRIC;
}

static int all_zero(const uint8_t *p, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i) if (p[i] != 0) return 0;
    return 1;
}

int waypoints_decode(const uint8_t *record, size_t length, waypoints_t *out)
{
    waypoints_t decoded;
    unsigned i;
    if (record == NULL || out == NULL) return 0;
    memset(&decoded, 0, sizeof(decoded));
    if (length == NAV_WAYPOINT_RECORD_SIZE || length == WAYPOINTS_RECORD_SIZE) {
        if (record[0] == 'B' && record[1] == 'I' && record[2] == 'P' && record[3] == 'W') {
            if (length == WAYPOINTS_RECORD_SIZE && !all_zero(record + NAV_WAYPOINT_RECORD_SIZE,
                    WAYPOINTS_RECORD_SIZE - NAV_WAYPOINT_RECORD_SIZE)) return 0;
            if (!nav_waypoint_decode(record, NAV_WAYPOINT_RECORD_SIZE, &decoded.coordinates[0])) return 0;
            memset(&decoded.coordinates[1], 0, sizeof(decoded.coordinates) - sizeof(decoded.coordinates[0]));
            decoded.occupied_mask = 1u; decoded.selected = 0; decoded.units = NAV_UNITS_METRIC;
            *out = decoded;
            return 1;
        }
    }
    if (length != WAYPOINTS_RECORD_SIZE || record[0] != 'B' || record[1] != 'I' ||
        record[2] != 'P' || record[3] != 'C' || record[4] != 1 ||
        record[5] >= WAYPOINTS_SLOT_COUNT || (record[6] & (uint8_t)~SLOT_MASK) != 0 ||
        record[7] > NAV_UNITS_NAUTICAL || get_u32le(record + CRC_OFFSET) != crc32_bytes(record, CRC_OFFSET)) return 0;
    decoded.selected = record[5]; decoded.occupied_mask = record[6]; decoded.units = (nav_units_t)record[7];
    for (i = 0; i < WAYPOINTS_SLOT_COUNT; ++i) {
        const uint8_t *slot = record + SLOT_OFFSET + i * NAV_WAYPOINT_RECORD_SIZE;
        if (decoded.occupied_mask & (1u << i)) {
            if (!nav_waypoint_decode(slot, NAV_WAYPOINT_RECORD_SIZE, &decoded.coordinates[i])) return 0;
        } else if (!all_zero(slot, NAV_WAYPOINT_RECORD_SIZE)) return 0;
    }
    if (!collection_valid(&decoded)) return 0;
    *out = decoded;
    return 1;
}

int waypoints_encode(const waypoints_t *c, uint8_t out[WAYPOINTS_RECORD_SIZE])
{
    unsigned i;
    if (out == NULL || !collection_valid(c)) return 0;
    memset(out, 0, WAYPOINTS_RECORD_SIZE);
    out[0] = 'B'; out[1] = 'I'; out[2] = 'P'; out[3] = 'C'; out[4] = 1;
    out[5] = c->selected; out[6] = c->occupied_mask; out[7] = (uint8_t)c->units;
    for (i = 0; i < WAYPOINTS_SLOT_COUNT; ++i)
        if ((c->occupied_mask & (1u << i)) &&
            !nav_waypoint_encode(c->coordinates[i], out + SLOT_OFFSET + i * NAV_WAYPOINT_RECORD_SIZE)) return 0;
    put_u32le(out + CRC_OFFSET, crc32_bytes(out, CRC_OFFSET));
    return 1;
}

int waypoints_set(waypoints_t *c, uint8_t slot, nav_coordinate_t coordinate)
{
    if (c == NULL || slot >= WAYPOINTS_SLOT_COUNT || !nav_coordinate_valid(coordinate)) return 0;
    c->coordinates[slot] = coordinate; c->occupied_mask |= (uint8_t)(1u << slot);
    return 1;
}

int waypoints_remove(waypoints_t *c, uint8_t slot)
{
    unsigned i;
    if (c == NULL || slot >= WAYPOINTS_SLOT_COUNT || !(c->occupied_mask & (1u << slot))) return 0;
    c->occupied_mask &= (uint8_t)~(1u << slot);
    memset(&c->coordinates[slot], 0, sizeof(c->coordinates[slot]));
    if (c->selected == slot) {
        c->selected = 0;
        for (i = 0; i < WAYPOINTS_SLOT_COUNT; ++i)
            if (c->occupied_mask & (1u << i)) { c->selected = (uint8_t)i; break; }
    }
    return 1;
}

int waypoints_select(waypoints_t *c, uint8_t slot)
{
    if (c == NULL || slot >= WAYPOINTS_SLOT_COUNT || !(c->occupied_mask & (1u << slot))) return 0;
    c->selected = slot; return 1;
}

int waypoints_get_selected(const waypoints_t *c, nav_coordinate_t *coordinate)
{
    if (!collection_valid(c) || coordinate == NULL || !(c->occupied_mask & (1u << c->selected))) return 0;
    *coordinate = c->coordinates[c->selected]; return 1;
}

int waypoints_set_units(waypoints_t *c, nav_units_t units)
{
    if (c == NULL || (units != NAV_UNITS_METRIC && units != NAV_UNITS_NAUTICAL)) return 0;
    c->units = units; return 1;
}
