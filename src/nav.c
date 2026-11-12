#include "nav.h"

#include <math.h>
#ifdef NAV_SMALL_MATH
#include "nav_trig.h"
#endif

#define NAV_PI 3.14159265358979323846
#define NAV_EARTH_RADIUS_M 6371008.8
#define NAV_METRES_PER_NAUTICAL_MILE 1852.0

/* Share the coordinate checks and codecs across call sites in the ARM ELF.
 * Inlining them during LTO duplicates the double-precision setup and checks. */
#if defined(NAV_SMALL_MATH) && defined(__arm__) && defined(__GNUC__)
#define NAV_COORDINATE_NOINLINE __attribute__((noinline))
#else
#define NAV_COORDINATE_NOINLINE
#endif

NAV_COORDINATE_NOINLINE int nav_coordinate_valid(nav_coordinate_t c)
{
    /* Ordered range checks reject NaN and infinity as well. */
    return c.latitude_deg >= -90.0 && c.latitude_deg <= 90.0 &&
           c.longitude_deg >= -180.0 && c.longitude_deg <= 180.0;
}

int nav_distance_bearing(nav_coordinate_t from, nav_coordinate_t to,
                         double *distance_metres, double *bearing_deg)
{
#ifdef NAV_SMALL_MATH
    const float radians_per_degree = 0.01745329251994329577f;
    float lat1, lat2, dlat, dlon, a, c, y, x, bearing;
    double central_angle;
    double longitude_delta;
    if (!nav_coordinate_valid(from) || !nav_coordinate_valid(to) ||
        distance_metres == NULL || bearing_deg == NULL) return 0;
    if ((from.latitude_deg == to.latitude_deg && from.longitude_deg == to.longitude_deg) ||
        (from.latitude_deg == to.latitude_deg && fabs(from.latitude_deg) == 90.0)) {
        *distance_metres = 0.0;
        *bearing_deg = 0.0;
        return 1;
    }
    /* Subtract in double before narrowing so nearby coordinates retain their delta. */
    lat1 = (float)from.latitude_deg * radians_per_degree;
    lat2 = (float)to.latitude_deg * radians_per_degree;
    dlat = (float)(to.latitude_deg - from.latitude_deg) * radians_per_degree;
    longitude_delta = to.longitude_deg - from.longitude_deg;
    if (longitude_delta > 180.0) longitude_delta -= 360.0;
    else if (longitude_delta < -180.0) longitude_delta += 360.0;
    dlon = (float)longitude_delta * radians_per_degree;
    float sin_half_lat = nav_sin_bounded(dlat * 0.5f);
    float sin_half_lon = nav_sin_bounded(dlon * 0.5f);
    const float half_pi = 1.57079632679489661923f;
    float cos_lat2 = nav_sin_bounded(half_pi - fabsf(lat2));
    float cos_product = nav_sin_bounded(half_pi - fabsf(lat1)) * cos_lat2;
    a = sin_half_lat * sin_half_lat + cos_product * sin_half_lon * sin_half_lon;
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    if (a > 0.9f) {
        /* Near antipodes, 1-a loses precision; calculate the small complement. */
        float anti_dlat = (float)(to.latitude_deg + from.latitude_deg) * radians_per_degree;
        float anti_dlon = 3.14159265358979323846f - fabsf(dlon);
        float sin_anti_lat = nav_sin_bounded(anti_dlat * 0.5f);
        float sin_anti_lon = nav_sin_bounded(anti_dlon * 0.5f);
        float anti_a = sin_anti_lat * sin_anti_lat + cos_product * sin_anti_lon * sin_anti_lon;
        if (anti_a < 0.0f) anti_a = 0.0f;
        if (anti_a > 1.0f) anti_a = 1.0f;
        c = 2.0f * nav_atan2_bounded(sqrtf(anti_a), sqrtf(1.0f - anti_a));
        central_angle = NAV_PI - (double)c;
    } else {
        c = 2.0f * nav_atan2_bounded(sqrtf(a), sqrtf(1.0f - a));
        central_angle = (double)c;
    }
    y = nav_sin_bounded(dlon) * cos_lat2;
    /* Stable local-bearings identity avoids subtracting nearly equal terms. */
    x = nav_sin_bounded(dlat) + 2.0f * nav_sin_bounded(lat1) * cos_lat2 * sin_half_lon * sin_half_lon;
    bearing = nav_atan2_bounded(y, x) * (180.0f / 3.14159265358979323846f);
    if (bearing < 0.0f) bearing += 360.0f;
    if (bearing >= 360.0f) bearing -= 360.0f;
    *distance_metres = NAV_EARTH_RADIUS_M * central_angle;
    *bearing_deg = (double)bearing;
    return 1;
#else
    double lat1, lat2, dlat, dlon, a, c, y, x, bearing;
    if (!nav_coordinate_valid(from) || !nav_coordinate_valid(to) ||
        distance_metres == NULL || bearing_deg == NULL) return 0;
    if ((from.latitude_deg == to.latitude_deg && from.longitude_deg == to.longitude_deg) ||
        ((from.latitude_deg == 90.0 || from.latitude_deg == -90.0) &&
         from.latitude_deg == to.latitude_deg)) {
        *distance_metres = 0.0;
        *bearing_deg = 0.0;
        return 1;
    }
    lat1 = from.latitude_deg * NAV_PI / 180.0;
    lat2 = to.latitude_deg * NAV_PI / 180.0;
    dlat = lat2 - lat1;
    dlon = (to.longitude_deg - from.longitude_deg) * NAV_PI / 180.0;
    a = sin(dlat / 2.0) * sin(dlat / 2.0) +
        cos(lat1) * cos(lat2) * sin(dlon / 2.0) * sin(dlon / 2.0);
    if (a < 0.0) a = 0.0;
    if (a > 1.0) a = 1.0;
    c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    y = sin(dlon) * cos(lat2);
    x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dlon);
    bearing = atan2(y, x) * 180.0 / NAV_PI;
    if (bearing < 0.0) bearing += 360.0;
    if (bearing >= 360.0) bearing -= 360.0;
    *distance_metres = NAV_EARTH_RADIUS_M * c;
    *bearing_deg = bearing;
    return 1;
#endif
}

int nav_relative_angle(double bearing_deg, double heading_deg, double *relative_deg)
{
    double d;
    if (relative_deg == NULL || !isfinite(bearing_deg) || !isfinite(heading_deg) ||
        bearing_deg < 0.0 || bearing_deg >= 360.0 ||
        heading_deg < 0.0 || heading_deg >= 360.0) return 0;
    d = bearing_deg - heading_deg;
    while (d < -180.0) d += 360.0;
    while (d >= 180.0) d -= 360.0;
    *relative_deg = d;
    return 1;
}

int nav_fix_is_fresh(uint32_t now_ms, uint32_t fix_time_ms)
{
    return (uint32_t)(now_ms - fix_time_ms) <= NAV_MAX_FIX_AGE_MS;
}

int nav_convert_distance(double metres, nav_units_t units, double *converted)
{
    if (converted == NULL || !isfinite(metres) || metres < 0.0) return 0;
    if (units == NAV_UNITS_METRIC) *converted = metres;
    else if (units == NAV_UNITS_NAUTICAL) {
        *converted = metres / NAV_METRES_PER_NAUTICAL_MILE;
    }
    else return 0;
    return 1;
}

static uint32_t crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = UINT32_C(0xffffffff);
    size_t i;
    unsigned bit;
    for (i = 0; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0u);
    }
    return crc ^ UINT32_C(0xffffffff);
}

static void put_u32le(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16); p[3] = (uint8_t)(value >> 24);
}

static uint32_t get_u32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

NAV_COORDINATE_NOINLINE int nav_waypoint_encode(nav_coordinate_t c, uint8_t out[NAV_WAYPOINT_RECORD_SIZE])
{
    int32_t lat, lon;
    if (out == NULL || !nav_coordinate_valid(c)) return 0;
    /* Validated coordinate bounds make the rounded values fit signed int32. */
    lat = (int32_t)(c.latitude_deg * 10000000.0 +
                    (c.latitude_deg < 0.0 ? -0.5 : 0.5));
    lon = (int32_t)(c.longitude_deg * 10000000.0 +
                    (c.longitude_deg < 0.0 ? -0.5 : 0.5));
    out[0] = 'B'; out[1] = 'I'; out[2] = 'P'; out[3] = 'W'; out[4] = 1;
    put_u32le(out + 5, (uint32_t)lat);
    put_u32le(out + 9, (uint32_t)lon);
    put_u32le(out + 13, crc32(out, 13));
    return 1;
}

NAV_COORDINATE_NOINLINE int nav_waypoint_decode(const uint8_t *record, size_t length, nav_coordinate_t *c)
{
    uint32_t raw_lat, raw_lon;
    int32_t lat, lon;
    nav_coordinate_t decoded;
    if (record == NULL || c == NULL || length != NAV_WAYPOINT_RECORD_SIZE ||
        record[0] != 'B' || record[1] != 'I' || record[2] != 'P' ||
        record[3] != 'W' || record[4] != 1 || get_u32le(record + 13) != crc32(record, 13))
        return 0;
    raw_lat = get_u32le(record + 5); raw_lon = get_u32le(record + 9);
    lat = (int32_t)(raw_lat <= INT32_MAX ? (int64_t)raw_lat :
                    (int64_t)raw_lat - INT64_C(4294967296));
    lon = (int32_t)(raw_lon <= INT32_MAX ? (int64_t)raw_lon :
                    (int64_t)raw_lon - INT64_C(4294967296));
    decoded.latitude_deg = (double)lat / 10000000.0;
    decoded.longitude_deg = (double)lon / 10000000.0;
    if (!nav_coordinate_valid(decoded)) return 0;
    *c = decoded;
    return 1;
}
