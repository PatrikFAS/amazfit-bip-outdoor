#include <libbip.h>
#include "outdoor_data.h"
#include "tides_config.h"
#include <stdint.h>

#define PAGE_SUN 0
#define PAGE_TIDES 1
#define PAGE_INFO 2
#define PAGE_TWILIGHT 3
#define UPDATE_MS 30000

typedef struct {
    void *ret_f;
    Elf_proc_ *proc;
    int page;
    outdoor_tide_info_t tide_info;
    outdoor_tide_event_t tide_events[OUTDOOR_TIDE_MAX_EVENTS];
    outdoor_tide_status_t tide_status;
    int have_tide;
} tides_sun_state_t;

static void show_screen(void *param);
static void screen_job(void);
static int dispatch_screen(void *param);
static void key_press_screen(void);
static void draw_screen(tides_sun_state_t *state);
static void exit_app(tides_sun_state_t *state);
static uint32_t current_utc_seconds(void);

static struct regmenu_ screen_data = {
    55, 1, 0, dispatch_screen, key_press_screen, screen_job, 0,
    show_screen, 0, 0
};

int main(int param0, char **argv)
{
    (void)argv;
    show_screen((void *)(uintptr_t)(unsigned int)param0);
    return 0;
}

static void show_screen(void *param)
{
    tides_sun_state_t **slot = (tides_sun_state_t **)get_ptr_temp_buf_2();
    tides_sun_state_t *state;
    if (!slot) return;
    if (*slot && param == *slot && get_var_menu_overlay()) {
        state = *slot;
        *slot = 0;
        reg_menu(&screen_data, 0);
        *slot = state;
    } else {
        reg_menu(&screen_data, 0);
        *slot = (tides_sun_state_t *)pvPortMalloc(sizeof(*state));
        state = *slot;
        if (!state) { show_watchface(); return; }
        _memclr(state, sizeof(*state));
        state->proc = get_proc_by_addr((void *)main);
        state->ret_f = (param && state->proc && state->proc->elf_finish) ?
                       (void *)state->proc->elf_finish : (void *)show_watchface;
        state->tide_status = OUTDOOR_TIDE_EMPTY;
#if TIDES_CONFIG_DATA_SIZE > 0
        state->tide_status = outdoor_tide_decode(tides_config_data,
            TIDES_CONFIG_DATA_SIZE, current_utc_seconds(),
            &state->tide_info, state->tide_events, OUTDOOR_TIDE_MAX_EVENTS);
        state->have_tide = state->tide_status == OUTDOOR_TIDE_OK;
#endif
    }
    draw_screen(state);
    set_update_period(1, UPDATE_MS);
}

static void screen_job(void)
{
    tides_sun_state_t **slot = (tides_sun_state_t **)get_ptr_temp_buf_2();
    if (!slot || !*slot) return;
#if TIDES_CONFIG_DATA_SIZE > 0
    (*slot)->tide_status = outdoor_tide_decode(tides_config_data,
        TIDES_CONFIG_DATA_SIZE, current_utc_seconds(),
        &(*slot)->tide_info, (*slot)->tide_events, OUTDOOR_TIDE_MAX_EVENTS);
    (*slot)->have_tide = (*slot)->tide_status == OUTDOOR_TIDE_OK;
#endif
    draw_screen(*slot);
    repaint_screen_lines(0, VIDEO_Y);
    set_update_period(1, UPDATE_MS);
}

static int dispatch_screen(void *param)
{
    tides_sun_state_t **slot = (tides_sun_state_t **)get_ptr_temp_buf_2();
    tides_sun_state_t *state;
    struct gesture_ *gesture = (struct gesture_ *)param;
    if (!slot || !(state = *slot) || !gesture) return 0;
    if (gesture->gesture == GESTURE_SWIPE_RIGHT) exit_app(state);
    else if (gesture->gesture == GESTURE_SWIPE_LEFT ||
             gesture->gesture == GESTURE_SWIPE_UP) {
        state->page = (state->page + 1) % 4;
        draw_screen(state);
        repaint_screen_lines(0, VIDEO_Y);
    } else if (gesture->gesture == GESTURE_CLICK) {
        state->page = (state->page + 1) % 4;
        draw_screen(state);
        repaint_screen_lines(0, VIDEO_Y);
    }
    return 0;
}

static void key_press_screen(void)
{
    tides_sun_state_t **slot = (tides_sun_state_t **)get_ptr_temp_buf_2();
    if (slot && *slot) exit_app(*slot);
}

static void exit_app(tides_sun_state_t *state)
{
    set_update_period(0, 0);
    show_menu_animate(state ? state->ret_f : (void *)show_watchface,
                      (int)(uintptr_t)show_screen, ANIMATE_RIGHT);
}

/* Gregorian civil date from Unix days, including negative offsets across midnight. */
static void epoch_local(uint32_t utc, int offset, outdoor_date_t *date, int *minute)
{
    int64_t seconds = (int64_t)utc + (int64_t)offset * 60;
    int64_t days = seconds / 86400, sod = seconds % 86400;
    int64_t z, era, doe, yoe, y, doy, mp, d, m;
    if (sod < 0) { sod += 86400; --days; }
    z = days + 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = z - era * 146097;
    yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    y = yoe + era * 400;
    doy = doe - (365*yoe + yoe/4 - yoe/100);
    mp = (5*doy + 2) / 153;
    d = doy - (153*mp+2)/5 + 1;
    m = mp + (mp < 10 ? 3 : -9);
    y += (m <= 2);
    date->year = (int)y; date->month = (int)m; date->day = (int)d;
    *minute = (int)(sod / 60);
}

static void format_height(char *line, int16_t height_cm)
{
    int magnitude = height_cm < 0 ? -(int)height_cm : (int)height_cm;
    _sprintf(line, "HEIGHT %s%ld.%02ldM", height_cm < 0 ? "-" : "",
             (long)(magnitude / 100), (long)(magnitude % 100));
}

static int64_t civil_days(outdoor_date_t date)
{
    int64_t y = date.year, m = date.month, d = date.day;
    int64_t era, yoe, doy, doe;
    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe/4 - yoe/100 + doy;
    return era * 146097 + doe - 719468;
}

/* The watch clock is local wall time; convert it with our explicit configured
 * fixed offset instead of relying on undocumented timestamp timezone semantics. */
static uint32_t current_utc_seconds(void)
{
    struct datetime_ dt;
    outdoor_date_t date;
    int64_t seconds;
    _memclr(&dt, sizeof(dt));
    get_current_date_time(&dt);
    date.year = dt.year; date.month = dt.month; date.day = dt.day;
    if (!outdoor_date_valid(date) || dt.hour > 23 || dt.min > 59 || dt.sec > 59)
        return 0;
    seconds = civil_days(date) * INT64_C(86400) + dt.hour * 3600 + dt.min * 60 + dt.sec;
    seconds -= (int64_t)TIDES_CONFIG_UTC_OFFSET_MINUTES * 60;
    if (seconds < 0 || seconds > UINT32_MAX) return 0;
    return (uint32_t)seconds;
}

static void draw_header(int page)
{
    text_out(page == PAGE_SUN ? "SUN" : page == PAGE_TWILIGHT ? "TWILIGHT" :
             page == PAGE_TIDES ? "TIDES" : "INFO", 8, 3);
    text_out("POINT ATK UTC-07", 8, 22);
}

static void draw_sun(void)
{
    struct datetime_ dt;
    outdoor_date_t date;
    outdoor_solar_t sun;
    char line[28];
    _memclr(&dt, sizeof(dt));
    get_current_date_time(&dt);
    date.year = dt.year; date.month = dt.month; date.day = dt.day;
    if (!outdoor_date_valid(date) || !current_utc_seconds()) {
        text_out("CLOCK UNAVAILABLE", 8, 50); return;
    }
    _sprintf(line, "%04d-%02d-%02d", date.year, date.month, date.day);
    text_out(line, 8, 43);
    _sprintf(line, "NOW %02d:%02d", (int)dt.hour, (int)dt.min);
    text_out(line, 8, 62);
    if (outdoor_solar_calculate(date, TIDES_CONFIG_LATITUDE_DEG,
            TIDES_CONFIG_LONGITUDE_DEG, TIDES_CONFIG_UTC_OFFSET_MINUTES,
            &sun) != OUTDOOR_SOLAR_OK) {
        text_out("SUNRISE/SET N/A", 8, 88);
        text_out("NO RISE/SET TODAY", 8, 108);
        return;
    }
    _sprintf(line, "RISE %02d:%02d", sun.sunrise_minute/60, sun.sunrise_minute%60);
    text_out(line, 8, 82);
    _sprintf(line, "SET  %02d:%02d", sun.sunset_minute/60, sun.sunset_minute%60);
    text_out(line, 8, 102);
    {
        int now = (int)dt.hour * 60 + dt.min;
        if (now < sun.sunrise_minute) _sprintf(line, "BEFORE SUNRISE");
        else if (now >= sun.sunset_minute) _sprintf(line, "AFTER SUNSET");
        else {
            int left = sun.sunset_minute - now;
            _sprintf(line, "DAY LEFT %02d:%02d", left/60, left%60);
        }
    }
    text_out(line, 8, 122);
    _sprintf(line, "DAYLENGTH %02d:%02d",
             (sun.sunset_minute - sun.sunrise_minute)/60,
             (sun.sunset_minute - sun.sunrise_minute)%60);
    text_out(line, 8, 142);
}

static void draw_twilight(void)
{
    struct datetime_ dt;
    outdoor_date_t date;
    outdoor_solar_t sun;
    char line[24];
    _memclr(&dt, sizeof(dt)); get_current_date_time(&dt);
    date.year = dt.year; date.month = dt.month; date.day = dt.day;
    if (!outdoor_date_valid(date) || !current_utc_seconds()) {
        text_out("CLOCK UNAVAILABLE", 8, 62); return;
    }
    if (outdoor_solar_calculate(date, TIDES_CONFIG_LATITUDE_DEG,
            TIDES_CONFIG_LONGITUDE_DEG, TIDES_CONFIG_UTC_OFFSET_MINUTES,
            &sun) != OUTDOOR_SOLAR_OK) {
        text_out("TWILIGHT N/A", 8, 62); return;
    }
    if (sun.civil_dawn_minute < 0) text_out("CIVIL AM --:--", 8, 66);
    else { _sprintf(line, "CIVIL AM %02d:%02d", sun.civil_dawn_minute/60,
                    sun.civil_dawn_minute%60); text_out(line, 8, 66); }
    if (sun.civil_dusk_minute < 0) text_out("CIVIL PM --:--", 8, 86);
    else { _sprintf(line, "CIVIL PM %02d:%02d", sun.civil_dusk_minute/60,
                    sun.civil_dusk_minute%60); text_out(line, 8, 86); }
    if (sun.nautical_dawn_minute < 0) text_out("NAUT AM --:--", 8, 106);
    else { _sprintf(line, "NAUT AM %02d:%02d", sun.nautical_dawn_minute/60,
                    sun.nautical_dawn_minute%60); text_out(line, 8, 106); }
    if (sun.nautical_dusk_minute < 0) text_out("NAUT PM --:--", 8, 126);
    else { _sprintf(line, "NAUT PM %02d:%02d", sun.nautical_dusk_minute/60,
                    sun.nautical_dusk_minute%60); text_out(line, 8, 126); }
}

static void draw_tides(tides_sun_state_t *state)
{
    char line[40];
    outdoor_tide_event_t next;
    uint32_t now = current_utc_seconds();
    if (!now) { text_out("CLOCK UNAVAILABLE", 8, 66); return; }
    if (state->tide_status == OUTDOOR_TIDE_EXPIRED) {
        text_out("TIDE DATA EXPIRED", 8, 66);
        text_out("UPDATE REQUIRED", 8, 88);
        return;
    }
    if (state->tide_status == OUTDOOR_TIDE_INVALID) {
        text_out("TIDE DATA INVALID", 8, 66);
        return;
    }
    if (!state->have_tide) {
        text_out("NO TIDE DATA", 8, 66);
        text_out("NO OFFLINE TABLE", 8, 88);
        text_out("NO CURVE ESTIMATE", 8, 110);
        return;
    }
    if (outdoor_tide_next(&state->tide_info, state->tide_events, now,
                          OUTDOOR_TIDE_HIGH, &next)) {
        outdoor_date_t d; int min;
        epoch_local(next.utc, state->tide_info.utc_offset_minutes, &d, &min);
        _sprintf(line, "HIGH %02d/%02d %02d:%02d", d.month, d.day, min/60, min%60);
        text_out(line, 8, 66);
        format_height(line, (int16_t)next.height_cm);
        text_out(line, 8, 84);
    } else text_out("NO NEXT HIGH", 8, 66);
    if (outdoor_tide_next(&state->tide_info, state->tide_events, now,
                          OUTDOOR_TIDE_LOW, &next)) {
        outdoor_date_t d; int min;
        epoch_local(next.utc, state->tide_info.utc_offset_minutes, &d, &min);
        _sprintf(line, "LOW  %02d/%02d %02d:%02d", d.month, d.day, min/60, min%60);
        text_out(line, 8, 104);
        format_height(line, (int16_t)next.height_cm);
        text_out(line, 8, 122);
    } else text_out("NO NEXT LOW", 8, 104);
    _sprintf(line, "STA %.14s", state->tide_info.station);
    text_out(line, 8, 142);
    _sprintf(line, "SRC %.14s", state->tide_info.source);
    text_out(line, 8, 160);
}

static void draw_info(tides_sun_state_t *state)
{
    char line[32];
    text_out(TIDES_CONFIG_LOCATION_NAME, 8, 50);
    _sprintf(line, "%.4f %.4f", TIDES_CONFIG_LATITUDE_DEG,
             TIDES_CONFIG_LONGITUDE_DEG); text_out(line, 8, 68);
    _sprintf(line, "UTC %c%02d:%02d",
        TIDES_CONFIG_UTC_OFFSET_MINUTES < 0 ? '-' : '+',
        (TIDES_CONFIG_UTC_OFFSET_MINUTES < 0 ? -TIDES_CONFIG_UTC_OFFSET_MINUTES :
          TIDES_CONFIG_UTC_OFFSET_MINUTES) / 60,
        (TIDES_CONFIG_UTC_OFFSET_MINUTES < 0 ? -TIDES_CONFIG_UTC_OFFSET_MINUTES :
          TIDES_CONFIG_UTC_OFFSET_MINUTES) % 60);
    text_out(line, 8, 86);
    _sprintf(line, "STA %.14s", TIDES_CONFIG_STATION); text_out(line, 8, 104);
    if (state->have_tide) {
        outdoor_date_t expires_date; int expires_minute;
        _sprintf(line, "SRC %.14s", state->tide_info.source);
        text_out(line, 8, 122);
        _sprintf(line, "%.18s", state->tide_info.note);
        text_out(line, 8, 140);
        epoch_local(state->tide_info.expires_utc,
                    state->tide_info.utc_offset_minutes,
                    &expires_date, &expires_minute);
        _sprintf(line, "EXP %02d/%02d %02d:%02d", expires_date.month,
                 expires_date.day, expires_minute/60, expires_minute%60);
        text_out(line, 8, 158);
    } else {
        text_out("NO TIDE DATA", 8, 122);
        text_out("SWIPE LEFT: NEXT", 8, 150);
    }
}

static void draw_screen(tides_sun_state_t *state)
{
    set_bg_color(COLOR_BLACK); fill_screen_bg();
    GUI_UC_SetEncodeUTF8(); load_font(); set_fg_color(COLOR_WHITE);
    draw_header(state->page);
    if (state->page == PAGE_SUN) draw_sun();
    else if (state->page == PAGE_TWILIGHT) draw_twilight();
    else if (state->page == PAGE_TIDES) draw_tides(state);
    else draw_info(state);
}
