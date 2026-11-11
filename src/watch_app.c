#include <libbip.h>
#include "nav.h"
#include "waypoints.h"

#include <stdint.h>

#ifndef DIAGNOSTIC
#define DIAGNOSTIC 0
#endif

#define SENSOR_DISABLE 0
#define SENSOR_ENABLE 1
#define UPDATE_MS 2000
#define CONFIRM_TICKS 3
#define NEAR_TARGET_METRES 10

#if !DIAGNOSTIC
enum nav_status { GPS_PAUSED, GPS_WAITING, GPS_UNCHANGED, GPS_READY };
enum waypoint_action { ACTION_NONE, ACTION_REPLACE, ACTION_DELETE };
enum waypoint_message { MSG_NONE, MSG_FULL, MSG_SAVED, MSG_DELETED, MSG_STORAGE, MSG_GPS };

typedef struct {
    int32_t distance; /* metres or tenths of the selected larger unit */
    int16_t bearing; /* -1 when unavailable */
    uint8_t large_unit;
    uint8_t status;
} nav_readings_t;
#endif

typedef struct {
    void *ret_f;
    Elf_proc_ *proc;
    navi_struct_ navi;
    nav_coordinate_t current;
    nav_coordinate_t waypoint;
#if !DIAGNOSTIC
    waypoints_t collection;
    uint8_t slot;
    uint8_t page;
    int action_pending;
    int action_ticks;
    enum waypoint_message message;
    nav_readings_t displayed;
#endif
    int has_fix;
    int unchanged;
    int have_sample;
    int last_latitude;
    int last_longitude;
    int last_ns;
    int last_ew;
    uint32_t sample_tick;
    int has_waypoint;
    int gps_on;
    int units;
} watch_state_t;

static void show_screen(void *param);
static void screen_job(void);
static int dispatch_screen(void *param);
static void key_press_screen(void);
static void draw_screen(watch_state_t *state);
static void poll_gps(watch_state_t *state);
static void exit_app(watch_state_t *state);
#if !DIAGNOSTIC
static int persist_collection(const waypoints_t *next);
static int draw_navigator(watch_state_t *state, int force);
static int navigator_gesture(watch_state_t *state, const struct gesture_ *gesture);
static int load_waypoint(watch_state_t *state);
static void sync_selected(watch_state_t *state);
#endif

static struct regmenu_ screen_data = {
    55, 1, 0,
    dispatch_screen,
    key_press_screen,
    screen_job, 0,
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
    watch_state_t **state_slot = (watch_state_t **)get_ptr_temp_buf_2();
    watch_state_t *state;
    Elf_proc_ *proc;

    if (!state_slot) return;
    if (*state_slot && param == *state_slot && get_var_menu_overlay()) {
        state = *state_slot;
        *state_slot = 0;
        reg_menu(&screen_data, 0);
        *state_slot = state;
#if !DIAGNOSTIC
        state->action_pending = ACTION_NONE;
#endif
    } else {
        reg_menu(&screen_data, 0);
        *state_slot = (watch_state_t *)pvPortMalloc(sizeof(watch_state_t));
        state = *state_slot;
        if (!state) {
            show_watchface();
            return;
        }
        _memclr(state, sizeof(*state));
        proc = get_proc_by_addr((void *)main);
        state->proc = proc;
        state->ret_f = (param && proc && proc->elf_finish) ?
                       (void *)proc->elf_finish : (void *)show_watchface;
        state->units = NAV_UNITS_METRIC;
#if !DIAGNOSTIC
        state->has_waypoint = load_waypoint(state);
#endif
        switch_gps_pressure_sensors(SENSOR_ENABLE);
        state->gps_on = 1;
    }
    set_display_state_value(8, 1);
    set_display_state_value(2, 1);
    poll_gps(state);
    draw_screen(state);
    set_update_period(1, UPDATE_MS);
}

static void poll_gps(watch_state_t *state)
{
    nav_coordinate_t candidate;
    /* Clear every field first; firmware may leave unavailable fields untouched. */
    _memclr(&state->navi, sizeof(state->navi));
    if (state->gps_on) get_navi_data(&state->navi);
    state->has_fix = 0;
    if (!state->gps_on || !is_gps_fixed() ||
        !IS_NAVI_GPS_LAT_READY(state->navi.ready) ||
        !IS_NAVI_GPS_LONG_READY(state->navi.ready)) {
        state->have_sample = 0;
        state->unchanged = 0;
        return;
    }
    if (state->navi.latitude < 0 || state->navi.longitude < 0 ||
        state->navi.ns < NAVI_NORTH_HEMISPHERE ||
        state->navi.ns > NAVI_SOUTH_HEMISPHERE ||
        (state->navi.ew != NAVI_WEST_HEMISPHERE &&
         state->navi.ew != NAVI_EAST_HEMISPHERE)) {
        state->have_sample = 0;
        state->unchanged = 0;
        return;
    }
    {
        uint32_t now = (uint32_t)get_tick_count();
        if (!state->have_sample || state->last_latitude != state->navi.latitude ||
            state->last_longitude != state->navi.longitude ||
            state->last_ns != state->navi.ns || state->last_ew != state->navi.ew) {
            state->last_latitude = state->navi.latitude;
            state->last_longitude = state->navi.longitude;
            state->last_ns = state->navi.ns;
            state->last_ew = state->navi.ew;
            state->sample_tick = now;
            state->have_sample = 1;
            state->unchanged = 0;
        } else if ((uint32_t)(now - state->sample_tick) > 15300u) {
            state->unchanged = 1;
        }
    }
    candidate.latitude_deg = (double)state->navi.latitude / 3000000.0;
    candidate.longitude_deg = (double)state->navi.longitude / 3000000.0;
    if (state->navi.ns == NAVI_SOUTH_HEMISPHERE)
        candidate.latitude_deg = -candidate.latitude_deg;
    if (state->navi.ew == NAVI_WEST_HEMISPHERE)
        candidate.longitude_deg = -candidate.longitude_deg;
    if (!nav_coordinate_valid(candidate)) {
        state->have_sample = 0;
        state->unchanged = 0;
        return;
    }
    state->current = candidate;
    state->has_fix = !state->unchanged;
}

static void screen_job(void)
{
    watch_state_t **slot = (watch_state_t **)get_ptr_temp_buf_2();
    watch_state_t *state;
    if (!slot || !(state = *slot)) return;
    poll_gps(state);
#if DIAGNOSTIC
    draw_screen(state);
    repaint_screen_lines(0, VIDEO_Y);
#else
    int expired = state->action_pending && --state->action_ticks == 0;
    if (expired) state->action_pending = ACTION_NONE;
    if (draw_navigator(state, expired)) repaint_screen_lines(0, VIDEO_Y);
#endif
    /* Experimental refresh: the launch-only flags did not prevent timeout on
     * the user's watch. Reassert the same upstream values during active GPS;
     * this does not enable the separate backlight flag or an unknown timer. */
    if (state->gps_on) {
        set_display_state_value(8, 1);
        set_display_state_value(2, 1);
    }
    set_update_period(1, UPDATE_MS);
}

static void toggle_gps(watch_state_t *state)
{
    state->gps_on = !state->gps_on;
    switch_gps_pressure_sensors(state->gps_on ? SENSOR_ENABLE : SENSOR_DISABLE);
    state->has_fix = 0;
    state->have_sample = 0;
    state->unchanged = 0;
    if (state->gps_on) poll_gps(state);
}

static void toggle_units(watch_state_t *state)
{
    state->units = state->units == NAV_UNITS_METRIC ? NAV_UNITS_NAUTICAL : NAV_UNITS_METRIC;
#if !DIAGNOSTIC
    waypoints_set_units(&state->collection, (nav_units_t)state->units);
#endif
}

static int dispatch_screen(void *param)
{
    watch_state_t **slot = (watch_state_t **)get_ptr_temp_buf_2();
    watch_state_t *state;
    const struct gesture_ *gesture = param;
    if (!slot || !(state = *slot) || !gesture) return 0;
#if DIAGNOSTIC
    if (gesture->gesture == GESTURE_SWIPE_RIGHT) {
        exit_app(state);
        return 0;
    }
    if (gesture->gesture == GESTURE_SWIPE_LEFT) toggle_units(state);
    else if (gesture->gesture == GESTURE_CLICK) {
        if (gesture->touch_pos_y < 42 && gesture->touch_pos_x < 88) toggle_gps(state);
    } else return 0;
#else
    if (!navigator_gesture(state, gesture)) return 0;
#endif
    draw_screen(state);
    repaint_screen_lines(0, VIDEO_Y);
    return 0;
}

static void key_press_screen(void)
{
    watch_state_t **slot = (watch_state_t **)get_ptr_temp_buf_2();
    if (slot && *slot) exit_app(*slot);
}

static void exit_app(watch_state_t *state)
{
    set_update_period(0, 0);
    if (state && state->gps_on) {
        switch_gps_pressure_sensors(SENSOR_DISABLE);
        state->gps_on = 0;
    }
    show_menu_animate(state ? state->ret_f : (void *)show_watchface,
                      (int)(uintptr_t)show_screen, ANIMATE_RIGHT);
}

#if !DIAGNOSTIC
static int load_waypoint(watch_state_t *state)
{
    uint8_t record[WAYPOINTS_RECORD_SIZE];
    int size, count;
    waypoints_init(&state->collection);
    size = ElfGetSettingsSize(ELF_INDEX_SELF);
    if (size != (int)WAYPOINTS_RECORD_SIZE && size != (int)NAV_WAYPOINT_RECORD_SIZE) return 0;
    _memclr(record, sizeof(record));
    count = ElfReadSettings(ELF_INDEX_SELF, record, 0, size);
    if (count != size || !waypoints_decode(record, (size_t)size, &state->collection)) return 0;
    state->units = state->collection.units;
    sync_selected(state);
    return state->has_waypoint;
}

static void sync_selected(watch_state_t *state)
{
    state->has_waypoint = waypoints_get_selected(&state->collection, &state->waypoint);
}

static int persist_collection(const waypoints_t *next)
{
    uint8_t record[WAYPOINTS_RECORD_SIZE], verify[WAYPOINTS_RECORD_SIZE];
    waypoints_t decoded;
    int written, read;
    if (ElfGetSettingsSize(ELF_INDEX_SELF) != (int)WAYPOINTS_RECORD_SIZE) return 0;
    if (!waypoints_encode(next, record)) return 0;
    written = ElfWriteSettings(ELF_INDEX_SELF, record, 0, WAYPOINTS_RECORD_SIZE);
    if (written < 0) return 0;
    _memclr(verify, sizeof(verify));
    read = ElfReadSettings(ELF_INDEX_SELF, verify, 0, WAYPOINTS_RECORD_SIZE);
    if (read != (int)WAYPOINTS_RECORD_SIZE || _memcmp(record, verify, sizeof(record)) != 0 ||
        !waypoints_decode(verify, sizeof(verify), &decoded)) return 0;
    return 1;
}

static int commit_waypoints(watch_state_t *state, const waypoints_t *next)
{
    if (!persist_collection(next)) {
        state->message = MSG_STORAGE;
        return 0;
    }
    state->collection = *next;
    sync_selected(state);
    return 1;
}

static int occupied(const watch_state_t *state)
{
    return (state->collection.occupied_mask & (1u << state->slot)) != 0;
}

static void waypoint_row(watch_state_t *state, int y)
{
    waypoints_t next = state->collection;
    int used = occupied(state);
    int pending = state->action_pending;
    state->action_pending = ACTION_NONE;

    if (y < 128) {
        if (!used) return;
        if (state->slot == next.selected ||
            (waypoints_select(&next, state->slot) && commit_waypoints(state, &next))) {
            state->page = 0;
            state->message = MSG_NONE;
        }
        return;
    }
    if (y >= 152) {
        if (!used) return;
        if (pending != ACTION_DELETE) {
            state->action_pending = ACTION_DELETE;
            state->action_ticks = CONFIRM_TICKS;
        } else if (waypoints_remove(&next, state->slot) && commit_waypoints(state, &next)) {
            state->message = MSG_DELETED;
        }
        return;
    }
    poll_gps(state);
    if (!state->has_fix) {
        state->message = MSG_GPS;
        return;
    }
    if (used && pending != ACTION_REPLACE) {
        state->action_pending = ACTION_REPLACE;
        state->action_ticks = CONFIRM_TICKS;
        return;
    }
    if (!waypoints_set(&next, state->slot, state->current)) {
        state->message = MSG_GPS;
        return;
    }
    if (!state->has_waypoint) waypoints_select(&next, state->slot);
    if (commit_waypoints(state, &next)) state->message = MSG_SAVED;
}

static void step_slot(watch_state_t *state, unsigned step)
{
    state->slot = (uint8_t)((state->slot + step) % WAYPOINTS_SLOT_COUNT);
    state->action_pending = ACTION_NONE;
    state->message = MSG_NONE;
}

static void open_waypoints(watch_state_t *state, int new_point)
{
    state->page = 1;
    state->slot = state->collection.selected;
    state->message = MSG_NONE;
    state->action_pending = ACTION_NONE;
    if (new_point) {
        for (unsigned i = 0; i < WAYPOINTS_SLOT_COUNT; ++i) {
            if (!(state->collection.occupied_mask & (1u << i))) {
                state->slot = (uint8_t)i;
                return;
            }
        }
        state->message = MSG_FULL;
    }
}

static int navigator_gesture(watch_state_t *state, const struct gesture_ *gesture)
{
    int x = gesture->touch_pos_x, y = gesture->touch_pos_y;
    if (gesture->gesture == GESTURE_SWIPE_RIGHT) {
        if (!state->page) {
            exit_app(state);
            return 0;
        }
        state->page = 0;
        state->action_pending = ACTION_NONE;
        state->message = MSG_NONE;
    } else if (gesture->gesture == GESTURE_SWIPE_LEFT) {
        if (state->page) step_slot(state, 1);
        else toggle_units(state);
    } else if (gesture->gesture == GESTURE_CLICK && x >= 0 && x < 176 && y >= 0 && y < VIDEO_Y) {
        if (state->page) {
            if (y < 32) {
                if (x < 58) step_slot(state, WAYPOINTS_SLOT_COUNT - 1);
                else if (x >= 118) step_slot(state, 1);
                else {
                    state->page = 0;
                    state->action_pending = ACTION_NONE;
                    state->message = MSG_NONE;
                }
            } else if (y >= 104) waypoint_row(state, y);
            else return 0;
        } else if (y < 32) {
            if (x < 88) toggle_gps(state);
            else toggle_units(state);
        } else if (y >= 144) open_waypoints(state, !state->has_waypoint || x >= 88);
        else return 0;
    } else return 0;
    return 1;
}
#endif

static void coord_text(char *out, const char *label, double value)
{
    int32_t e5 = (int32_t)(value * 100000.0 + (value >= 0.0 ? 0.5 : -0.5));
    int32_t mag = e5 < 0 ? -e5 : e5;
    _sprintf(out, "%s %c%ld.%05ld", label, e5 < 0 ? '-' : '+',
             (long)(mag / 100000), (long)(mag % 100000));
}

#if DIAGNOSTIC
static void draw_screen(watch_state_t *state)
{
    char line[40];
    int battery = get_battery_charge();
    set_bg_color(COLOR_BLACK);
    fill_screen_bg();
    GUI_UC_SetEncodeUTF8();
    load_font();
    set_fg_color(COLOR_WHITE);
    text_out_center("BIP DIAGNOSTIC", 88, 4);
    _sprintf(line, "GPS %s", state->gps_on ? "ON" : "OFF");
    text_out(line, 8, 25);
    if (battery >= 0 && battery <= 100) _sprintf(line, "BAT %d%%", battery);
    else _sprintf(line, "BAT N/A");
    text_out(line, 100, 25);
    if (state->unchanged) text_out("FIX NO CHANGE 30S", 8, 45);
    else text_out("AGE UNKNOWN", 8, 45);
    if (state->has_fix) {
        coord_text(line, "LAT", state->current.latitude_deg);
        text_out(line, 8, 65);
        coord_text(line, "LON", state->current.longitude_deg);
        text_out(line, 8, 83);
    } else {
        text_out(state->gps_on ? "WAITING FOR FIX" : "GPS PAUSED", 8, 65);
    }
    text_out(is_gps_fixed() ? "FIX API YES" : "FIX API NO", 8, 106);
    _sprintf(line, "RAW %lu PA", (unsigned long)state->navi.pressure);
    text_out(line, 8, 124);
    _sprintf(line, "READY %08lX", (unsigned long)(uint32_t)state->navi.ready);
    text_out(line, 8, 142);
}
#else
static const char *const slot_names[WAYPOINTS_SLOT_COUNT] = {
    "HOME", "CAR", "CAMP", "BOAT", "POINT 1", "POINT 2"
};

static void show_status(enum nav_status status, int y)
{
    static const char *const labels[] = {
        "GPS PAUSED", "WAITING FOR FIX", "FIX UNCHANGED", "FIX READY"
    };
    set_fg_color(status == GPS_READY ? COLOR_GREEN : COLOR_YELLOW);
    text_out_center(labels[status], 88, y);
}

static void draw_waypoints(const watch_state_t *state, enum nav_status status)
{
    char line[40];
    int used = occupied(state);
    int selected = used && state->slot == state->collection.selected;
    set_fg_color(COLOR_AQUA);
    text_out_center("PREV", 29, 8);
    text_out_center("BACK", 88, 8);
    text_out_center("NEXT", 147, 8);
    set_fg_color(selected ? COLOR_AQUA : COLOR_WHITE);
    _sprintf(line, "%s%s", slot_names[state->slot], selected ? " (ACTIVE)" : "");
    text_out_center(line, 88, 34);
    set_fg_color(COLOR_WHITE);
    if (used) {
        coord_text(line, "LAT", state->collection.coordinates[state->slot].latitude_deg);
        text_out_center(line, 88, 52);
        coord_text(line, "LON", state->collection.coordinates[state->slot].longitude_deg);
        text_out_center(line, 88, 70);
    } else text_out_center("EMPTY", 88, 52);

    if (state->action_pending) {
        int deleting = state->action_pending == ACTION_DELETE;
        set_fg_color(deleting ? COLOR_RED : COLOR_YELLOW);
        text_out_center(deleting ? "TAP DELETE AGAIN" : "TAP REPLACE AGAIN", 88, 88);
    } else if (state->message == MSG_NONE || state->message == MSG_GPS) {
        show_status(status, 88);
    } else {
        const char *label = "STORAGE FAILED";
        int color = COLOR_RED;
        if (state->message == MSG_FULL) { label = "SLOTS FULL"; color = COLOR_YELLOW; }
        else if (state->message == MSG_SAVED) { label = "SAVED"; color = COLOR_GREEN; }
        else if (state->message == MSG_DELETED) { label = "DELETED"; color = COLOR_GREEN; }
        set_fg_color(color);
        text_out_center(label, 88, 88);
    }

    set_fg_color(COLOR_AQUA);
    if (used) text_out_center("SELECT", 88, 108);
    text_out_center(used ? "REPLACE" : "SAVE HERE", 88, 132);
    if (used) text_out_center("DELETE", 88, 156);
}

/* Seven segments use the SDK rectangle primitive and a ten-byte digit table.
 * This avoids loading a second font or keeping a bitmap in RAM. */
static void draw_large_distance(const char *digits)
{
    static const uint8_t digit_segments[] = {
        0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f
    };
    static const uint8_t segments[][4] = {
        {4, 0, 15, 2}, {16, 3, 18, 16}, {16, 20, 18, 33},
        {4, 34, 15, 36}, {1, 20, 3, 33}, {1, 3, 3, 16}, {4, 17, 15, 19}
    };
    int width = 0;
    for (const char *c = digits; *c; ++c) width += *c == '.' ? 8 : 22;
    int x = (176 - width) / 2;
    for (const char *c = digits; *c; ++c) {
        if (*c == '.') {
            draw_filled_rect(x + 2, 105, x + 5, 108);
            x += 8;
        } else {
            uint8_t mask = digit_segments[*c - '0'];
            for (unsigned i = 0; i < 7; ++i) {
                if (mask & (1u << i)) {
                    const uint8_t *r = segments[i];
                    draw_filled_rect(x + r[0], 72 + r[1], x + r[2], 72 + r[3]);
                }
            }
            x += 22;
        }
    }
}

static void draw_main(const watch_state_t *state, const nav_readings_t *readings)
{
    char line[40];
    set_fg_color(COLOR_AQUA);
    text_out_center(state->gps_on ? "GPS ON" : "GPS OFF", 44, 8);
    text_out_center(state->units == NAV_UNITS_METRIC ? "M/KM" : "M/NM", 132, 8);
    show_status((enum nav_status)readings->status, 36);
    set_fg_color(state->has_waypoint ? COLOR_AQUA : COLOR_WHITE);
    if (state->has_waypoint) {
        _sprintf(line, "TO %s", slot_names[state->collection.selected]);
        text_out_center(line, 88, 56);
    } else text_out_center("NO TARGET", 88, 56);
    set_fg_color(COLOR_WHITE);
    if (readings->distance >= 0) {
        long distance = readings->distance;
        if (!readings->large_unit) _sprintf(line, "%ld", distance);
        else _sprintf(line, "%ld.%ld", distance / 10, distance % 10);
        draw_large_distance(line);
        text_out_center(!readings->large_unit ? "M" :
                        state->units == NAV_UNITS_METRIC ? "KM" : "NM", 88, 110);
        if (readings->bearing >= 0) {
            _sprintf(line, "BRG %03d T", (int)readings->bearing);
            text_out_center(line, 88, 129);
        } else text_out_center("NEAR TARGET", 88, 129);
    } else if (readings->status == GPS_UNCHANGED) {
        set_fg_color(COLOR_YELLOW);
        text_out_center("MOVE TO UPDATE", 88, 96);
    }
    set_fg_color(COLOR_AQUA);
    if (state->has_waypoint) {
        text_out_center("WAYPOINTS", 44, 152);
        text_out_center("NEW POINT", 132, 152);
    } else text_out_center("SAVE A POINT", 88, 152);
}

static int draw_navigator(watch_state_t *state, int force)
{
    nav_readings_t next = { .distance = -1, .bearing = -1 };
    next.status = !state->gps_on ? GPS_PAUSED : state->unchanged ? GPS_UNCHANGED :
                  state->has_fix ? GPS_READY : GPS_WAITING;
    if (!state->page && state->has_fix && state->has_waypoint) {
        double distance, bearing, converted;
        if (nav_distance_bearing(state->current, state->waypoint, &distance, &bearing) &&
            nav_convert_distance(distance, (nav_units_t)state->units, &converted)) {
            next.large_unit = distance >= 1000.0;
            if (next.large_unit) {
                converted *= state->units == NAV_UNITS_METRIC ? 0.01 : 10.0;
                next.distance = (int32_t)(converted + 0.5);
            } else next.distance = (int32_t)(distance + 0.5);
            if (next.large_unit || next.distance > NEAR_TARGET_METRES) next.bearing = (int16_t)((int)(bearing + 0.5) % 360);
        }
    }
    /* Interactions force a draw. On a timer, compare only sensor-driven fields
       that are visible on the current page, including rounded readings. */
    if (!force && (state->page ?
        ((state->action_pending || (state->message != MSG_NONE && state->message != MSG_GPS)) ||
         next.status == state->displayed.status) :
        (next.status == state->displayed.status && next.distance == state->displayed.distance &&
         next.large_unit == state->displayed.large_unit && next.bearing == state->displayed.bearing))) return 0;
    state->displayed = next;
    set_bg_color(COLOR_BLACK);
    fill_screen_bg();
    GUI_UC_SetEncodeUTF8();
    load_font();
    if (state->page) draw_waypoints(state, (enum nav_status)next.status);
    else draw_main(state, &next);
    return 1;
}

static void draw_screen(watch_state_t *state)
{
    draw_navigator(state, 1);
}
#endif
