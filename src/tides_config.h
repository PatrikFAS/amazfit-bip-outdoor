#ifndef BIP_OUTDOOR_TIDES_CONFIG_H
#define BIP_OUTDOOR_TIDES_CONFIG_H

/* Explicit setup: Point Atkinson, BC, fixed UTC-07 offset. */
#define TIDES_CONFIG_LOCATION_NAME "POINT ATK"
#define TIDES_CONFIG_STATION "CHS07795"
#define TIDES_CONFIG_LATITUDE_DEG 49.3375
#define TIDES_CONFIG_LONGITUDE_DEG (-123.253583)
#define TIDES_CONFIG_UTC_OFFSET_MINUTES (-420)

/* Test builds inject a mutable fixed-size BIPT record. Production builds use
 * the generated offline table header when it is present. */
#ifdef TIDES_SUN_TEST
extern unsigned char tides_config_data[];
#define TIDES_CONFIG_DATA_SIZE 164u
#elif defined(__has_include)
#if __has_include("point_atkinson_tides.h")
#include "point_atkinson_tides.h"
#else
static const unsigned char tides_config_data[] = { 0 };
#define TIDES_CONFIG_DATA_SIZE 0u
#endif
#else
static const unsigned char tides_config_data[] = { 0 };
#define TIDES_CONFIG_DATA_SIZE 0u
#endif

#endif
