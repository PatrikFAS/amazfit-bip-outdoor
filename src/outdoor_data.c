#include "outdoor_data.h"

#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846f

static int is_leap_year(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int outdoor_date_valid(outdoor_date_t date)
{
    static const uint8_t days_per_month[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    int days;

    if (date.year < 1583 || date.year > 9999 || date.month < 1 ||
        date.month > 12 || date.day < 1) return 0;
    days = days_per_month[date.month - 1];
    if (date.month == 2 && is_leap_year(date.year)) ++days;
    return date.day <= days;
}

static int day_of_year(outdoor_date_t date)
{
    int month, result = date.day;
    for (month = 1; month < date.month; ++month) {
        if (month == 2) result += is_leap_year(date.year) ? 29 : 28;
        else if (month == 4 || month == 6 || month == 9 || month == 11) result += 30;
        else result += 31;
    }
    return result;
}

/* NOAA-style fractional-year approximation and sunrise hour-angle equation. */
static int calc_altitude_crossing(outdoor_date_t date, float latitude,
                                  float longitude, int utc_offset,
                                  float zenith, int *rise, int *set)
{
    const float days = is_leap_year(date.year) ? 366.0f : 365.0f;
    const float gamma = (2.0f * PI) / days *
                        (float)(day_of_year(date) - 1);
    const float s = sinf(gamma), c = cosf(gamma);
    const float s2 = sinf(2.0f * gamma), c2 = cosf(2.0f * gamma);
    const float decl = 0.006918f - 0.399912f * c + 0.070257f * s -
                       0.006758f * c2 + 0.000907f * s2 -
                       0.002697f * cosf(3.0f * gamma) +
                       0.00148f * sinf(3.0f * gamma);
    const float equation_minutes = 229.18f * (0.000075f + 0.001868f * c -
        0.032077f * s - 0.014615f * c2 - 0.040849f * s2);
    const float latitude_rad = latitude * (PI / 180.0f);
    const float denom = cosf(latitude_rad) * cosf(decl);
    float cos_hour_angle, hour_angle, solar_noon;
    if (fabsf(denom) < 1.0e-6f) return 0;
    cos_hour_angle = (cosf(zenith * PI / 180.0f) -
        sinf(latitude_rad) * sinf(decl)) / denom;
    if (cos_hour_angle < -1.0f || cos_hour_angle > 1.0f) return 0;
    hour_angle = atan2f(sqrtf((1.0f - cos_hour_angle) *
                               (1.0f + cos_hour_angle)),
                        cos_hour_angle) * (180.0f / PI) * 4.0f;
    solar_noon = 720.0f - 4.0f * longitude - equation_minutes +
                 (float)utc_offset;
    {
        float rise_value = solar_noon - hour_angle;
        float set_value = solar_noon + hour_angle;
        int rise_rounded = rise_value < 0.0f ?
            (int)(rise_value - 0.5f) : (int)(rise_value + 0.5f);
        int set_rounded = set_value < 0.0f ?
            (int)(set_value - 0.5f) : (int)(set_value + 0.5f);
        *rise = ((rise_rounded % 1440) + 1440) % 1440;
        *set = ((set_rounded % 1440) + 1440) % 1440;
    }
    return 1;
}

outdoor_solar_status_t outdoor_solar_calculate(outdoor_date_t date,
    float latitude, float longitude, int utc_offset, outdoor_solar_t *result)
{
    int rise, set;
    if (result == NULL) return OUTDOOR_SOLAR_INVALID;
    memset(result, 0, sizeof(*result));
    result->civil_dawn_minute = result->civil_dusk_minute = -1;
    result->nautical_dawn_minute = result->nautical_dusk_minute = -1;
    result->status = OUTDOOR_SOLAR_INVALID;
    if (!outdoor_date_valid(date) || !isfinite(latitude) || !isfinite(longitude) ||
        latitude < -90.0f || latitude > 90.0f || longitude < -180.0f ||
        longitude > 180.0f || utc_offset < -840 || utc_offset > 840) return result->status;
    if (!calc_altitude_crossing(date, latitude, longitude, utc_offset,
                                90.833f, &rise, &set)) {
        result->status = OUTDOOR_SOLAR_NO_RISE_SET;
        return result->status;
    }
    result->sunrise_minute = rise;
    result->sunset_minute = set;
    (void)calc_altitude_crossing(date, latitude, longitude, utc_offset,
                                 96.0f, &result->civil_dawn_minute,
                                 &result->civil_dusk_minute);
    (void)calc_altitude_crossing(date, latitude, longitude, utc_offset,
                                 102.0f, &result->nautical_dawn_minute,
                                 &result->nautical_dusk_minute);
    result->status = OUTDOOR_SOLAR_OK;
    return result->status;
}

static uint32_t read_u32le(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint16_t read_u16le(const uint8_t *p)
{
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

static int16_t read_i16le(const uint8_t *p)
{
    uint16_t value = read_u16le(p);
    return (int16_t)(value < 32768u ? value : (int32_t)value - 65536);
}

static int32_t read_i32le(const uint8_t *p)
{
    uint32_t value = read_u32le(p);
    return (int32_t)(value <= INT32_MAX ? (int64_t)value :
                     (int64_t)value - INT64_C(4294967296));
}

static uint32_t crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = UINT32_MAX;
    size_t i;
    int bit;
    for (i = 0; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0u);
    }
    return crc ^ UINT32_MAX;
}

static int decode_text(char *output, size_t capacity,
                       const uint8_t *input, size_t length)
{
    size_t i;
    if (length >= capacity) return 0;
    for (i = 0; i < length; ++i) {
        if (input[i] < 32 || input[i] > 126) return 0;
        output[i] = (char)input[i];
    }
    output[length] = '\0';
    return 1;
}

outdoor_tide_status_t outdoor_tide_decode(const uint8_t *data, size_t length,
    uint32_t now_utc, outdoor_tide_info_t *info,
    outdoor_tide_event_t *events, size_t event_capacity)
{
    uint8_t count;
    size_t required, index;
    uint32_t previous_utc = 0;

    if (data == NULL || info == NULL || events == NULL ||
        length < OUTDOOR_TIDE_HEADER_SIZE || memcmp(data, "BIPT", 4) != 0 ||
        data[4] != 1) return OUTDOOR_TIDE_INVALID;
    count = data[5];
    if (count > OUTDOOR_TIDE_MAX_EVENTS || event_capacity < count)
        return OUTDOOR_TIDE_INVALID;
    required = OUTDOOR_TIDE_HEADER_SIZE +
               (size_t)count * OUTDOOR_TIDE_EVENT_SIZE + 4u;
    if (length != required || read_u32le(data + length - 4) !=
        crc32(data, length - 4)) return OUTDOOR_TIDE_INVALID;

    memset(info, 0, sizeof(*info));
    info->event_count = count;
    info->generated_utc = read_u32le(data + 8);
    info->expires_utc = read_u32le(data + 12);
    info->latitude_e7 = read_i32le(data + 16);
    info->longitude_e7 = read_i32le(data + 20);
    info->utc_offset_minutes = read_i16le(data + 24);
    if (data[6] != 0 || data[7] != 0 || data[29] != 0 ||
        data[30] != 0 || data[31] != 0 ||
        info->expires_utc <= info->generated_utc ||
        info->utc_offset_minutes < -840 || info->utc_offset_minutes > 840 ||
        info->latitude_e7 < -900000000 || info->latitude_e7 > 900000000 ||
        info->longitude_e7 < -1800000000 || info->longitude_e7 > 1800000000)
        return OUTDOOR_TIDE_INVALID;
    if (!decode_text(info->station, sizeof(info->station), data + 32, data[26]) ||
        !decode_text(info->source, sizeof(info->source), data + 64, data[27]) ||
        !decode_text(info->note, sizeof(info->note), data + 96, data[28]) ||
        data[26] == 0 || data[27] == 0) return OUTDOOR_TIDE_INVALID;

    for (index = 0; index < count; ++index) {
        const uint8_t *event = data + OUTDOOR_TIDE_HEADER_SIZE +
                               index * OUTDOOR_TIDE_EVENT_SIZE;
        events[index].utc = read_u32le(event);
        events[index].height_cm = read_i16le(event + 4);
        events[index].kind = (outdoor_tide_kind_t)event[6];
        if (event[7] != 0 || events[index].kind < OUTDOOR_TIDE_HIGH ||
            events[index].kind > OUTDOOR_TIDE_LOW ||
            events[index].utc > info->expires_utc ||
            (index != 0 && events[index].utc <= previous_utc))
            return OUTDOOR_TIDE_INVALID;
        previous_utc = events[index].utc;
    }
    if (count == 0) return OUTDOOR_TIDE_EMPTY;
    if (now_utc > info->expires_utc) return OUTDOOR_TIDE_EXPIRED;
    return OUTDOOR_TIDE_OK;
}

int outdoor_tide_next(const outdoor_tide_info_t *info,
    const outdoor_tide_event_t *events, uint32_t now_utc,
    outdoor_tide_kind_t kind, outdoor_tide_event_t *next)
{
    size_t index;
    if (info == NULL || events == NULL || next == NULL ||
        (kind != OUTDOOR_TIDE_HIGH && kind != OUTDOOR_TIDE_LOW)) return 0;
    for (index = 0; index < info->event_count; ++index) {
        if (events[index].kind == kind && events[index].utc >= now_utc) {
            *next = events[index];
            return 1;
        }
    }
    return 0;
}
