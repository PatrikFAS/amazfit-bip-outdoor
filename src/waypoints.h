#ifndef BIP_OUTDOOR_WAYPOINTS_H
#define BIP_OUTDOOR_WAYPOINTS_H

#include "nav.h"

#define WAYPOINTS_SLOT_COUNT 6u
#define WAYPOINTS_RECORD_SIZE (8u + WAYPOINTS_SLOT_COUNT * NAV_WAYPOINT_RECORD_SIZE + 4u)

typedef struct {
    nav_coordinate_t coordinates[WAYPOINTS_SLOT_COUNT];
    uint8_t occupied_mask;
    uint8_t selected;
    nav_units_t units;
    int16_t variation; /* Magnetic variation in degrees, east positive; 0 is Off. */
} waypoints_t;

void waypoints_init(waypoints_t *collection);
int waypoints_decode(const uint8_t *record, size_t length, waypoints_t *collection);
int waypoints_encode(const waypoints_t *collection, uint8_t output[WAYPOINTS_RECORD_SIZE]);
int waypoints_set(waypoints_t *collection, uint8_t slot, nav_coordinate_t coordinate);
int waypoints_remove(waypoints_t *collection, uint8_t slot);
int waypoints_select(waypoints_t *collection, uint8_t slot);
int waypoints_get_selected(const waypoints_t *collection, nav_coordinate_t *coordinate);
int waypoints_set_units(waypoints_t *collection, nav_units_t units);

#endif
