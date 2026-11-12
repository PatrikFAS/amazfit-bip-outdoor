#include <libbip.h>
#include "bip_display.h"
#include "outdoor_data.h"
#include "tides_config.h"
#include <stdint.h>

enum { PAGE_TIDES, PAGE_SUN, PAGE_TWILIGHT, PAGE_INFO, PAGE_COUNT, PAGE_TIDE_LIST = PAGE_COUNT };
#define UPDATE_MS 2000
#define DATA_REFRESH_TICKS (30000 / UPDATE_MS)
#define TIDES_PER_PAGE 3u

typedef struct {
    void *ret_f;
    Elf_proc_ *proc;
    int page;
    unsigned char refresh_ticks;
    unsigned char list_page;
    unsigned char last_list_page;
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
    bip_keep_screen_open();
    draw_screen(state);
    set_update_period(1, UPDATE_MS);
}

static void screen_job(void)
{
    tides_sun_state_t **slot = (tides_sun_state_t **)get_ptr_temp_buf_2();
    if (!slot || !*slot) return;
    bip_keep_screen_open();
    if (++(*slot)->refresh_ticks < DATA_REFRESH_TICKS) {
        set_update_period(1, UPDATE_MS);
        return;
    }
    (*slot)->refresh_ticks = 0;
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
    int tap = gesture->gesture == GESTURE_CLICK;
    int next = gesture->gesture == GESTURE_SWIPE_LEFT || gesture->gesture == GESTURE_SWIPE_UP;
    int back = gesture->gesture == GESTURE_SWIPE_RIGHT;
    int x = gesture->touch_pos_x;
    int footer = tap && (unsigned)x < 176u &&
                 gesture->touch_pos_y >= 144 && gesture->touch_pos_y < VIDEO_Y;
    if (state->page == PAGE_TIDE_LIST) {
        unsigned last_page = state->last_list_page;
        if (footer && x >= 58 && x < 118) state->page = PAGE_TIDES;
        else if (back) {
            if (state->list_page) --state->list_page;
            else state->page = PAGE_TIDES;
        }
        else if ((next || (footer && x >= 118)) && state->list_page < last_page)
            ++state->list_page;
        else if (footer && x < 58 && state->list_page) --state->list_page;
        else return 0;
    } else if (back) {
        if (state->page > PAGE_TIDES) --state->page;
        else return 0;
    } else if (state->page == PAGE_TIDES && footer && x < 88) {
        state->page = PAGE_TIDE_LIST;
        state->list_page = 0;
    } else if (next || (tap && state->page != PAGE_INFO)) {
        if (state->page < PAGE_INFO) ++state->page;
        else return 0;
    } else if (state->page == PAGE_INFO && footer && tap) {
        --state->page;
    } else return 0;
    draw_screen(state);
    repaint_screen_lines(0, VIDEO_Y);
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

static void draw_rule(int y)
{
    set_fg_color(COLOR_AQUA);
    draw_filled_rect(8, y, 167, y);
}

static void draw_notice(const char *message, int color)
{
    set_fg_color(color);
    text_out_center(message, 88, 76);
}

static void draw_time(int minute, int x, int y, int color)
{
    char line[12];
    if (minute < 0) {
        set_fg_color(COLOR_YELLOW);
        text_out("--:--", x, y);
        return;
    }
    _sprintf(line, "%02d:%02d", minute / 60, minute % 60);
    set_fg_color(color);
    text_out(line, x, y);
}

static void draw_sun_row(const char *label, int minute, int y, int color)
{
    set_fg_color(COLOR_AQUA);
    text_out(label, 8, y);
    draw_time(minute, 112, y, color);
}

static void draw_twilight_row(const char *label, int dawn, int dusk, int y)
{
    set_fg_color(COLOR_AQUA);
    text_out(label, 8, y);
    draw_time(dawn, 64, y, COLOR_WHITE);
    draw_time(dusk, 120, y, COLOR_WHITE);
}

static void draw_solar(int page)
{
    struct datetime_ dt;
    outdoor_solar_t sun;
    _memclr(&dt, sizeof(dt));
    get_current_date_time(&dt);
    outdoor_date_t date = { dt.year, dt.month, dt.day };
    if (!outdoor_date_valid(date) || !current_utc_seconds()) {
        draw_notice("CLOCK UNAVAILABLE", COLOR_RED);
        return;
    }
    if (outdoor_solar_calculate(date, TIDES_CONFIG_LATITUDE_DEG,
            TIDES_CONFIG_LONGITUDE_DEG, TIDES_CONFIG_UTC_OFFSET_MINUTES,
            &sun) != OUTDOOR_SOLAR_OK) {
        draw_notice(page == PAGE_SUN ? "SUNRISE/SET N/A" : "TWILIGHT N/A", COLOR_YELLOW);
        return;
    }
    if (page == PAGE_TWILIGHT) {
        set_fg_color(COLOR_AQUA);
        text_out("DAWN", 64, 51);
        text_out("DUSK", 120, 51);
        draw_rule(72);
        draw_twilight_row("CIVIL", sun.civil_dawn_minute, sun.civil_dusk_minute, 84);
        draw_twilight_row("NAUT", sun.nautical_dawn_minute, sun.nautical_dusk_minute, 118);
        return;
    }
    draw_sun_row("SUNRISE", sun.sunrise_minute, 51, COLOR_WHITE);
    draw_sun_row("SUNSET", sun.sunset_minute, 74, COLOR_WHITE);
    draw_rule(96);
    draw_sun_row("DAYLIGHT", sun.sunset_minute - sun.sunrise_minute, 103, COLOR_WHITE);
    int now = (int)dt.hour * 60 + dt.min;
    if (now >= sun.sunrise_minute && now < sun.sunset_minute)
        draw_sun_row("LEFT", sun.sunset_minute - now, 126, COLOR_GREEN);
    else {
        set_fg_color(COLOR_WHITE);
        text_out_center(now < sun.sunrise_minute ? "BEFORE SUNRISE" : "AFTER SUNSET", 88, 126);
    }
}

static void draw_tide_entry(const outdoor_tide_event_t *event, outdoor_tide_kind_t kind,
                            int offset, int y)
{
    set_fg_color(COLOR_AQUA);
    text_out(kind == OUTDOOR_TIDE_HIGH ? "HIGH" : "LOW", 8, y);
    if (!event) {
        draw_time(-1, 112, y, COLOR_GREEN);
        return;
    }

    outdoor_date_t date;
    int minute;
    char line[16];
    int height = event->height_cm;
    int magnitude = height < 0 ? -height : height;
    epoch_local(event->utc, offset, &date, &minute);
    draw_time(minute, 112, y, COLOR_GREEN);
    set_fg_color(COLOR_WHITE);
    _sprintf(line, "%s%ld.%02ld m", height < 0 ? "-" : "",
             (long)(magnitude / 100), (long)(magnitude % 100));
    text_out(line, 8, y + 22);
    _sprintf(line, "%02d/%02d", date.month, date.day);
    text_out(line, 112, y + 22);
}

static int tide_data_available(const tides_sun_state_t *state, uint32_t now)
{
    if (!now) draw_notice("CLOCK UNAVAILABLE", COLOR_RED);
    else if (state->tide_status == OUTDOOR_TIDE_EXPIRED ||
             (state->have_tide && now > state->tide_info.expires_utc)) {
        draw_notice("TIDES EXPIRED", COLOR_YELLOW);
        text_out_center("UPDATE DATA", 88, 101);
    } else if (state->tide_status == OUTDOOR_TIDE_INVALID) {
        draw_notice("TIDE DATA INVALID", COLOR_RED);
    } else if (!state->have_tide) {
        draw_notice("NO TIDE DATA", COLOR_YELLOW);
    } else return 1;
    return 0;
}

static void draw_tides(const tides_sun_state_t *state)
{
    uint32_t now = current_utc_seconds();
    if (!tide_data_available(state, now)) return;
    outdoor_tide_event_t event;
    int offset = state->tide_info.utc_offset_minutes;
    int found = outdoor_tide_next(&state->tide_info, state->tide_events, now, OUTDOOR_TIDE_HIGH, &event);
    draw_tide_entry(found ? &event : NULL, OUTDOOR_TIDE_HIGH, offset, 52);
    draw_rule(97);
    found = outdoor_tide_next(&state->tide_info, state->tide_events, now, OUTDOOR_TIDE_LOW, &event);
    draw_tide_entry(found ? &event : NULL, OUTDOOR_TIDE_LOW, offset, 103);
}

static void draw_tide_list(tides_sun_state_t *state)
{
    uint32_t now = current_utc_seconds();
    int previous = 0, next = 0;
    state->last_list_page = 0;
    if (tide_data_available(state, now)) {
        unsigned first = 0, count = state->tide_info.event_count;
        while (first < count && state->tide_events[first].utc < now) ++first;
        if (first == count) {
            draw_notice("NO UPCOMING TIDES", COLOR_YELLOW);
        } else {
            unsigned last_page = (count - first - 1) / TIDES_PER_PAGE;
            state->last_list_page = (unsigned char)last_page;
            if (state->list_page > last_page) state->list_page = (unsigned char)last_page;
            previous = state->list_page > 0;
            next = state->list_page < last_page;
            unsigned i = first + state->list_page * TIDES_PER_PAGE;
            int offset = state->tide_info.utc_offset_minutes;
            for (unsigned row = 0; row < TIDES_PER_PAGE && i < count; ++row, ++i) {
                const outdoor_tide_event_t *event = &state->tide_events[i];
                int y = 31 + (int)row * 40;
                if (row) draw_rule(y - 2);
                draw_tide_entry(event, event->kind, offset, y);
            }
        }
    }
    if (!previous && !next) state->list_page = 0;
    set_fg_color(COLOR_AQUA);
    if (previous) text_out_center("PREV", 29, 154);
    text_out_center("BACK", 88, 154);
    if (next) text_out_center("NEXT", 147, 154);
}

static void draw_info(const tides_sun_state_t *state)
{
    char line[24];
    int offset = TIDES_CONFIG_UTC_OFFSET_MINUTES;
    int magnitude = offset < 0 ? -offset : offset;
    set_fg_color(COLOR_WHITE);
    text_out_center(TIDES_CONFIG_STATION, 88, 50);
    _sprintf(line, "UTC %c%02d:%02d", offset < 0 ? '-' : '+', magnitude / 60, magnitude % 60);
    text_out_center(line, 88, 70);
    if (state->have_tide || state->tide_status == OUTDOOR_TIDE_EXPIRED) {
        outdoor_date_t date;
        int minute;
        _sprintf(line, "%.18s", state->tide_info.source);
        text_out_center(line, 88, 90);
        text_out_center("Chart datum", 88, 110);
        epoch_local(state->tide_info.expires_utc, state->tide_info.utc_offset_minutes, &date, &minute);
        _sprintf(line, "EXP %02d/%02d %02d:%02d", date.month, date.day, minute / 60, minute % 60);
        if (state->tide_status == OUTDOOR_TIDE_EXPIRED) set_fg_color(COLOR_YELLOW);
        text_out_center(line, 88, 131);
    } else {
        set_fg_color(COLOR_YELLOW);
        text_out_center("NO TIDE DATA", 88, 109);
    }
}

static void draw_screen(tides_sun_state_t *state)
{
    static const char *const titles[] = { "TIDES", "SUN", "TWILIGHT", "INFO" };
    set_bg_color(COLOR_BLACK);
    fill_screen_bg();
    GUI_UC_SetEncodeUTF8();
    load_font();
    set_fg_color(COLOR_AQUA);
    if (state->page == PAGE_TIDE_LIST) {
        text_out_center("TIDE LIST", 88, 6);
        draw_tide_list(state);
        return;
    }
    text_out_center(titles[state->page], 88, 6);
    set_fg_color(COLOR_WHITE);
    text_out_center(TIDES_CONFIG_LOCATION_NAME, 88, 27);
    if (state->page == PAGE_TIDES) draw_tides(state);
    else if (state->page == PAGE_INFO) draw_info(state);
    else draw_solar(state->page);
    set_fg_color(COLOR_AQUA);
    if (state->page == PAGE_TIDES) text_out_center("ALL TIDES", 44, 154);
    if (state->page < PAGE_INFO)
        text_out_center("NEXT >", state->page == PAGE_TIDES ? 132 : 88, 154);
    else
        text_out_center("< PREV", 88, 154);
}
