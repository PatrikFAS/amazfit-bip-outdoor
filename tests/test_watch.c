#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define main watch_app_entry
#include "../src/watch_app.c"
#undef main

static watch_state_t *active_state;
static Elf_proc_ fake_proc;
static navi_struct_ fake_navi;
static int fake_fixed, fake_tick, fake_settings_size, fake_sensor_mode;
static short fake_compass_degree;
static int fake_compass_calibration, fake_compass_resume_calls;
static int fake_compass_read_calls, fake_compass_calibration_calls;
static int fake_compass_sensor_calls, fake_compass_sensor_enabled;
static int fake_write_result, fake_read_result, fake_fail_readback;
static int fake_update_enabled, fake_update_period, fake_animated_exit;
static int fake_free_ram;
static int fake_fw_version, fake_bipos_build, fake_charging;
static int fake_display_state_calls, fake_display_state_parameters[128], fake_display_state_values[128];
static int fake_overlay, fake_screen_registrations;
static int fake_settings_size_calls, fake_settings_read_calls, fake_settings_write_calls;
#ifdef WATCH_PREVIEW
static int capture_screen;
#endif

#if !DIAGNOSTIC
static int near(double a, double b, double epsilon)
{
    return fabs(a - b) <= epsilon;
}
#endif
static unsigned char fake_settings[256];
static struct {
    char text[40];
    int color;
} drawn[20];
static int draw_count, foreground, screen_clears, repaints, polls, rectangles;
static int red_rectangles, green_rectangles, yellow_rectangles;
static int red_xsum, red_ysum, green_xsum, green_ysum, yellow_xsum, yellow_ysum;

#if !DIAGNOSTIC
static int drawn_color(const char *text)
{
    for (int i = 0; i < draw_count; ++i)
        if (strcmp(drawn[i].text, text) == 0) return drawn[i].color;
    return -1;
}
#endif

#if DIAGNOSTIC
static int drawn_has(const char *text)
{
    for (int i = 0; i < draw_count; ++i)
        if (strcmp(drawn[i].text, text) == 0) return 1;
    return 0;
}
#endif

static void capture_text(const char *text)
{
    assert(draw_count < (int)(sizeof(drawn) / sizeof(drawn[0])));
    snprintf(drawn[draw_count].text, sizeof(drawn[draw_count].text), "%s", text);
    drawn[draw_count++].color = foreground;
}

void *_memclr(void *ptr, size_t size) { return memset(ptr, 0, size); }
int _memcmp(const void *a, const void *b, size_t size) { return memcmp(a, b, size); }
int _sprintf(char *out, const char *format, ...)
{
    int result;
    va_list args;
    va_start(args, format); result = vsprintf(out, format, args); va_end(args);
    return result;
}
void *pvPortMalloc(size_t size)
{
    static watch_state_t state;
    assert(size == sizeof(state));
    memset(&state, 0, sizeof(state));
    active_state = &state;
    return &state;
}
void *get_ptr_temp_buf_2(void) { return &active_state; }
int get_var_menu_overlay(void) { return fake_overlay; }
void reg_menu(void *menu, int param) { (void)menu; (void)param; ++fake_screen_registrations; }
Elf_proc_ *get_proc_by_addr(void *addr) { (void)addr; return &fake_proc; }
void show_watchface(void) {}
void switch_gps_pressure_sensors(int mode) { fake_sensor_mode = mode; }
int sensors_enable(int sensor_type, int enable)
{
    assert(sensor_type == SENSOR_TYPE_COMPASS);
    ++fake_compass_sensor_calls;
    fake_compass_sensor_enabled = enable;
    return 0;
}
void compass_task_resume(void) { ++fake_compass_resume_calls; }
short compass_get_degree(void) { ++fake_compass_read_calls; return fake_compass_degree; }
int compass_need_calibration(void)
{ ++fake_compass_calibration_calls; return fake_compass_calibration; }
void get_navi_data(navi_struct_ *data) { *data = fake_navi; ++polls; }
int is_gps_fixed(void) { return fake_fixed; }
unsigned int get_tick_count(void) { return (unsigned int)fake_tick; }
void set_update_period(int enable, int period)
{ fake_update_enabled = enable; fake_update_period = period; }
int set_display_state_value(int parameter, int value)
{
    if (fake_display_state_calls < 128) {
        fake_display_state_parameters[fake_display_state_calls] = parameter;
        fake_display_state_values[fake_display_state_calls] = value;
    }
    ++fake_display_state_calls;
    return 0;
}
int get_battery_charge(void) { return 76; }
int get_var_free_ram(void) { return fake_free_ram; }
int get_fw_version(void) { return fake_fw_version; }
int get_bipos_build(void) { return fake_bipos_build; }
int check_app_state(long long state)
{
    assert(state == APP_STATE_CHARGE_PLUGGED);
    return fake_charging;
}
void set_bg_color(int color) { (void)color; }
void fill_screen_bg(void)
{
    ++screen_clears; draw_count = rectangles = 0;
    red_rectangles = green_rectangles = yellow_rectangles = 0;
    red_xsum = red_ysum = green_xsum = green_ysum = yellow_xsum = yellow_ysum = 0;
}
void draw_filled_rect(int x1, int y1, int x2, int y2)
{
    assert(x1 >= 0 && x1 <= x2 && x2 < 176);
    assert(y1 >= 0 && y1 <= y2 && y2 < 176);
    ++rectangles;
    if (foreground == COLOR_RED) {
        ++red_rectangles;
        red_xsum += x1 + x2;
        red_ysum += y1 + y2;
    }
    if (foreground == COLOR_YELLOW) {
        ++yellow_rectangles;
        yellow_xsum += x1 + x2;
        yellow_ysum += y1 + y2;
    }
    if (foreground == COLOR_GREEN) {
        ++green_rectangles;
        green_xsum += x1 + x2;
        green_ysum += y1 + y2;
    }
#ifdef WATCH_PREVIEW
    if (capture_screen) printf("R\t%d\t%d\t%d\t%d\n", x1, y1, x2, y2);
#endif
}
void set_graph_callback_to_ram_1(void) {}
void load_font(void) {}
void set_fg_color(int color)
{
    foreground = color;
#ifdef WATCH_PREVIEW
    if (capture_screen) printf("F\t%06X\n", (unsigned)color);
#endif
}
void GUI_UC_SetEncodeUTF8(void) {}
void text_out_center(const char *text, int x, int y)
{
    capture_text(text);
#ifdef WATCH_PREVIEW
    if (capture_screen) printf("C\t%d\t%d\t%s\n", x, y, text);
#else
    (void)text; (void)x; (void)y;
#endif
}
void text_out(const char *text, int x, int y)
{
    assert(x >= 0); assert(y >= 0 && y <= 160);
    capture_text(text);
#ifdef WATCH_PREVIEW
    if (capture_screen) printf("T\t%d\t%d\t%s\n", x, y, text);
#endif
}
void repaint_screen_lines(int start, int end) { (void)start; (void)end; ++repaints; }
int show_menu_animate(void *fn, int param, int animation)
{ (void)fn; (void)param; (void)animation; ++fake_animated_exit; return 0; }
int ElfGetSettingsSize(int index)
{ (void)index; ++fake_settings_size_calls; return fake_settings_size; }
int ElfReadSettings(int index, void *buffer, int offset, int length)
{
    int count = fake_read_result;
    (void)index;
    ++fake_settings_read_calls;
    if (count > length) count = length;
    if (count > 0) memcpy(buffer, fake_settings + offset, (size_t)count);
    if (fake_fail_readback) {
        fake_fail_readback = 0;
        if (length > 0) ((unsigned char *)buffer)[0] ^= 1u;
    }
    return count;
}
int ElfWriteSettings(int index, const void *buffer, int offset, int length)
{
    int count = fake_write_result;
    (void)index;
    ++fake_settings_write_calls;
    if (count > length) count = length;
    if (count > 0) memcpy(fake_settings + offset, buffer, (size_t)count);
    return count;
}

static void reset_fakes(void)
{
    memset(&fake_navi, 0, sizeof(fake_navi));
    draw_count = screen_clears = repaints = polls = rectangles = 0;
    red_rectangles = green_rectangles = yellow_rectangles = 0;
    memset(fake_settings, 0, sizeof(fake_settings));
    fake_fixed = 1; fake_tick = 1000; fake_settings_size = WAYPOINTS_RECORD_SIZE;
    fake_sensor_mode = -1; fake_write_result = WAYPOINTS_RECORD_SIZE;
    fake_compass_degree = 0; fake_compass_calibration = 0;
    fake_compass_resume_calls = fake_compass_read_calls = 0;
    fake_compass_calibration_calls = fake_compass_sensor_calls = 0;
    fake_compass_sensor_enabled = 0;
    fake_read_result = WAYPOINTS_RECORD_SIZE; fake_fail_readback = 0;
    fake_update_enabled = 0; fake_update_period = 0; fake_animated_exit = 0;
    fake_free_ram = 24576;
    fake_fw_version = 11205; fake_bipos_build = 7; fake_charging = 1;
    fake_display_state_calls = 0;
    fake_overlay = 0; fake_screen_registrations = 0; active_state = NULL;
    fake_settings_size_calls = 0; fake_settings_read_calls = 0;
    fake_settings_write_calls = 0;
}

static void set_fix(int ready, int latitude, int ns, int longitude, int ew)
{
    fake_navi.ready = ready;
    fake_navi.latitude = latitude; fake_navi.ns = ns;
    fake_navi.longitude = longitude; fake_navi.ew = ew;
    fake_fixed = 1;
}

static void enter_app(void)
{
    show_screen(NULL);
    assert(active_state != NULL);
#if DIAGNOSTIC
    assert(fake_sensor_mode == -1);
    assert(fake_display_state_calls == 2);
#else
    assert(fake_sensor_mode == 1);
    assert(fake_display_state_calls == 2);
    assert(fake_display_state_parameters[0] == 8 && fake_display_state_values[0] == 1);
    assert(fake_display_state_parameters[1] == 2 && fake_display_state_values[1] == 1);
#endif
}

static void click(int x, int y)
{
    struct gesture_ g = {GESTURE_CLICK, x, y};
    (void)dispatch_screen(&g);
}

static void swipe(int gesture)
{
    struct gesture_ g = {gesture, 88, 80};
    (void)dispatch_screen(&g);
}

#if !DIAGNOSTIC
static void test_gps_validation_and_fix_loss(void)
{
    reset_fakes(); enter_app();
    set_fix(0x0c, 45000000, NAVI_NORTH_HEMISPHERE,
            75000000, NAVI_EAST_HEMISPHERE);
    poll_gps(active_state);
    assert(active_state->has_fix);
    assert(near(active_state->current.latitude_deg, 15.0, 1e-9));
    assert(near(active_state->current.longitude_deg, 25.0, 1e-9));
    set_fix(0x04, 999999999, NAVI_NORTH_HEMISPHERE,
            75000000, NAVI_EAST_HEMISPHERE);
    poll_gps(active_state);
    assert(!active_state->has_fix && active_state->have_sample);
    set_fix(0x08, 45000000, NAVI_NORTH_HEMISPHERE,
            75000000, NAVI_EAST_HEMISPHERE);
    poll_gps(active_state);
    assert(!active_state->has_fix);
    set_fix(0x0c, 45000000, 99, 75000000, NAVI_EAST_HEMISPHERE);
    poll_gps(active_state);
    assert(!active_state->has_fix);
    set_fix(0x0c, 45000000, NAVI_NORTH_HEMISPHERE,
            75000000, 99);
    poll_gps(active_state);
    assert(!active_state->has_fix);
    set_fix(0x0c, 270000001, NAVI_NORTH_HEMISPHERE,
            75000000, NAVI_EAST_HEMISPHERE);
    poll_gps(active_state);
    assert(!active_state->has_fix);
    set_fix(0x0c, 45000000, NAVI_NORTH_HEMISPHERE,
            540000001, NAVI_EAST_HEMISPHERE);
    poll_gps(active_state);
    assert(!active_state->has_fix);
    set_fix(0x0c, 45000000, NAVI_SOUTH_HEMISPHERE,
            75000000, NAVI_WEST_HEMISPHERE);
    poll_gps(active_state);
    assert(active_state->has_fix);
    assert(near(active_state->current.latitude_deg, -15.0, 1e-9));
    assert(near(active_state->current.longitude_deg, -25.0, 1e-9));
    fake_fixed = 0; poll_gps(active_state);
    assert(!active_state->has_fix && active_state->have_sample);
}

static void fix_at(int north)
{
    set_fix(0x0c, north, NAVI_NORTH_HEMISPHERE, 75000000, NAVI_EAST_HEMISPHERE);
}

static void new_point(void) { click(88, 160); }
static void browse(void) { click(88, 160); }
static void next_slot(void) { click(146, 15); }
static void previous_slot(void) { click(28, 15); }
static void back(void) { click(88, 15); }
static void select_slot(void) { click(88, 120); }
static void save_here(void) { click(88, 120); }
static void open_options(void) { click(88, 157); }
static void open_coords(void)
{
    if (active_state->page == PAGE_OPTIONS) {
        click(88, 116);
    } else if (active_state->page == PAGE_WAYPOINTS &&
        (active_state->collection.occupied_mask & (1u << active_state->slot))) {
        open_options();
        click(88, 116);
    } else {
        click(88, 157);
    }
}
static void replace_here(void) { click(88, 76); }
static void delete_slot(void) { click(88, 156); }

static void save_target_at(int latitude, int longitude)
{
    set_fix(0x0c, latitude, NAVI_NORTH_HEMISPHERE,
            longitude, NAVI_EAST_HEMISPHERE);
    poll_gps(active_state);
    click(88, 144);
    assert(active_state->page == PAGE_WAYPOINTS);
    save_here();
    assert(active_state->has_waypoint);
    back();
    assert(active_state->page == PAGE_MAIN);
}

static void coordinate_key(char key)
{
    static const char keys[] = "123456789.0\b";
    const char *found = strchr(keys, key);
    assert(found != NULL);
    int index = (int)(found - keys);
    click(29 + index % 3 * 59, 78 + index / 3 * 28);
}

static void enter_coordinate(const char *digits)
{
    while (*digits) coordinate_key(*digits++);
}

static void enter_123_25358_west(void)
{
    enter_coordinate("123.25358");
    click(88, 15); /* E to W */
    click(147, 15); /* LON to review */
}

static void test_coordinate_editor_custom_signed_save_and_reload(void)
{
    reset_fakes(); enter_app();
    fake_fixed = 0; /* A manually entered coordinate must not need a GPS fix. */
    new_point();
    assert(active_state->page == PAGE_WAYPOINTS && active_state->slot == 0);
    assert(drawn_color("SAVE HERE") == COLOR_AQUA);
    assert(drawn_color("COORDS") == COLOR_AQUA);
    open_coords();
    assert(active_state->page == PAGE_LATITUDE && fake_settings_write_calls == 0);
    assert(drawn_color("LAT _ N") == COLOR_WHITE);

    enter_coordinate("49.33750");
    coordinate_key('8'); /* Fractional precision is capped at five digits. */
    assert(drawn_color("LAT 49.33750 N") == COLOR_WHITE);
    click(88, 15);
    assert(drawn_color("LAT 49.33750 S") == COLOR_WHITE);
    click(147, 15);
    assert(active_state->page == PAGE_LONGITUDE);
    click(29, 15);
    assert(active_state->page == PAGE_LATITUDE);
    assert(drawn_color("LAT 49.33750 S") == COLOR_WHITE);
    click(147, 15);
    enter_123_25358_west();
    assert(active_state->page == PAGE_REVIEW);
    assert(drawn_color("CHECK POINT") == COLOR_AQUA);
    assert(drawn_color("LAT -49.33750") == COLOR_WHITE);
    assert(drawn_color("LON -123.25358") == COLOR_WHITE);
    assert(drawn_color("SAVE POINT") == COLOR_GREEN);
    assert(fake_settings_write_calls == 0);
    click(29, 15);
    assert(active_state->page == PAGE_LONGITUDE);
    assert(drawn_color("LON 123.25358 W") == COLOR_WHITE);
    click(147, 15);
    assert(active_state->page == PAGE_REVIEW);

    click(88, 158);
    assert(active_state->page == PAGE_WAYPOINTS);
    assert(drawn_color("SAVED") == COLOR_GREEN);
    assert(active_state->collection.selected == 0 && active_state->has_waypoint);
    assert(near(active_state->waypoint.latitude_deg, -49.3375, 1e-7));
    assert(near(active_state->waypoint.longitude_deg, -123.25358, 1e-7));
    assert(fake_settings_write_calls == 1);

    active_state = NULL;
    show_screen(NULL);
    assert(active_state->collection.occupied_mask == 1);
    assert(near(active_state->waypoint.latitude_deg, -49.3375, 1e-7));
    assert(near(active_state->waypoint.longitude_deg, -123.25358, 1e-7));
}

static void test_coordinate_editor_exact_ranges_and_invalid_values(void)
{
    reset_fakes(); enter_app();
    new_point(); open_coords();
    enter_coordinate("90.00001");
    click(147, 15);
    assert(active_state->page == PAGE_LATITUDE);
    assert(drawn_color("ENTER 0 TO 90") == COLOR_YELLOW);
    coordinate_key('\b');
    click(147, 15);
    assert(active_state->page == PAGE_LONGITUDE);
    enter_coordinate("180.00001");
    click(147, 15);
    assert(active_state->page == PAGE_LONGITUDE);
    assert(drawn_color("ENTER 0 TO 180") == COLOR_YELLOW);
    coordinate_key('\b');
    click(147, 15);
    assert(active_state->page == PAGE_REVIEW);
    assert(drawn_color("LAT +90.00000") == COLOR_WHITE);
    assert(drawn_color("LON +180.00000") == COLOR_WHITE);
    assert(fake_settings_write_calls == 0);
}

static void test_coordinate_editor_cancel_replace_and_storage_retry(void)
{
    waypoints_t stored;
    reset_fakes();
    waypoints_init(&stored);
    assert(waypoints_set(&stored, 0, (nav_coordinate_t){12.5, -40.25}));
    assert(waypoints_encode(&stored, fake_settings));
    enter_app();
    click(44, 160); /* Browse the selected occupied slot. */
    open_coords();
    enter_coordinate("13.5"); click(147, 15);
    enter_coordinate("41.25"); click(88, 15); click(147, 15);
    assert(active_state->page == PAGE_REVIEW);
    assert(drawn_color("REPLACE POINT") == COLOR_YELLOW);
    assert(fake_settings_write_calls == 0);

    fake_fail_readback = 1;
    click(88, 158);
    assert(active_state->page == PAGE_REVIEW);
    assert(drawn_color("STORAGE FAILED") == COLOR_RED);
    assert(fake_settings_write_calls == 1);
    fake_write_result = -1;
    click(88, 158);
    assert(active_state->page == PAGE_REVIEW);
    assert(drawn_color("STORAGE FAILED") == COLOR_RED);
    assert(fake_settings_write_calls == 2);

    fake_write_result = WAYPOINTS_RECORD_SIZE;
    click(88, 158);
    assert(active_state->page == PAGE_WAYPOINTS);
    assert(drawn_color("SAVED") == COLOR_GREEN);
    assert(active_state->collection.occupied_mask == 1);
    assert(near(active_state->waypoint.latitude_deg, 13.5, 1e-7));
    assert(near(active_state->waypoint.longitude_deg, -41.25, 1e-7));

    open_coords();
    enter_coordinate("77");
    click(29, 15);
    assert(active_state->page == PAGE_OPTIONS && fake_settings_write_calls == 3);
    open_coords();
    enter_coordinate("55");
    struct gesture_ swipe = {GESTURE_SWIPE_RIGHT, 20, 70};
    (void)dispatch_screen(&swipe);
    assert(active_state->page == PAGE_OPTIONS && fake_settings_write_calls == 3);
}

static void test_create_select_reload_and_units(void)
{
    waypoints_t stored;
    reset_fakes(); enter_app();
    assert(drawn_color("WAYPOINTS") == COLOR_AQUA);
    click(132, 15);
    assert(active_state->units == NAV_UNITS_NAUTICAL);
    assert(fake_settings_write_calls == 0);
    fix_at(45000000);
    browse(); next_slot();
    save_here();
    assert(active_state->collection.occupied_mask == 2 && active_state->collection.selected == 1);
    assert(fake_settings_write_calls == 1);
    previous_slot();
    fix_at(48000000);
    save_here(); /* Saving HOME leaves CAR selected. */
    assert(active_state->collection.occupied_mask == 3 && active_state->collection.selected == 1);
    select_slot(); /* SELECT is available only on an inactive occupied slot. */
    assert(active_state->page == PAGE_MAIN && fake_settings_write_calls == 3);
    assert(drawn_color("WAYPOINTS") == COLOR_AQUA);
    assert(waypoints_decode(fake_settings, WAYPOINTS_RECORD_SIZE, &stored));
    assert(stored.selected == 0 && stored.units == NAV_UNITS_NAUTICAL);
    active_state = NULL;
    show_screen(NULL);
    assert(active_state->collection.occupied_mask == 3 && active_state->collection.selected == 0);
    assert(active_state->units == NAV_UNITS_NAUTICAL);
    assert(near(active_state->waypoint.latitude_deg, 16, 1e-7));
    assert(near(active_state->collection.coordinates[1].latitude_deg, 15, 1e-7));
}

static void test_full_slots_and_safe_actions(void)
{
    waypoints_t stored;
    reset_fakes(); enter_app();
    click(88, 144); /* The center bottom action opens the empty HOME browser. */
    assert(active_state->page == PAGE_WAYPOINTS && active_state->slot == 0);
    assert(drawn_color("SAVE HERE") == COLOR_AQUA && drawn_color("COORDS") == COLOR_AQUA);
    fix_at(45000000);
    click(88, 104); /* Empty-slot upper row saves the current GPS coordinate. */
    assert(active_state->collection.occupied_mask == 1 && fake_settings_write_calls == 1);
    assert(active_state->page == PAGE_WAYPOINTS);
    click(88, 140); /* Active slot offers OPTIONS across the lower row. */
    assert(active_state->page == PAGE_OPTIONS);
    assert(fake_settings_write_calls == 1);
    back();
    assert(active_state->page == PAGE_WAYPOINTS);
    click(88, 157);
    struct gesture_ swipe = {GESTURE_SWIPE_RIGHT, 80, 70};
    (void)dispatch_screen(&swipe);
    assert(active_state->page == PAGE_WAYPOINTS);
    click(88, 157);
    replace_here();
    assert(active_state->page == PAGE_OPTIONS && active_state->action_pending);
    assert(fake_settings_write_calls == 1);
    replace_here();
    assert(active_state->page == PAGE_WAYPOINTS && !active_state->action_pending);
    assert(fake_settings_write_calls == 2);
    click(88, 31); /* Browser BACK exits to the navigator. */
    assert(active_state->page == PAGE_MAIN);
    assert(fake_settings_read_calls >= 1);
    assert(waypoints_decode(fake_settings, WAYPOINTS_RECORD_SIZE, &stored));
    assert(stored.occupied_mask == 1);
}

static void test_storage_faults_and_legacy(void)
{
    reset_fakes();
    assert(nav_waypoint_encode((nav_coordinate_t){12, -34}, fake_settings));
    fake_settings_size = NAV_WAYPOINT_RECORD_SIZE;
    fake_read_result = NAV_WAYPOINT_RECORD_SIZE;
    enter_app();
    assert(active_state->collection.occupied_mask == 1 && active_state->has_waypoint);
    assert(near(active_state->waypoint.latitude_deg, 12, 1e-7));
    fake_settings_size = fake_read_result = WAYPOINTS_RECORD_SIZE;
    fix_at(45000000);
    new_point(); next_slot();
    fake_fail_readback = 1;
    save_here();
    assert(active_state->collection.occupied_mask == 1);
    assert(drawn_color("STORAGE FAILED") == COLOR_RED);
    fake_write_result = -1;
    save_here();
    assert(active_state->collection.occupied_mask == 1);
    fake_write_result = WAYPOINTS_RECORD_SIZE - 1;
    fake_settings[WAYPOINTS_RECORD_SIZE - 1] ^= 0xff;
    save_here();
    assert(active_state->collection.occupied_mask == 1);
    fake_write_result = WAYPOINTS_RECORD_SIZE;
    save_here();
    assert(active_state->collection.occupied_mask == 3);
    fake_write_result = -1;
    select_slot();
    assert(active_state->page != PAGE_MAIN && active_state->collection.selected == 0);
    open_options();
    delete_slot(); delete_slot();
    assert(active_state->collection.occupied_mask == 3);
    assert(drawn_color("STORAGE FAILED") == COLOR_RED);
}

static void expect_distance(double metres, int units, int expected, const char *unit, int blocks)
{
    active_state->has_fix = active_state->has_waypoint = 1;
    active_state->current = (nav_coordinate_t){0, 0};
    active_state->waypoint = (nav_coordinate_t){0, metres / 111195.08};
    active_state->units = units;
    draw_screen(active_state);
    assert(active_state->displayed.distance == expected);
    assert(drawn_color(unit) == COLOR_WHITE);
    assert(rectangles == blocks);
}

static void test_distance_and_color(void)
{
    reset_fakes(); enter_app();
    assert(drawn_color("WAITING FOR FIX") == COLOR_YELLOW);
    expect_distance(5, NAV_UNITS_METRIC, 5, "M", 5);
    expect_distance(5, NAV_UNITS_NAUTICAL, 5, "M", 5);
    assert(drawn_color("NEAR TARGET") == COLOR_WHITE);
    assert(drawn_color("BRG 090 T") == -1);
    expect_distance(999, NAV_UNITS_METRIC, 999, "M", 18);
    expect_distance(999, NAV_UNITS_NAUTICAL, 999, "M", 18);
    assert(drawn_color("BRG 090 T") == COLOR_WHITE);
    expect_distance(1000.2, NAV_UNITS_METRIC, 10, "KM", 9);
    expect_distance(1000.2, NAV_UNITS_NAUTICAL, 5, "NM", 12);
    expect_distance(1852, NAV_UNITS_NAUTICAL, 10, "NM", 9);
    expect_distance(20000000, NAV_UNITS_METRIC, 200000, "20000.0 KM", 0);
    assert(drawn_color("FIX READY") == COLOR_GREEN);
    assert(drawn_color("TO HOME") == COLOR_AQUA);
    active_state->has_fix = 0;
    draw_screen(active_state);
    assert(rectangles == 0 && drawn_color("NEAR TARGET") == -1);
}

static void test_variation_editor_and_heading(void)
{
    reset_fakes(); enter_app();
    save_target_at(45000000, 75003000);
    fake_compass_degree = 358;
    fix_at(45000000); screen_job();
    assert(active_state->displayed.heading == 358 && active_state->displayed.bearing == 90);

    browse();
    click(113, 15);
    assert(active_state->page == PAGE_VARIATION && drawn_color("OFF") == COLOR_WHITE);
    assert(active_state->gps_on);
    int writes = fake_settings_write_calls;
    click(29, 75);
    assert(drawn_color("1 deg W") == COLOR_WHITE);
    back();
    assert(active_state->page == PAGE_WAYPOINTS && fake_settings_write_calls == writes);

    click(113, 15);
    for (int i = 0; i < 15; ++i) click(147, 75);
    assert(drawn_color("15 deg E") == COLOR_WHITE && fake_settings_write_calls == writes);
    fake_write_result = -1;
    click(88, 153);
    assert(active_state->page == PAGE_VARIATION && active_state->collection.variation == 0);
    assert(drawn_color("STORAGE FAILED") == COLOR_RED);
    fake_write_result = WAYPOINTS_RECORD_SIZE;
    click(88, 153);
    assert(active_state->page == PAGE_WAYPOINTS && active_state->collection.variation == 15);
    back();
    assert(active_state->displayed.heading == 13 && active_state->displayed.bearing == 90);

    active_state = NULL;
    show_screen(NULL);
    assert(active_state->collection.variation == 15 && active_state->has_waypoint);
    assert(active_state->displayed.heading == 13 && active_state->displayed.bearing == 90);
    browse();
    click(113, 15);
    click(88, 105); /* Reset the draft before saving. */
    click(29, 75);
    click(88, 153);
    fake_compass_degree = 0;
    back();
    assert(active_state->collection.variation == -1 && active_state->displayed.heading == 359);

    browse();
    click(113, 15);
    click(88, 105);
    click(88, 153);
    back();
    assert(active_state->collection.variation == 0 && active_state->displayed.heading == 0);
}

static void test_edge_compass_rotation_and_lifecycle(void)
{
    reset_fakes(); enter_app();
    assert(fake_compass_resume_calls == 0 && fake_compass_read_calls == 0);
    save_target_at(45000000, 75003000);
    active_state->course_origin_valid = 0; /* Start observing GPS from the simulated test position. */
    fake_compass_degree = 359;
    fix_at(45000000); screen_job();
    assert(drawn_color("BRG 090 T") == COLOR_WHITE);
    assert(fake_compass_resume_calls == 1 && fake_compass_sensor_calls == 1);
    assert(fake_compass_sensor_enabled == 1 && fake_update_period == 1000);
    assert(active_state->gps_on && fake_sensor_mode == 1);
    assert(red_rectangles > 0 && green_rectangles > 0 && yellow_rectangles == 0);
    assert(red_ysum < 2 * 16 * red_rectangles);
    assert(green_xsum > 2 * 150 * green_rectangles);

    int distance = active_state->displayed.distance;
    fake_compass_degree = 1;
    screen_job();
    assert(red_ysum < 2 * 16 * red_rectangles);
    fake_compass_degree = 90;
    screen_job();
    assert(active_state->displayed.distance == distance && active_state->displayed.bearing == 90);
    assert(red_xsum < 2 * 16 * red_rectangles);
    assert(green_ysum < 2 * 26 * green_rectangles);

    fake_compass_degree = 275;
    screen_job();
    assert(red_xsum > 2 * 150 * red_rectangles);
    assert(green_ysum > 2 * 150 * green_rectangles);

    fake_tick = active_state->sample_tick + 15301u;
    screen_job();
    assert(active_state->unchanged && !active_state->has_fix);
    assert(drawn_color("LAST FIX") == COLOR_YELLOW && green_rectangles > 0);
    fake_tick = active_state->sample_tick + COMPASS_FIX_TICKS + 1;
    screen_job();
    assert(drawn_color("FIX UNCHANGED") == COLOR_YELLOW && green_rectangles == 0);
    assert(red_rectangles > 0);
    fix_at(45000001); screen_job();
    assert(active_state->has_fix && green_rectangles > 0);

    browse();
    assert(!fake_compass_sensor_enabled && active_state->gps_on && fake_update_period == UPDATE_MS);
    back();
    assert(active_state->page == PAGE_MAIN && fake_compass_sensor_enabled);
    assert(green_rectangles > 0 && yellow_rectangles == 0);

    key_press_screen();
    assert(fake_compass_sensor_enabled == 0 && fake_sensor_mode == 0);
    assert(!active_state->gps_on && !fake_update_enabled);
}

static void test_edge_gps_course_and_missing_sensors(void)
{
    reset_fakes(); enter_app();
    save_target_at(45003000, 75000000);
    active_state->course_origin_valid = 0; /* Start observing GPS from the simulated test position. */
    fix_at(45000000); screen_job();
    assert(green_rectangles > 0 && yellow_rectangles == 0);
    fake_tick += 510;
    fix_at(45000100); screen_job();
    assert(yellow_rectangles == 0); /* About four metres, below the movement threshold. */

    fake_tick += 5100;
    fix_at(45001000); screen_job();
    assert(active_state->displayed.course == 0 && active_state->displayed.bearing == 0);
    assert(red_rectangles > 0 && green_rectangles > 0 && yellow_rectangles > 0);
    assert(yellow_ysum < 2 * 26 * yellow_rectangles);
    assert(yellow_xsum / yellow_rectangles != green_xsum / green_rectangles);

    fake_compass_degree = 90;
    screen_job();
    assert(active_state->displayed.course == 0);
    assert(yellow_xsum < 2 * 26 * yellow_rectangles);
    fake_tick += COURSE_HOLD_TICKS + 1;
    screen_job();
    assert(active_state->displayed.course == -1 && yellow_rectangles == 0);

    fake_tick += 5100;
    fix_at(45002000); screen_job();
    assert(yellow_rectangles > 0);
    fake_fixed = 0;
    screen_job();
    assert(drawn_color("LAST FIX") == COLOR_YELLOW);
    assert(red_rectangles > 0 && green_rectangles > 0 && yellow_rectangles == 0);

    fake_tick += 510;
    fix_at(45003000); screen_job();
    assert(drawn_color("NEAR TARGET") == COLOR_WHITE);
    assert(green_rectangles == 0 && red_rectangles > 0);
    fake_compass_calibration = 1;
    screen_job();
    assert(drawn_color("CHECK COMPASS") == COLOR_YELLOW);
    assert(red_rectangles == 0 && green_rectangles == 0 && yellow_rectangles == 0);
    fake_compass_calibration = 0;
    fake_compass_degree = 360;
    screen_job();
    assert(red_rectangles == 0);
}

static void test_stationary_fix_guard(void)
{
    reset_fakes(); enter_app();
    fix_at(45000000); screen_job();
    assert(active_state->has_fix && !active_state->unchanged);
    fake_tick += 15301;
    screen_job();
    assert(!active_state->has_fix && active_state->unchanged);
    assert(drawn_color("MOVE TO UPDATE") == COLOR_YELLOW);
    new_point(); save_here();
    assert(!active_state->has_waypoint && fake_settings_write_calls == 0);
    fix_at(45000001); screen_job();
    assert(active_state->has_fix && !active_state->unchanged);
    save_here();
    assert(active_state->has_waypoint && fake_settings_write_calls == 1);
}

static void test_last_fix_hold_across_waypoint_navigation(void)
{
    reset_fakes(); enter_app();
    fix_at(45000000); screen_job();
    assert(active_state->has_fix && active_state->have_sample);
    nav_coordinate_t last = active_state->current;
    browse();
    assert(active_state->page == PAGE_WAYPOINTS);
    fake_fixed = 0;
    screen_job();
    assert(!active_state->has_fix && active_state->have_sample);
    assert(near(active_state->current.latitude_deg, last.latitude_deg, 1e-9));
    assert(drawn_color("LAST FIX") == COLOR_YELLOW);
    save_here();
    assert(!active_state->has_waypoint && fake_settings_write_calls == 0);
    back();
    assert(active_state->page == PAGE_MAIN);
    assert(drawn_color("LAST FIX") == COLOR_YELLOW);
    assert(active_state->gps_on && fake_sensor_mode == 1);
    fake_tick += 5101;
    screen_job();
    assert(!active_state->has_fix && !active_state->have_sample);
    assert(drawn_color("LAST FIX") == -1);
    fix_at(45000030); screen_job();
    assert(active_state->has_fix && active_state->have_sample);
    assert(active_state->gps_on && fake_sensor_mode == 1);
    browse(); save_here();
    assert(active_state->has_waypoint && fake_settings_write_calls == 1);
    back();
    fix_at(46000000); screen_job();
    int last_distance = active_state->displayed.distance;
    assert(last_distance > 0);
    fake_fixed = 0; screen_job();
    assert(!active_state->has_fix && active_state->have_sample);
    assert(active_state->displayed.distance == last_distance);
    assert(drawn_color("LAST FIX") == COLOR_YELLOW);
    fake_tick += 5101;
    screen_job();
    assert(!active_state->has_fix && !active_state->have_sample);
    assert(drawn_color("WAITING FOR FIX") == COLOR_YELLOW);
    assert(active_state->displayed.distance == -1);
}

static void test_render_scheduling_and_overlay(void)
{
    int before_paints, before_clears, before_polls, before_flags;
    reset_fakes(); enter_app();
    fix_at(45000000); new_point(); save_here(); back();
    fix_at(45000300); screen_job();
    assert(active_state->displayed.distance == 11 && green_rectangles > 0);
    before_paints = repaints; before_clears = screen_clears;
    before_polls = polls; before_flags = fake_display_state_calls;
    fix_at(45000301); /* Raw position changed, but rounded distance and bearing did not. */
    screen_job(); screen_job();
    assert(repaints == before_paints && screen_clears == before_clears);
    assert(polls == before_polls + 2 && fake_display_state_calls == before_flags + 4);
    fake_overlay = 1;
    watch_state_t *retained = active_state;
    show_screen(retained);
    assert(active_state == retained && screen_clears == before_clears + 1);
    fake_fixed = 0; screen_job();
    assert(repaints > before_paints && !active_state->has_fix);
    click(12, 15); /* The full top row now toggles units; GPS stays automatic. */
    assert(active_state->units == NAV_UNITS_NAUTICAL);
    assert(active_state->gps_on && fake_sensor_mode == 1);
    before_polls = polls; before_flags = fake_display_state_calls;
    screen_job();
    assert(polls == before_polls + 1 && fake_display_state_calls == before_flags + 2);
    browse();
    key_press_screen();
    assert(fake_animated_exit == 1 && fake_sensor_mode == 0 && !fake_update_enabled);
}

static void test_legend_compass_swipes_and_footer(void)
{
    reset_fakes(); enter_app();
    assert(active_state->page == PAGE_MAIN && active_state->gps_on);

    int before_polls = polls;
    swipe(GESTURE_SWIPE_RIGHT);
    assert(active_state->page == PAGE_LEGEND);
    assert(active_state->units == NAV_UNITS_METRIC);
    click(150, 15); /* Unit selection is only available on MAIN. */
    assert(active_state->page == PAGE_LEGEND && active_state->units == NAV_UNITS_METRIC);
    assert(drawn_color("LEGEND") == COLOR_WHITE);
    assert(drawn_color("WAYPOINT") != -1 && drawn_color("GPS TRACK") != -1 &&
           drawn_color("NORTH") != -1);
    assert(active_state->gps_on && fake_sensor_mode == 1 && polls == before_polls);
    assert(fake_animated_exit == 0);
    screen_job();
    assert(polls == before_polls + 1 && active_state->gps_on);

    swipe(GESTURE_SWIPE_RIGHT); /* Already at the outer edge. */
    assert(active_state->page == PAGE_LEGEND && fake_animated_exit == 0);
    swipe(GESTURE_SWIPE_LEFT);
    assert(active_state->page == PAGE_MAIN);
    swipe(GESTURE_SWIPE_LEFT);
    assert(active_state->page == PAGE_COMPASS);
    click(150, 15);
    assert(active_state->page == PAGE_COMPASS && active_state->units == NAV_UNITS_METRIC);
    assert(drawn_color("COMPASS") == COLOR_WHITE);
    assert(active_state->gps_on && fake_sensor_mode == 1 && fake_animated_exit == 0);
    screen_job(); /* Compass works even when no waypoint is saved. */
    assert(active_state->compass_on && fake_compass_sensor_enabled == 1);
    assert(fake_compass_read_calls > 0 && drawn_color("COMPASS") == COLOR_WHITE);
    swipe(GESTURE_SWIPE_LEFT); /* Already at the right outer edge. */
    assert(active_state->page == PAGE_COMPASS && fake_animated_exit == 0);
    swipe(GESTURE_SWIPE_RIGHT);
    assert(active_state->page == PAGE_MAIN && active_state->gps_on);

    assert(drawn_color("WAYPOINTS") == COLOR_AQUA);
    assert(drawn_color("?") == -1 && drawn_color("CMP") == -1);
    click(20, 160);
    assert(active_state->page == PAGE_WAYPOINTS);
    back();
    click(150, 160);
    assert(active_state->page == PAGE_WAYPOINTS);
    back();

    swipe(GESTURE_SWIPE_RIGHT);
    click(88, 150); /* Legend BACK returns to Navigator. */
    assert(active_state->page == PAGE_MAIN);
    swipe(GESTURE_SWIPE_LEFT);
    click(88, 150); /* Compass BACK returns to Navigator. */
    assert(active_state->page == PAGE_MAIN && active_state->gps_on);
    assert(fake_animated_exit == 0);

    key_press_screen();
    assert(fake_animated_exit == 1 && fake_sensor_mode == 0 && !active_state->gps_on);
    assert(fake_compass_sensor_enabled == 0 && !active_state->compass_on && !fake_update_enabled);

    const int exit_pages[] = {PAGE_LEGEND, PAGE_COMPASS};
    for (size_t i = 0; i < sizeof(exit_pages) / sizeof(exit_pages[0]); ++i) {
        int page = exit_pages[i];
        reset_fakes(); enter_app();
        if (page == PAGE_LEGEND) swipe(GESTURE_SWIPE_RIGHT);
        else swipe(GESTURE_SWIPE_LEFT);
        assert(active_state->page == page && active_state->gps_on);
        key_press_screen();
        assert(fake_animated_exit == 1 && fake_sensor_mode == 0 && !active_state->gps_on);
        assert(fake_compass_sensor_enabled == 0 && !active_state->compass_on && !fake_update_enabled);
    }
}

static void test_compass_continues_across_root_pages_with_waypoint(void)
{
    reset_fakes(); enter_app();
    save_target_at(45000000, 75003000);
    assert(active_state->compass_on && fake_compass_sensor_enabled == 1);
    fake_compass_degree = 38;
    screen_job();
    assert(active_state->displayed.heading == 38);

    swipe(GESTURE_SWIPE_RIGHT);
    assert(active_state->page == PAGE_LEGEND && active_state->gps_on && active_state->compass_on);
    assert(fake_sensor_mode == 1 && fake_compass_sensor_enabled == 1);
    screen_job();
    assert(fake_compass_sensor_enabled == 1 && active_state->gps_on);
    swipe(GESTURE_SWIPE_LEFT);
    swipe(GESTURE_SWIPE_LEFT);
    assert(active_state->page == PAGE_COMPASS && active_state->gps_on && active_state->compass_on);
    screen_job();
    assert(fake_compass_sensor_enabled == 1 && fake_sensor_mode == 1);
    swipe(GESTURE_SWIPE_RIGHT);
    assert(active_state->page == PAGE_MAIN && active_state->compass_on && active_state->gps_on);

    key_press_screen();
    assert(fake_sensor_mode == 0 && fake_compass_sensor_enabled == 0);
    assert(!active_state->gps_on && !active_state->compass_on && !fake_update_enabled);
}
#else
static void test_diagnostic_menu_and_sensor_lifecycle(void)
{
    reset_fakes();
    set_fix(0x0c, 45000000, NAVI_NORTH_HEMISPHERE,
            75000000, NAVI_EAST_HEMISPHERE);
    enter_app();
    assert(active_state->page == DIAG_MENU);
    assert(!active_state->gps_on && !active_state->compass_on && polls == 0);
    assert(drawn_has("DIAGNOSTICS") && drawn_has("GPS") &&
           drawn_has("COMPASS") && drawn_has("WATCH"));
    assert(fake_settings_size_calls == 0 && fake_settings_read_calls == 0 &&
           fake_settings_write_calls == 0);

    swipe(GESTURE_SWIPE_RIGHT);
    click(88, 30); /* Menu header does not start GPS or switch pages. */
    assert(active_state->page == DIAG_MENU && fake_sensor_mode == -1 && polls == 0);
    assert(fake_animated_exit == 0 && fake_update_enabled);
    click(88, 68);
    assert(active_state->page == DIAG_GPS && active_state->gps_on && active_state->has_fix);
    assert(fake_sensor_mode == 1 && drawn_has("GPS DIAGNOSTIC"));
    assert(drawn_has("LAT +15.00000") && drawn_has("LON +25.00000"));
    assert(drawn_has("RAW 0 PA") && drawn_has("READY 0000000C"));
    assert(fake_compass_sensor_enabled == 0 && fake_compass_resume_calls == 0);

    int polls_before = polls;
    click(100, 30); /* Only the left side of the GPS header is the toggle. */
    assert(active_state->gps_on && active_state->has_fix && polls == polls_before);
    click(10, 30);
    assert(!active_state->gps_on && !active_state->has_fix && fake_sensor_mode == 0);
    assert(drawn_has("GPS PAUSED"));
    click(10, 30);
    assert(active_state->gps_on && active_state->has_fix && fake_sensor_mode == 1);
    assert(drawn_has("LAT +15.00000") && drawn_has("READY 0000000C"));
    assert(fake_settings_size_calls == 0 && fake_settings_read_calls == 0 &&
           fake_settings_write_calls == 0);

    click(88, 160);
    assert(active_state->page == DIAG_MENU && !active_state->gps_on && fake_sensor_mode == 0);
    fake_compass_degree = 123;
    click(88, 108);
    assert(active_state->page == DIAG_COMPASS && active_state->compass_on && !active_state->gps_on);
    assert(fake_sensor_mode == 0 && fake_compass_sensor_enabled == 1);
    assert(fake_compass_resume_calls == 1 && fake_compass_read_calls == 1);
    assert(fake_update_period == 1000 && drawn_has("COMPASS CHECK") &&
           drawn_has("RAW HEADING") && drawn_has("123 deg") && drawn_has("REF UNVERIFIED"));
    fake_compass_calibration = 1;
    fake_compass_degree = 42;
    screen_job();
    assert(fake_compass_calibration_calls == 2 && fake_compass_read_calls == 2);
    assert(active_state->compass_calibration_needed && !active_state->compass_heading_valid);
    assert(drawn_has("CALIBRATION NEEDED") && fake_update_period == 1000);
    swipe(GESTURE_SWIPE_RIGHT);
    assert(active_state->page == DIAG_GPS && !active_state->compass_on && active_state->gps_on);
    assert(fake_compass_sensor_enabled == 0 && fake_sensor_mode == 1);
    swipe(GESTURE_SWIPE_RIGHT);
    assert(active_state->page == DIAG_MENU && !active_state->gps_on);
    swipe(GESTURE_SWIPE_LEFT);
    assert(active_state->page == DIAG_GPS && active_state->gps_on);
    swipe(GESTURE_SWIPE_LEFT);
    assert(active_state->page == DIAG_COMPASS && active_state->compass_on && !active_state->gps_on);
    swipe(GESTURE_SWIPE_LEFT);
    swipe(GESTURE_SWIPE_LEFT); /* The last page stays open. */
    assert(active_state->page == DIAG_WATCH && !active_state->gps_on && !active_state->compass_on);
    assert(fake_animated_exit == 0 && fake_update_enabled);
    assert(fake_sensor_mode == 0 && fake_compass_sensor_enabled == 0);
    assert(drawn_has("WATCH INFO") && drawn_has("FW 1.1.2.05") &&
           drawn_has("BIPOS BUILD 7") && drawn_has("FREE RAM 24576 B") &&
           drawn_has("BATTERY 76%") && drawn_has("CHARGING YES"));
    fake_charging = 0;
    draw_screen(active_state);
    assert(drawn_has("FW 1.1.2.05") && drawn_has("BIPOS BUILD 7") &&
           drawn_has("FREE RAM 24576 B") && drawn_has("BATTERY 76%") &&
           drawn_has("CHARGING NO"));
    fake_fw_version = 55555;
    fake_bipos_build = -1;
    draw_screen(active_state);
    assert(drawn_has("FW UNIVERSAL") && drawn_has("BIPOS BUILD N/A") &&
           drawn_has("FREE RAM 24576 B") && drawn_has("BATTERY 76%"));
    click(88, 155);
    assert(active_state->page == DIAG_MENU);

    key_press_screen();
    assert(fake_animated_exit == 1 && fake_sensor_mode == 0 &&
           fake_compass_sensor_enabled == 0 && !fake_update_enabled);
    reset_fakes(); enter_app(); click(88, 108);
    key_press_screen();
    assert(fake_animated_exit == 1 && fake_compass_sensor_enabled == 0 &&
           !active_state->compass_on && !fake_update_enabled);
}
#endif

/* This checks scheduling/calls, not the actual firmware's timeout semantics. */
static void test_active_gps_display_refresh(void)
{
#if DIAGNOSTIC
    reset_fakes();
    enter_app();
    assert(active_state->page == DIAG_MENU && polls == 0);
    screen_job();
    assert(polls == 0 && fake_display_state_calls == 2);
    click(88, 68);
    assert(active_state->gps_on && polls == 1);
    screen_job();
    assert(polls == 2 && fake_display_state_calls == 4);
    click(10, 30);
    assert(!active_state->gps_on && fake_sensor_mode == 0);
    int before_polls = polls;
    screen_job();
    assert(polls == before_polls && fake_display_state_calls == 4);
    exit_app(active_state);
    assert(!fake_update_enabled && fake_sensor_mode == 0);
#else
    int before;
    reset_fakes();
    enter_app();
    before = fake_display_state_calls;
    screen_job();
    assert(fake_display_state_calls == before + 2);
    assert(fake_display_state_parameters[before] == 8 && fake_display_state_values[before] == 1);
    assert(fake_display_state_parameters[before + 1] == 2 && fake_display_state_values[before + 1] == 1);
    click(10, 30); /* Navigator has no GPS pause control. */
    assert(active_state->gps_on && fake_sensor_mode == 1);
    before = fake_display_state_calls;
    screen_job();
    assert(fake_display_state_calls == before + 2);
    exit_app(active_state);
    assert(!fake_update_enabled && fake_sensor_mode == 0);
#endif
}

#ifndef HOST_TEST_MAIN
#define HOST_TEST_MAIN main
#endif
int HOST_TEST_MAIN(void)
{
    test_active_gps_display_refresh();
#if DIAGNOSTIC
    test_diagnostic_menu_and_sensor_lifecycle();
    puts("diagnostic watch integration tests passed");
#else
    test_gps_validation_and_fix_loss();
    test_create_select_reload_and_units();
    test_coordinate_editor_custom_signed_save_and_reload();
    test_coordinate_editor_exact_ranges_and_invalid_values();
    test_coordinate_editor_cancel_replace_and_storage_retry();
    test_full_slots_and_safe_actions();
    test_storage_faults_and_legacy();
    test_distance_and_color();
    test_variation_editor_and_heading();
    test_edge_compass_rotation_and_lifecycle();
    test_edge_gps_course_and_missing_sensors();
    test_stationary_fix_guard();
    test_last_fix_hold_across_waypoint_navigation();
    test_render_scheduling_and_overlay();
    test_legend_compass_swipes_and_footer();
    test_compass_continues_across_root_pages_with_waypoint();
    puts("watch integration tests passed");
#endif
    return 0;
}
