#include <libbip.h>
#include "bip_display.h"
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
#include "waypoint_markers.h"

#define KEYPAD_TOP 64
#define KEY_HEIGHT 28
#define LAST_FIX_TICKS 5100u /* About ten seconds on the BipOS tick clock. */
#define COMPASS_FIX_TICKS 30600u /* Hold unchanged positions for at most one minute. */
#define COURSE_WINDOW_TICKS 15300u /* About thirty seconds to accumulate displacement. */
#define COURSE_HOLD_TICKS 7650u /* Show the last course for about fifteen seconds. */
#define COURSE_MIN_METRES 15.0

enum nav_status { GPS_WAITING, GPS_UNCHANGED, GPS_LAST, GPS_READY, GPS_CALIBRATE };
enum navigator_page {
    PAGE_LEGEND, PAGE_MAIN, PAGE_COMPASS,
    PAGE_WAYPOINTS, PAGE_OPTIONS, PAGE_VARIATION, PAGE_LATITUDE, PAGE_LONGITUDE, PAGE_REVIEW
};
enum waypoint_action { ACTION_NONE, ACTION_REPLACE, ACTION_DELETE };
enum waypoint_message { MSG_NONE, MSG_SAVED, MSG_DELETED, MSG_STORAGE, MSG_GPS, MSG_COORDINATE };

typedef struct {
    char text[10]; /* Three degree digits, a decimal point, five fractional digits. */
    uint8_t length;
    uint8_t fraction_start; /* Offset after the decimal point, or zero. */
    uint8_t negative;
} coordinate_entry_t;

static const char coordinate_keys[] = "123456789.0\b";

typedef struct {
    int32_t distance; /* metres or tenths of the selected larger unit */
    int16_t bearing; /* -1 when unavailable */
    int16_t course; /* GPS course over ground, or -1. */
    int16_t heading; /* Watch heading with any manual correction, or -1. */
    uint8_t large_unit;
    uint8_t status;
} nav_readings_t;

/* The timer compares the complete snapshot without reading padding bytes. */
typedef char nav_readings_without_padding[sizeof(nav_readings_t) == 12 ? 1 : -1];
#else
enum diagnostic_page { DIAG_MENU, DIAG_GPS, DIAG_COMPASS, DIAG_WATCH };
#endif

typedef struct {
    void *ret_f;
    navi_struct_ navi;
    nav_coordinate_t current;
    nav_coordinate_t waypoint;
#if DIAGNOSTIC
    uint8_t page;
    short compass_heading;
    int compass_calibration_needed;
    int compass_heading_valid;
#else
    waypoints_t collection;
    coordinate_entry_t entry[2];
    nav_coordinate_t draft;
    uint32_t last_fix_tick;
    nav_coordinate_t course_origin;
    uint32_t course_tick;
    int16_t course;
    int16_t variation_draft;
    uint8_t course_origin_valid;
    uint8_t last_fix;
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
    uint32_t last_latitude;
    uint32_t last_longitude;
    uint32_t sample_tick;
    int has_waypoint;
    int gps_on;
    int compass_on;
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
static void load_waypoint(watch_state_t *state);
static void sync_selected(watch_state_t *state);
#endif

static void schedule_update(const watch_state_t *state)
{
    set_update_period(1, state->compass_on ? 1000 : UPDATE_MS);
}

#if !DIAGNOSTIC
static void sync_compass(watch_state_t *state)
{
    int enabled = state->page == PAGE_COMPASS ||
                  (state->page < PAGE_COMPASS && state->has_waypoint);
    if (state->compass_on == enabled) return;
    state->compass_on = enabled;
    if (enabled) compass_task_resume();
    else state->course_origin_valid = 0;
    sensors_enable(SENSOR_TYPE_COMPASS, enabled);
}
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
        state->ret_f = (param && proc && proc->elf_finish) ?
                       (void *)proc->elf_finish : (void *)show_watchface;
        state->units = NAV_UNITS_METRIC;
#if DIAGNOSTIC
        state->page = DIAG_MENU;
#else
        state->page = PAGE_MAIN;
        load_waypoint(state);
#endif
#if !DIAGNOSTIC
        switch_gps_pressure_sensors(SENSOR_ENABLE);
        state->gps_on = 1;
#endif
    }
    bip_keep_screen_open();
#if DIAGNOSTIC
    if (state->page == DIAG_GPS) poll_gps(state);
#else
    sync_compass(state);
    poll_gps(state);
#endif
    draw_screen(state);
    schedule_update(state);
}

static void poll_gps(watch_state_t *state)
{
    /* Clear every field first; firmware may leave unavailable fields untouched. */
    _memclr(&state->navi, sizeof(state->navi));
    if (state->gps_on) get_navi_data(&state->navi);
    if (!state->gps_on || !is_gps_fixed() ||
        !IS_NAVI_GPS_LAT_READY(state->navi.ready) ||
        !IS_NAVI_GPS_LONG_READY(state->navi.ready)) goto missing;
    unsigned latitude = (unsigned)state->navi.latitude;
    unsigned longitude = (unsigned)state->navi.longitude;
    int ns = state->navi.ns, ew = state->navi.ew;
    if (latitude > 270000000u || longitude > 540000000u ||
        (ns != NAVI_NORTH_HEMISPHERE && ns != NAVI_SOUTH_HEMISPHERE) ||
        (ew != NAVI_WEST_HEMISPHERE && ew != NAVI_EAST_HEMISPHERE)) goto missing;

    /* Keep the hemisphere bit in the sample key, including at zero degrees. */
    uint32_t lat_sample = latitude | ((uint32_t)(ns == NAVI_SOUTH_HEMISPHERE) << 31);
    uint32_t lon_sample = longitude | ((uint32_t)(ew == NAVI_WEST_HEMISPHERE) << 31);
    uint32_t now = (uint32_t)get_tick_count();
    if (!state->have_sample || state->last_latitude != lat_sample || state->last_longitude != lon_sample) {
        double lat = (double)latitude / 3000000.0;
        double lon = (double)longitude / 3000000.0;
        state->current.latitude_deg = ns == NAVI_SOUTH_HEMISPHERE ? -lat : lat;
        state->current.longitude_deg = ew == NAVI_WEST_HEMISPHERE ? -lon : lon;
        state->last_latitude = lat_sample;
        state->last_longitude = lon_sample;
        state->sample_tick = now;
        state->have_sample = 1;
    }
    state->unchanged = (uint32_t)(now - state->sample_tick) > 15300u;
    state->has_fix = !state->unchanged;
#if !DIAGNOSTIC
    state->last_fix = 0;
    if (state->has_fix) state->last_fix_tick = now;
#endif
    return;

missing:
#if !DIAGNOSTIC
    /* Keep only the last accepted coordinates. GPS saves still require has_fix. */
    state->last_fix = (state->has_fix || state->last_fix) &&
                      (uint32_t)(get_tick_count() - state->last_fix_tick) <= LAST_FIX_TICKS;
#endif
    state->has_fix = 0;
    state->unchanged = 0;
#if !DIAGNOSTIC
    if (state->last_fix) return;
#endif
    state->have_sample = 0;
}

static void screen_job(void)
{
    watch_state_t **slot = (watch_state_t **)get_ptr_temp_buf_2();
    watch_state_t *state;
    if (!slot || !(state = *slot)) return;
#if DIAGNOSTIC
    if (state->page == DIAG_GPS) poll_gps(state);
    if (state->page == DIAG_COMPASS && state->compass_on) {
        state->compass_calibration_needed = compass_need_calibration() != 0;
        state->compass_heading = compass_get_degree();
        state->compass_heading_valid = !state->compass_calibration_needed &&
                                       state->compass_heading >= 0 && state->compass_heading < 360;
    }
    draw_screen(state);
    repaint_screen_lines(0, VIDEO_Y);
#else
    poll_gps(state);
    int expired = state->action_pending && --state->action_ticks == 0;
    if (expired) state->action_pending = ACTION_NONE;
    if (draw_navigator(state, expired)) repaint_screen_lines(0, VIDEO_Y);
#endif
#if DIAGNOSTIC
    if (state->gps_on || state->compass_on) bip_keep_screen_open();
#else
    if (state->gps_on) bip_keep_screen_open();
#endif
    schedule_update(state);
}

#if DIAGNOSTIC
static void stop_diagnostic_sensors(watch_state_t *state)
{
    if (state->gps_on) {
        switch_gps_pressure_sensors(SENSOR_DISABLE);
        state->gps_on = 0;
    }
    if (state->compass_on) {
        sensors_enable(SENSOR_TYPE_COMPASS, SENSOR_DISABLE);
        state->compass_on = 0;
    }
}

static void set_diagnostic_page(watch_state_t *state, enum diagnostic_page page)
{
    stop_diagnostic_sensors(state);
    state->page = (uint8_t)page;
    if (page == DIAG_GPS) {
        state->gps_on = 1;
        state->has_fix = state->have_sample = state->unchanged = 0;
        switch_gps_pressure_sensors(SENSOR_ENABLE);
        poll_gps(state);
    } else if (page == DIAG_COMPASS) {
        compass_task_resume();
        sensors_enable(SENSOR_TYPE_COMPASS, SENSOR_ENABLE);
        state->compass_on = 1;
        state->compass_calibration_needed = compass_need_calibration() != 0;
        state->compass_heading = compass_get_degree();
        state->compass_heading_valid = !state->compass_calibration_needed &&
                                       state->compass_heading >= 0 && state->compass_heading < 360;
    }
    schedule_update(state);
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
#endif

#if !DIAGNOSTIC
static void toggle_units(watch_state_t *state)
{
    state->units = state->units == NAV_UNITS_METRIC ? NAV_UNITS_NAUTICAL : NAV_UNITS_METRIC;
    waypoints_set_units(&state->collection, (nav_units_t)state->units);
}
#endif

static int dispatch_screen(void *param)
{
    watch_state_t **slot = (watch_state_t **)get_ptr_temp_buf_2();
    watch_state_t *state;
    const struct gesture_ *gesture = param;
    if (!slot || !(state = *slot) || !gesture) return 0;
#if DIAGNOSTIC
    enum diagnostic_page page = (enum diagnostic_page)state->page;
    if (gesture->gesture == GESTURE_SWIPE_LEFT) {
        if (page == DIAG_WATCH) return 0;
        ++page;
    } else if (gesture->gesture == GESTURE_SWIPE_RIGHT) {
        if (page == DIAG_MENU) return 0;
        --page;
    } else if (gesture->gesture != GESTURE_CLICK) {
        return 0;
    } else if (page == DIAG_MENU) {
        int y = gesture->touch_pos_y;
        if (y >= 48 && y < 88) page = DIAG_GPS;
        else if (y >= 88 && y < 128) page = DIAG_COMPASS;
        else if (y >= 128 && y < 176) page = DIAG_WATCH;
        else return 0;
    } else if (gesture->touch_pos_y >= 150) {
        page = DIAG_MENU;
    } else if (page == DIAG_GPS && gesture->touch_pos_y < 42 &&
               gesture->touch_pos_x < 88) toggle_gps(state);
    else return 0;
    if (page != state->page) set_diagnostic_page(state, page);
#else
    if (!navigator_gesture(state, gesture)) return 0;
    sync_compass(state);
    schedule_update(state);
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
#if !DIAGNOSTIC
    if (state && state->compass_on) {
        sensors_enable(SENSOR_TYPE_COMPASS, SENSOR_DISABLE);
        state->compass_on = 0;
    }
#endif
#if DIAGNOSTIC
    if (state) stop_diagnostic_sensors(state);
#endif
    if (state && state->gps_on) {
        switch_gps_pressure_sensors(SENSOR_DISABLE);
        state->gps_on = 0;
    }
    show_menu_animate(state ? state->ret_f : (void *)show_watchface,
                      (int)(uintptr_t)show_screen, ANIMATE_RIGHT);
}

#if !DIAGNOSTIC
static void load_waypoint(watch_state_t *state)
{
    uint8_t record[WAYPOINTS_RECORD_SIZE];
    int size, count;
    waypoints_init(&state->collection);
    size = ElfGetSettingsSize(ELF_INDEX_SELF);
    if (size != (int)WAYPOINTS_RECORD_SIZE && size != (int)NAV_WAYPOINT_RECORD_SIZE) return;
    count = ElfReadSettings(ELF_INDEX_SELF, record, 0, size);
    if (count != size || !waypoints_decode(record, (size_t)size, &state->collection)) return;
    state->units = state->collection.units;
    sync_selected(state);
}

static void sync_selected(watch_state_t *state)
{
    unsigned selected = state->collection.selected;
    state->has_waypoint = (state->collection.occupied_mask >> selected) & 1u;
    if (state->has_waypoint) state->waypoint = state->collection.coordinates[selected];
}

static int persist_collection(const waypoints_t *next)
{
    uint8_t record[WAYPOINTS_RECORD_SIZE], verify[WAYPOINTS_RECORD_SIZE];
    int written, read;
    if (ElfGetSettingsSize(ELF_INDEX_SELF) != (int)WAYPOINTS_RECORD_SIZE) return 0;
    if (!waypoints_encode(next, record)) return 0;
    written = ElfWriteSettings(ELF_INDEX_SELF, record, 0, WAYPOINTS_RECORD_SIZE);
    if (written < 0) return 0;
    read = ElfReadSettings(ELF_INDEX_SELF, verify, 0, WAYPOINTS_RECORD_SIZE);
    /* Exact readback verifies the same record that passed the encoder. */
    return read == (int)WAYPOINTS_RECORD_SIZE && _memcmp(record, verify, sizeof(record)) == 0;
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

static void save_waypoint(watch_state_t *state, nav_coordinate_t coordinate)
{
    waypoints_t next = state->collection;
    if (!waypoints_set(&next, state->slot, coordinate)) return;
    if (!state->has_waypoint) waypoints_select(&next, state->slot);
    if (commit_waypoints(state, &next)) {
        state->page = PAGE_WAYPOINTS;
        state->message = MSG_SAVED;
    }
}

static void open_coordinates(watch_state_t *state)
{
    _memclr(state->entry, sizeof(state->entry));
    state->page = PAGE_LATITUDE;
    state->action_pending = ACTION_NONE;
    state->message = MSG_NONE;
}

static void save_current_waypoint(watch_state_t *state, int replace)
{
    int pending = state->action_pending;
    state->action_pending = ACTION_NONE;
    poll_gps(state);
    if (!state->has_fix) {
        state->message = MSG_GPS;
        return;
    }
    if (replace && pending != ACTION_REPLACE) {
        state->action_pending = ACTION_REPLACE;
        state->action_ticks = CONFIRM_TICKS;
        return;
    }
    save_waypoint(state, state->current);
}

static void waypoint_row(watch_state_t *state, int y)
{
    if (!occupied(state)) {
        if (y < 140) save_current_waypoint(state, 0);
        else open_coordinates(state);
    } else if (y >= 140) {
        state->page = PAGE_OPTIONS;
        state->message = MSG_NONE;
    } else if (state->slot != state->collection.selected) {
        waypoints_t next = state->collection;
        if (waypoints_select(&next, state->slot) && commit_waypoints(state, &next)) {
            state->page = PAGE_MAIN;
            state->message = MSG_NONE;
        }
    }
}

static void waypoint_option(watch_state_t *state, int y)
{
    if (y < 96) {
        save_current_waypoint(state, 1);
    } else if (y < 136) {
        open_coordinates(state);
    } else if (state->action_pending != ACTION_DELETE) {
        state->action_pending = ACTION_DELETE;
        state->action_ticks = CONFIRM_TICKS;
    } else {
        waypoints_t next = state->collection;
        state->action_pending = ACTION_NONE;
        if (waypoints_remove(&next, state->slot) && commit_waypoints(state, &next)) {
            state->page = PAGE_WAYPOINTS;
            state->message = MSG_DELETED;
        }
    }
}

static void step_slot(watch_state_t *state, unsigned step)
{
    state->slot = (uint8_t)((state->slot + step) % WAYPOINTS_SLOT_COUNT);
    state->action_pending = ACTION_NONE;
    state->message = MSG_NONE;
}

static void close_waypoint_page(watch_state_t *state)
{
    state->page = state->page > PAGE_WAYPOINTS ? PAGE_WAYPOINTS : PAGE_MAIN;
    state->action_pending = ACTION_NONE;
    state->message = MSG_NONE;
}

static int read_heading(int variation)
{
    if (compass_need_calibration()) return -1;
    int heading = compass_get_degree();
    return (unsigned)heading < 360 ? (heading + variation + 360) % 360 : -1;
}

static int gps_course(watch_state_t *state)
{
    if (!state->has_fix) {
        state->course_origin_valid = 0;
        return -1;
    }

    uint32_t age = (uint32_t)(get_tick_count() - state->course_tick);
    if (!state->course_origin_valid || age > COURSE_WINDOW_TICKS) {
        state->course = -1;
    } else {
        double distance, bearing;
        if (state->sample_tick != state->course_tick &&
            nav_distance_bearing(state->course_origin, state->current, &distance, &bearing) &&
            distance >= COURSE_MIN_METRES) {
            state->course = (int16_t)((int)(bearing + 0.5) % 360);
        } else return age <= COURSE_HOLD_TICKS ? state->course : -1;
    }
    state->course_origin = state->current;
    state->course_tick = state->sample_tick;
    state->course_origin_valid = 1;
    return state->course;
}

static void enter_coordinate_key(coordinate_entry_t *entry, char key)
{
    if (key == '\b') {
        if (!entry->length) return;
        if (entry->length == entry->fraction_start) entry->fraction_start = 0;
        entry->text[--entry->length] = '\0';
        return;
    }
    if (key == '.') {
        if (entry->fraction_start) return;
        entry->fraction_start = entry->length + 1;
    } else if (entry->fraction_start ? entry->length - entry->fraction_start == 5 : entry->length == 3) {
        return;
    }
    entry->text[entry->length++] = key;
    entry->text[entry->length] = '\0';
}

static int read_coordinate(const coordinate_entry_t *entry, int limit, double *value)
{
    int32_t magnitude = 0, scale = 1;
    if (!entry->length || (entry->length == 1 && entry->text[0] == '.')) return 0;
    for (unsigned i = 0; i < entry->length; ++i) {
        if (entry->text[i] == '.') continue;
        magnitude = magnitude * 10 + entry->text[i] - '0';
        if (entry->fraction_start && i >= entry->fraction_start) scale *= 10;
    }
    if (magnitude > limit * scale) return 0;
    *value = (entry->negative ? -magnitude : magnitude) / (double)scale;
    return 1;
}

static int coordinate_gesture(watch_state_t *state, const struct gesture_ *gesture)
{
    int x = gesture->touch_pos_x, y = gesture->touch_pos_y;
    int tap = gesture->gesture == GESTURE_CLICK && x >= 0 && x < 176 && y >= 0 && y < VIDEO_Y;
    if (gesture->gesture == GESTURE_SWIPE_RIGHT || (tap && x < 58 && y < 26)) {
        if (state->page == PAGE_LATITUDE)
            state->page = occupied(state) ? PAGE_OPTIONS : PAGE_WAYPOINTS;
        else --state->page;
        state->message = MSG_NONE;
    } else if (!tap) {
        return 0;
    } else if (state->page == PAGE_REVIEW) {
        if (y < 144) return 0;
        save_waypoint(state, state->draft);
    } else {
        int longitude = state->page == PAGE_LONGITUDE;
        coordinate_entry_t *entry = &state->entry[longitude];
        state->message = MSG_NONE;
        if (y < 26) {
            if (x < 118) entry->negative = !entry->negative;
            else if (read_coordinate(entry, longitude ? 180 : 90,
                      longitude ? &state->draft.longitude_deg : &state->draft.latitude_deg)) ++state->page;
            else state->message = MSG_COORDINATE;
        } else if (y >= KEYPAD_TOP) {
            int column = x < 58 ? 0 : x < 118 ? 1 : 2;
            enter_coordinate_key(entry, coordinate_keys[(y - KEYPAD_TOP) / KEY_HEIGHT * 3 + column]);
        } else return 0;
    }
    return 1;
}

static int variation_tap(watch_state_t *state, int x, int y)
{
    if (y < 32) {
        close_waypoint_page(state);
    } else if (y >= 136) {
        waypoints_t next = state->collection;
        next.variation = state->variation_draft;
        if (next.variation == state->collection.variation || commit_waypoints(state, &next))
            close_waypoint_page(state);
    } else if (y >= 91 && y < 119) {
        state->variation_draft = 0;
        state->message = MSG_NONE;
    } else if (y >= 51 && y < 91 && (x < 58 || x >= 118)) {
        int next = state->variation_draft + (x < 58 ? -1 : 1);
        if (next >= -180 && next <= 180) state->variation_draft = (int16_t)next;
        state->message = MSG_NONE;
    } else return 0;
    return 1;
}

static int navigator_gesture(watch_state_t *state, const struct gesture_ *gesture)
{
    int x = gesture->touch_pos_x, y = gesture->touch_pos_y;
    if (state->page >= PAGE_LATITUDE) return coordinate_gesture(state, gesture);
    if (gesture->gesture == GESTURE_SWIPE_RIGHT) {
        if (state->page == PAGE_LEGEND) return 0;
        if (state->page <= PAGE_COMPASS) --state->page;
        else close_waypoint_page(state);
    } else if (gesture->gesture == GESTURE_SWIPE_LEFT) {
        if (state->page < PAGE_COMPASS) ++state->page;
        else if (state->page == PAGE_WAYPOINTS) step_slot(state, 1);
        else return 0;
    } else if (gesture->gesture == GESTURE_CLICK && x >= 0 && x < 176 && y >= 0 && y < VIDEO_Y) {
        if (state->page == PAGE_OPTIONS) {
            if (y < 32) close_waypoint_page(state);
            else if (y >= 56) waypoint_option(state, y);
            else return 0;
        } else if (state->page == PAGE_VARIATION) {
            return variation_tap(state, x, y);
        } else if (state->page == PAGE_WAYPOINTS) {
            if (y < 32) {
                if (x < 44) step_slot(state, WAYPOINTS_SLOT_COUNT - 1);
                else if (x >= 132) step_slot(state, 1);
                else if (x < 94) close_waypoint_page(state);
                else {
                    state->variation_draft = state->collection.variation;
                    state->page = PAGE_VARIATION;
                    state->message = MSG_NONE;
                }
            } else if (y >= 104) waypoint_row(state, y);
            else return 0;
        } else if (y >= 141) {
            if (state->page != PAGE_MAIN) state->page = PAGE_MAIN;
            else {
                state->page = PAGE_WAYPOINTS;
                state->slot = state->has_waypoint ? state->collection.selected : 0;
            }
        } else if (state->page == PAGE_MAIN && y < 36) toggle_units(state);
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
    set_bg_color(COLOR_BLACK);
    fill_screen_bg();
    GUI_UC_SetEncodeUTF8();
    load_font();
    set_fg_color(COLOR_WHITE);
    if (state->page == DIAG_MENU) {
        text_out_center("DIAGNOSTICS", 88, 10);
        set_fg_color(COLOR_AQUA);
        draw_filled_rect(16, 47, 159, 47);
        draw_filled_rect(16, 87, 159, 87);
        draw_filled_rect(16, 127, 159, 127);
        text_out_center("GPS", 88, 61);
        text_out_center("COMPASS", 88, 101);
        text_out_center("WATCH", 88, 141);
    } else if (state->page == DIAG_GPS) {
        int battery = get_battery_charge();
        text_out_center("GPS DIAGNOSTIC", 88, 4);
        _sprintf(line, "GPS %s", state->gps_on ? "ON" : "OFF");
        text_out(line, 8, 25);
        if (battery >= 0 && battery <= 100) _sprintf(line, "BAT %d%%", battery);
        else _sprintf(line, "BAT N/A");
        text_out(line, 100, 25);
        if (state->unchanged) text_out("FIX NO CHANGE 30S", 8, 45);
        else text_out("AGE UNKNOWN", 8, 45);
        if (state->has_fix) {
            coord_text(line, "LAT", state->current.latitude_deg);
            text_out(line, 8, 63);
            coord_text(line, "LON", state->current.longitude_deg);
            text_out(line, 8, 80);
        } else {
            text_out(state->gps_on ? "WAITING FOR FIX" : "GPS PAUSED", 8, 65);
        }
        text_out(is_gps_fixed() ? "FIX API YES" : "FIX API NO", 8, 98);
        _sprintf(line, "RAW %lu PA", (unsigned long)state->navi.pressure);
        text_out(line, 8, 115);
        _sprintf(line, "READY %08lX", (unsigned long)(uint32_t)state->navi.ready);
        text_out(line, 8, 132);
        set_fg_color(COLOR_AQUA);
        text_out_center("BACK", 88, 154);
    } else if (state->page == DIAG_COMPASS) {
        text_out_center("COMPASS CHECK", 88, 12);
        text_out_center("RAW HEADING", 88, 40);
        if (state->compass_calibration_needed) {
            text_out_center("CALIBRATION NEEDED", 88, 68);
            text_out_center("REF UNVERIFIED", 88, 94);
        } else if (state->compass_heading_valid) {
            _sprintf(line, "%d deg", (int)state->compass_heading);
            text_out_center(line, 88, 68);
            text_out_center("REF UNVERIFIED", 88, 94);
        } else {
            text_out_center("UNAVAILABLE", 88, 68);
            text_out_center("REF UNVERIFIED", 88, 94);
        }
        set_fg_color(COLOR_AQUA);
        text_out_center("BACK", 88, 154);
    } else {
        int battery = get_battery_charge();
        int firmware = get_fw_version();
        int bipos_build = get_bipos_build();
        int charging = IS_CHARGE_PLUGGED;
        text_out_center("WATCH INFO", 88, 12);
        if (firmware == 55555) _sprintf(line, "FW UNIVERSAL");
        else if (firmware >= 10000 && firmware <= 99999)
            _sprintf(line, "FW %d.%d.%d.%02d", firmware / 10000,
                     firmware / 1000 % 10, firmware / 100 % 10, firmware % 100);
        else _sprintf(line, "FW N/A");
        text_out_center(line, 88, 36);
        if (bipos_build >= 0) _sprintf(line, "BIPOS BUILD %d", bipos_build);
        else _sprintf(line, "BIPOS BUILD N/A");
        text_out_center(line, 88, 56);
        int free_ram = get_var_free_ram();
        if (free_ram >= 0) _sprintf(line, "FREE RAM %d B", free_ram);
        else _sprintf(line, "FREE RAM N/A");
        text_out_center(line, 88, 78);
        if (battery >= 0 && battery <= 100) _sprintf(line, "BATTERY %d%%", battery);
        else _sprintf(line, "BATTERY N/A");
        text_out_center(line, 88, 100);
        text_out_center(charging ? "CHARGING YES" : "CHARGING NO", 88, 122);
        set_fg_color(COLOR_AQUA);
        text_out_center("BACK", 88, 154);
    }
}
#else
static const char slot_names[WAYPOINTS_SLOT_COUNT][8] = {
    "HOME", "CAR", "CAMP", "BOAT", "POINT 1", "POINT 2"
};

static void show_status(enum nav_status status, int y)
{
    static const char labels[][16] = {
        "WAITING FOR FIX", "FIX UNCHANGED", "LAST FIX", "FIX READY", "CHECK COMPASS"
    };
    set_fg_color(status == GPS_READY ? COLOR_GREEN : COLOR_YELLOW);
    text_out_center(labels[status], 88, y);
}

static void draw_waypoint_status(const watch_state_t *state, enum nav_status status, int y)
{
    if (state->action_pending) {
        int deleting = state->action_pending == ACTION_DELETE;
        set_fg_color(deleting ? COLOR_RED : COLOR_YELLOW);
        text_out_center(deleting ? "TAP DELETE AGAIN" : "TAP REPLACE AGAIN", 88, y);
    } else if (state->message == MSG_NONE || state->message == MSG_GPS) {
        show_status(status, y);
    } else {
        set_fg_color(state->message == MSG_STORAGE ? COLOR_RED : COLOR_GREEN);
        text_out_center(state->message == MSG_STORAGE ? "STORAGE FAILED" :
                        state->message == MSG_SAVED ? "SAVED" : "DELETED", 88, y);
    }
}

static void draw_waypoint_action(const char *label, int top, int color)
{
    set_fg_color(color);
    draw_filled_rect(8, top, 167, top);
    text_out_center(label, 88, top + 12);
}

static void draw_waypoints(const watch_state_t *state, enum nav_status status)
{
    char line[40];
    int used = occupied(state);
    int selected = used && state->slot == state->collection.selected;
    set_fg_color(COLOR_AQUA);
    text_out_center("PREV", 22, 8);
    text_out_center("BACK", 69, 8);
    text_out_center("SET", 113, 8);
    text_out_center("NEXT", 154, 8);
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

    draw_waypoint_status(state, status, 88);
    if (!used || !selected) draw_waypoint_action(used ? "SELECT" : "SAVE HERE", 104, COLOR_AQUA);
    draw_waypoint_action(used ? "OPTIONS" : "COORDS", 140, COLOR_AQUA);
}

static void draw_waypoint_options(const watch_state_t *state, enum nav_status status)
{
    set_fg_color(COLOR_AQUA);
    text_out_center("BACK", 88, 8);
    if (state->action_pending || state->message != MSG_NONE) draw_waypoint_status(state, status, 34);
    else {
        set_fg_color(COLOR_WHITE);
        text_out_center(slot_names[state->slot], 88, 34);
    }
    draw_waypoint_action("REPLACE HERE", 56, COLOR_AQUA);
    draw_waypoint_action("COORDS", 96, COLOR_AQUA);
    draw_waypoint_action("DELETE", 136, COLOR_RED);
}

static void draw_variation(const watch_state_t *state)
{
    char value[12];
    int variation = state->variation_draft;
    set_fg_color(COLOR_AQUA);
    text_out_center("BACK", 88, 8);
    text_out_center("-", 29, 67);
    text_out_center("+", 147, 67);
    set_fg_color(COLOR_WHITE);
    text_out_center("VARIATION", 88, 34);
    if (variation) _sprintf(value, "%d deg %c", variation < 0 ? -variation : variation, variation < 0 ? 'W' : 'E');
    text_out_center(variation ? value : "OFF", 88, 67);
    draw_waypoint_action("RESET OFF", 91, COLOR_AQUA);
    if (state->message == MSG_STORAGE) {
        set_fg_color(COLOR_RED);
        text_out_center("STORAGE FAILED", 88, 119);
    }
    draw_waypoint_action("SAVE", 136, COLOR_AQUA);
}

static void draw_coordinate_editor(const watch_state_t *state)
{
    char line[24];
    set_fg_color(COLOR_AQUA);
    text_out_center("BACK", 29, 6);
    if (state->page == PAGE_REVIEW) {
        text_out_center("CHECK POINT", 88, 31);
        text_out_center(slot_names[state->slot], 88, 52);
        set_fg_color(COLOR_WHITE);
        coord_text(line, "LAT", state->draft.latitude_deg);
        text_out_center(line, 88, 74);
        coord_text(line, "LON", state->draft.longitude_deg);
        text_out_center(line, 88, 94);
        if (state->message == MSG_STORAGE) {
            set_fg_color(COLOR_RED);
            text_out_center("STORAGE FAILED", 88, 116);
        }
        set_fg_color(occupied(state) ? COLOR_YELLOW : COLOR_GREEN);
        text_out_center(occupied(state) ? "REPLACE POINT" : "SAVE POINT", 88, 152);
        return;
    }

    int longitude = state->page == PAGE_LONGITUDE;
    const coordinate_entry_t *entry = &state->entry[longitude];
    char hemisphere = longitude ? (entry->negative ? 'W' : 'E') : (entry->negative ? 'S' : 'N');
    text_out_center(longitude ? "E/W" : "N/S", 88, 6);
    text_out_center("NEXT", 147, 6);
    set_fg_color(COLOR_WHITE);
    _sprintf(line, "%s %s %c", longitude ? "LON" : "LAT", entry->length ? entry->text : "_", hemisphere);
    text_out_center(line, 88, 27);
    if (state->message == MSG_COORDINATE) {
        set_fg_color(COLOR_YELLOW);
        text_out_center(longitude ? "ENTER 0 TO 180" : "ENTER 0 TO 90", 88, 46);
    }
    set_fg_color(COLOR_AQUA);
    draw_filled_rect(57, KEYPAD_TOP, 57, 175);
    draw_filled_rect(117, KEYPAD_TOP, 117, 175);
    for (int y = KEYPAD_TOP - 1; y < 175; y += KEY_HEIGHT) draw_filled_rect(0, y, 175, y);
    for (int i = 0; i < 12; ++i) {
        char key[2] = {coordinate_keys[i], '\0'};
        text_out_center(key[0] == '\b' ? "DEL" : key, 29 + i % 3 * 59,
                        KEYPAD_TOP + 6 + i / 3 * KEY_HEIGHT);
    }
}

/* Seven segments use the SDK rectangle primitive and a ten-byte digit table.
 * This avoids loading a second font or keeping a bitmap in RAM. */
static int draw_large_distance(const char *digits, const char *unit)
{
    static const uint8_t digit_segments[] = {
        0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f
    };
    static const uint8_t segments[][4] = {
        {4, 0, 15, 2}, {16, 3, 18, 16}, {16, 20, 18, 33},
        {4, 34, 15, 36}, {1, 20, 3, 33}, {1, 3, 3, 16}, {4, 17, 15, 19}
    };
    const int top = 82;
    const int unit_gap = 6;
    int unit_width = unit[1] ? 18 : 9;
    int width = unit_gap + unit_width;
    for (const char *c = digits; *c; ++c) width += *c == '.' ? 8 : 22;
    if (width > 140) return 0;
    int x = (176 - width) / 2;
    for (const char *c = digits; *c; ++c) {
        if (*c == '.') {
            draw_filled_rect(x + 2, top + 33, x + 5, top + 36);
            x += 8;
        } else {
            uint8_t mask = digit_segments[*c - '0'];
            for (unsigned i = 0; i < 7; ++i) {
                if (mask & (1u << i)) {
                    const uint8_t *r = segments[i];
                    draw_filled_rect(x + r[0], top + r[1], x + r[2], top + r[3]);
                }
            }
            x += 22;
        }
    }
    text_out_center(unit, x + unit_gap + unit_width / 2, top + 21);
    return 1;
}

static void draw_legend(void)
{
    draw_direction_arrow(0, 25, 40, 7, COLOR_GREEN);
    draw_direction_arrow(6, 25, 67, 7, COLOR_YELLOW);
    draw_north_badge(25, 106);
    set_fg_color(COLOR_WHITE);
    text_out_center("LEGEND", 88, 8);
    text_out_center("WAYPOINT", 100, 32);
    text_out_center("GPS TRACK", 100, 59);
    text_out_center("WHILE MOVING", 100, 77);
    text_out_center("NORTH", 100, 98);
    set_fg_color(COLOR_GREEN);
    text_out_center("TURN GREEN UP", 88, 120);
}

static void draw_compass(const watch_state_t *state, const nav_readings_t *readings)
{
    char heading[18];
    set_fg_color(COLOR_WHITE);
    text_out_center("COMPASS", 88, 18);
    if (readings->heading < 0) {
        show_status(GPS_CALIBRATE, 37);
        return;
    }
    _sprintf(heading, "HDG %03d %s", (int)readings->heading,
             state->collection.variation ? "T" : "RAW");
    text_out_center(heading, 88, 37);
    for (int slot = 0; slot < 32; slot += 4) {
        int x, y;
        edge_vector(slot, slot & 4 ? 22 : 32, &x, &y);
        draw_filled_rect(87 + x, 88 + y, 88 + x, 89 + y);
    }
    if (readings->bearing >= 0)
        draw_direction_arrow(edge_slot(readings->bearing, readings->heading), 87, 88, 24, COLOR_GREEN);
}

static void draw_navigation(const watch_state_t *state, const nav_readings_t *readings)
{
    int compass = state->page == PAGE_COMPASS;
    char line[24];
    if (compass) draw_compass(state, readings);
    else {
        set_fg_color(COLOR_AQUA);
        text_out_center(state->units == NAV_UNITS_METRIC ? "M/KM" : "M/NM", 88, 18);
        show_status((enum nav_status)readings->status, 37);
        if (state->has_waypoint) {
            set_fg_color(COLOR_AQUA);
            _sprintf(line, "TO %s", slot_names[state->collection.selected]);
            text_out_center(line, 88, 56);
        } else {
            set_fg_color(COLOR_WHITE);
            text_out_center("NO TARGET", 88, 56);
        }
    }
    set_fg_color(COLOR_WHITE);
    if (readings->distance >= 0) {
        long distance = readings->distance;
        const char *unit = !readings->large_unit ? "M" : state->units == NAV_UNITS_METRIC ? "KM" : "NM";
        if (readings->large_unit) _sprintf(line, "%ld.%ld", distance / 10, distance % 10);
        else _sprintf(line, "%ld", distance);
        if (compass || !draw_large_distance(line, unit)) {
            char summary[24];
            int last = compass && readings->status == GPS_LAST;
            if (last) set_fg_color(COLOR_YELLOW);
            _sprintf(summary, "%s %s%s", line, unit, last ? " LAST" : "");
            text_out_center(summary, 88, compass ? 123 : 92);
        }
        if (!compass) {
            if (readings->bearing >= 0) {
                _sprintf(line, "BRG %03d T", (int)readings->bearing);
                text_out_center(line, 88, 123);
            } else text_out_center("NEAR TARGET", 88, 123);
        }
    } else if (compass) {
        if (!state->has_waypoint) text_out_center("NO TARGET", 88, 123);
        else show_status((enum nav_status)readings->status, 123);
    } else if (readings->status == GPS_UNCHANGED) {
        set_fg_color(COLOR_YELLOW);
        text_out_center("MOVE TO UPDATE", 88, 96);
    }
    if (readings->heading >= 0)
        draw_edge_markers(compass ? -1 : readings->bearing, readings->course, readings->heading);
}

static int draw_navigator(watch_state_t *state, int force)
{
    nav_readings_t next = { .distance = -1, .bearing = -1, .course = -1, .heading = -1 };
    next.status = state->has_fix ? GPS_READY : state->last_fix ? GPS_LAST :
                  state->unchanged ? GPS_UNCHANGED : GPS_WAITING;
    if (state->compass_on) {
        next.heading = (int16_t)read_heading(state->collection.variation);
        next.course = (int16_t)gps_course(state);
        if (next.heading >= 0 && state->unchanged &&
            (uint32_t)(get_tick_count() - state->sample_tick) <= COMPASS_FIX_TICKS)
            next.status = GPS_LAST;
    }
    if (state->page == PAGE_LEGEND && !force) return 0;
    if (state->page <= PAGE_COMPASS && next.status >= GPS_LAST && state->has_waypoint) {
        double distance, bearing;
        if (nav_distance_bearing(state->current, state->waypoint, &distance, &bearing)) {
            next.large_unit = distance >= 1000.0;
            if (next.large_unit) distance *= state->units == NAV_UNITS_METRIC ? 0.01 : (10.0 / 1852.0);
            next.distance = (int32_t)(distance + 0.5);
            if (next.large_unit || next.distance > NEAR_TARGET_METRES) next.bearing = (int16_t)((int)(bearing + 0.5) % 360);
        }
    }
    if (state->compass_on && next.heading < 0 && next.status == GPS_READY) next.status = GPS_CALIBRATE;
    /* Interactions force a draw. On a timer, compare only sensor-driven fields
       that are visible on the current page, including rounded readings. */
    if (!force) {
        if (state->page <= PAGE_COMPASS) {
            if (_memcmp(&next, &state->displayed, sizeof(next)) == 0) return 0;
        } else if (state->page >= PAGE_VARIATION || state->action_pending ||
                   (state->message != MSG_NONE && state->message != MSG_GPS) ||
                   next.status == state->displayed.status) return 0;
    }
    state->displayed = next;
    set_bg_color(COLOR_BLACK);
    fill_screen_bg();
    GUI_UC_SetEncodeUTF8();
    load_font();
    if (state->page >= PAGE_LATITUDE) draw_coordinate_editor(state);
    else if (state->page == PAGE_VARIATION) draw_variation(state);
    else if (state->page == PAGE_OPTIONS) draw_waypoint_options(state, (enum nav_status)next.status);
    else if (state->page == PAGE_WAYPOINTS) draw_waypoints(state, (enum nav_status)next.status);
    else {
        if (state->page == PAGE_LEGEND) draw_legend();
        else draw_navigation(state, &next);
        set_fg_color(COLOR_AQUA);
        text_out_center(state->page == PAGE_MAIN ? "WAYPOINTS" : "BACK", 88, 141);
    }
    return 1;
}

static void draw_screen(watch_state_t *state)
{
    draw_navigator(state, 1);
}
#endif
