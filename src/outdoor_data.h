#ifndef BIP_OUTDOOR_DATA_H
#define BIP_OUTDOOR_DATA_H
#include <stddef.h>
#include <stdint.h>
#define OUTDOOR_TIDE_MAX_EVENTS 64u
#define OUTDOOR_TIDE_LABEL_MAX 32u
#define OUTDOOR_TIDE_SOURCE_MAX 32u
#define OUTDOOR_TIDE_NOTE_MAX 48u
#define OUTDOOR_TIDE_HEADER_SIZE 144u
#define OUTDOOR_TIDE_EVENT_SIZE 8u

typedef struct { int year, month, day; } outdoor_date_t;
typedef enum { OUTDOOR_SOLAR_OK=0, OUTDOOR_SOLAR_NO_RISE_SET=1, OUTDOOR_SOLAR_INVALID=2 } outdoor_solar_status_t;
typedef struct {
    outdoor_solar_status_t status;
    int sunrise_minute, sunset_minute;
    int civil_dawn_minute, civil_dusk_minute;
    int nautical_dawn_minute, nautical_dusk_minute;
} outdoor_solar_t;
int outdoor_date_valid(outdoor_date_t date);
/* Fixed UTC offset in minutes; output minutes are local [0,1439]. Unavailable twilight crossings are -1. */
outdoor_solar_status_t outdoor_solar_calculate(outdoor_date_t date, float latitude_deg,
    float longitude_deg, int utc_offset_minutes, outdoor_solar_t *result);

typedef enum { OUTDOOR_TIDE_HIGH=1, OUTDOOR_TIDE_LOW=2 } outdoor_tide_kind_t;
typedef struct {
    uint32_t generated_utc, expires_utc;
    int32_t latitude_e7, longitude_e7;
    int16_t utc_offset_minutes;
    uint8_t event_count;
    char station[OUTDOOR_TIDE_LABEL_MAX+1];
    char source[OUTDOOR_TIDE_SOURCE_MAX+1];
    char note[OUTDOOR_TIDE_NOTE_MAX+1];
} outdoor_tide_info_t;
typedef struct { uint32_t utc; int16_t height_cm; outdoor_tide_kind_t kind; } outdoor_tide_event_t;
typedef enum { OUTDOOR_TIDE_OK=0, OUTDOOR_TIDE_EMPTY=1, OUTDOOR_TIDE_EXPIRED=2, OUTDOOR_TIDE_INVALID=3 } outdoor_tide_status_t;
/* Decode explicit byte format; event records follow fixed 144-byte header. */
outdoor_tide_status_t outdoor_tide_decode(const uint8_t *data,size_t length,uint32_t now_utc,
    outdoor_tide_info_t *info, outdoor_tide_event_t *events, size_t event_capacity);
int outdoor_tide_next(const outdoor_tide_info_t *info,const outdoor_tide_event_t *events,
    uint32_t now_utc,outdoor_tide_kind_t kind,outdoor_tide_event_t *next);
#endif
