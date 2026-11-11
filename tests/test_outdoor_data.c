#include "outdoor_data.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void put32(unsigned char *p, unsigned value)
{
    p[0] = (unsigned char)value; p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16); p[3] = (unsigned char)(value >> 24);
}
static unsigned crc32_bytes(const unsigned char *p, size_t n)
{
    unsigned crc = ~0u; size_t i; int bit;
    for (i = 0; i < n; ++i) {
        crc ^= p[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
    }
    return ~crc;
}
static void test_solar(void)
{
    outdoor_solar_t s;
    outdoor_date_t date = {2026, 3, 20};
    assert(outdoor_date_valid(date));
    assert(!outdoor_date_valid((outdoor_date_t){2025, 2, 29}));
    assert(outdoor_date_valid((outdoor_date_t){2024, 2, 29}));
    assert(outdoor_solar_calculate(date, 0, 0, 0, &s) == OUTDOOR_SOLAR_OK);
    assert(abs(s.sunrise_minute - 360) < 12);
    assert(abs(s.sunset_minute - 1080) < 12);
    assert(s.civil_dawn_minute >= 0 && s.civil_dawn_minute < s.sunrise_minute);
    assert(s.nautical_dawn_minute >= 0 && s.nautical_dawn_minute < s.civil_dawn_minute);
    assert(outdoor_solar_calculate((outdoor_date_t){2026, 6, 21}, 89, 0, 0, &s) == OUTDOOR_SOLAR_NO_RISE_SET);
    assert(outdoor_solar_calculate((outdoor_date_t){2026, 11, 8}, 49.3375,
                                   -123.253583, -420, &s) == OUTDOOR_SOLAR_OK);
    /* USNO 2026-11-08 Point Atkinson reference: sunrise 08:13, sunset 17:40 UTC-07. */
    assert(abs(s.sunrise_minute - 493) <= 3 && abs(s.sunset_minute - 1060) <= 3);
    assert(outdoor_solar_calculate((outdoor_date_t){2026, 6, 21}, 60, 0, 0, &s) == OUTDOOR_SOLAR_OK);
    assert(s.nautical_dawn_minute == -1 && s.nautical_dusk_minute == -1);
    assert(outdoor_solar_calculate((outdoor_date_t){2026, 2, 30}, 0, 0, 0, &s) == OUTDOOR_SOLAR_INVALID);
}
static void test_tide_fixture(void)
{
    unsigned char b[156] = {0};
    outdoor_tide_info_t info; outdoor_tide_event_t events[1], next;
    memcpy(b, "BIPT\1", 5); b[5] = 1;
    put32(b + 8, 100); put32(b + 12, 300); b[26] = 1; b[27] = 1;
    b[32] = 'A'; b[64] = 'S'; put32(b + 144, 150);
    b[148] = 44; b[149] = 1; b[150] = 1;
    put32(b + 152, crc32_bytes(b, 152));
    assert(outdoor_tide_decode(b, sizeof(b), 150, &info, events, 1) == OUTDOOR_TIDE_OK);
    assert(events[0].kind == OUTDOOR_TIDE_HIGH && events[0].height_cm == 300);
    assert(outdoor_tide_next(&info, events, 150, OUTDOOR_TIDE_HIGH, &next) && next.utc == 150);
    assert(outdoor_tide_decode(b, sizeof(b), 301, &info, events, 1) == OUTDOOR_TIDE_EXPIRED);
    b[149] ^= 1;
    assert(outdoor_tide_decode(b, sizeof(b), 200, &info, events, 1) == OUTDOOR_TIDE_INVALID);
    assert(outdoor_tide_decode(b, sizeof(b) - 1, 200, &info, events, 1) == OUTDOOR_TIDE_INVALID);
}
static void test_imported_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    unsigned char data[OUTDOOR_TIDE_HEADER_SIZE + OUTDOOR_TIDE_MAX_EVENTS * 8 + 4];
    size_t length; outdoor_tide_info_t info; outdoor_tide_event_t events[OUTDOOR_TIDE_MAX_EVENTS];
    assert(file != NULL); length = fread(data, 1, sizeof(data), file); assert(!ferror(file)); fclose(file);
    assert(outdoor_tide_decode(data, length, 1794099600u, &info, events,
                               OUTDOOR_TIDE_MAX_EVENTS) == OUTDOOR_TIDE_OK);
    assert(strcmp(info.station, "Point Atkinson CHS07795") == 0);
    assert(strcmp(info.source, "CHS/IWLS official API") == 0);
    assert(strlen(info.note) == OUTDOOR_TIDE_NOTE_MAX);
    assert(info.utc_offset_minutes == -420 && info.event_count == 3);
    assert(info.latitude_e7 == 493375000 && info.longitude_e7 == -1232535830);
    assert(events[0].kind == OUTDOOR_TIDE_HIGH && events[0].height_cm == 325);
    assert(events[1].kind == OUTDOOR_TIDE_LOW && events[1].height_cm == -50);
    assert(events[2].kind == OUTDOOR_TIDE_HIGH && events[2].height_cm == 401);
}
int main(int argc, char **argv)
{
    test_solar(); test_tide_fixture();
    if (argc == 2) test_imported_file(argv[1]);
    puts("outdoor_data: ok"); return 0;
}
