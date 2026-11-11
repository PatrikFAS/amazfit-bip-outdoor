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
static int fake_write_result, fake_read_result, fake_fail_readback;
static int fake_update_enabled, fake_update_period, fake_animated_exit;
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

#if !DIAGNOSTIC
static int drawn_color(const char *text)
{
    for (int i = 0; i < draw_count; ++i)
        if (strcmp(drawn[i].text, text) == 0) return drawn[i].color;
    return -1;
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
void set_bg_color(int color) { (void)color; }
void fill_screen_bg(void) { ++screen_clears; draw_count = rectangles = 0; }
void draw_filled_rect(int x1, int y1, int x2, int y2)
{
    assert(x1 >= 0 && x1 <= x2 && x2 < 176);
    assert(y1 >= 72 && y1 <= y2 && y2 <= 108);
    ++rectangles;
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
    memset(fake_settings, 0, sizeof(fake_settings));
    fake_fixed = 1; fake_tick = 1000; fake_settings_size = WAYPOINTS_RECORD_SIZE;
    fake_sensor_mode = -1; fake_write_result = WAYPOINTS_RECORD_SIZE;
    fake_read_result = WAYPOINTS_RECORD_SIZE; fake_fail_readback = 0;
    fake_update_enabled = 0; fake_update_period = 0; fake_animated_exit = 0;
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
    assert(fake_sensor_mode == 1);
    assert(fake_display_state_calls == 2);
    assert(fake_display_state_parameters[0] == 8 && fake_display_state_values[0] == 1);
    assert(fake_display_state_parameters[1] == 2 && fake_display_state_values[1] == 1);
}

static void click(int x, int y)
{
    struct gesture_ g = {GESTURE_CLICK, x, y};
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
    assert(!active_state->has_fix && !active_state->have_sample);
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
    assert(!active_state->has_fix && !active_state->have_sample);
}

static void fix_at(int north)
{
    set_fix(0x0c, north, NAVI_NORTH_HEMISPHERE, 75000000, NAVI_EAST_HEMISPHERE);
}

static void new_point(void) { click(134, 160); }
static void browse(void) { click(44, 160); }
static void next_slot(void) { click(146, 15); }
static void previous_slot(void) { click(28, 15); }
static void back(void) { click(88, 15); }
static void select_slot(void) { click(88, 115); }
static void save_here(void) { click(88, 139); }
static void delete_slot(void) { click(88, 163); }

static void test_create_select_reload_and_units(void)
{
    waypoints_t stored;
    reset_fakes(); enter_app();
    assert(drawn_color("SAVE A POINT") == COLOR_AQUA);
    assert(drawn_color("WAYPOINTS") == -1);
    click(132, 15);
    assert(active_state->units == NAV_UNITS_NAUTICAL);
    assert(fake_settings_write_calls == 0);
    fix_at(45000000);
    click(44, 160); next_slot(); /* First save outside slot 0 must activate that slot. */
    save_here();
    assert(active_state->collection.occupied_mask == 2 && active_state->collection.selected == 1);
    assert(fake_settings_write_calls == 1);
    select_slot(); /* Already selected, returns without another write. */
    assert(!active_state->page && fake_settings_write_calls == 1);
    assert(drawn_color("WAYPOINTS") == COLOR_AQUA);
    assert(drawn_color("NEW POINT") == COLOR_AQUA);
    new_point();
    assert(active_state->slot == 0);
    fix_at(48000000);
    save_here();
    assert(active_state->collection.occupied_mask == 3 && active_state->collection.selected == 1);
    select_slot();
    assert(!active_state->page && active_state->collection.selected == 0);
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
    waypoints_t full;
    reset_fakes();
    waypoints_init(&full);
    for (unsigned i = 0; i < WAYPOINTS_SLOT_COUNT; ++i)
        assert(waypoints_set(&full, i, (nav_coordinate_t){i, 25}));
    assert(waypoints_select(&full, 2));
    assert(waypoints_encode(&full, fake_settings));
    enter_app(); fix_at(45000000);
    new_point();
    assert(active_state->page && active_state->slot == 2);
    assert(drawn_color("SLOTS FULL") == COLOR_YELLOW);
    save_here();
    assert(active_state->action_pending && fake_settings_write_calls == 0);
    assert(drawn_color("TAP REPLACE AGAIN") == COLOR_YELLOW);
    delete_slot();
    assert(active_state->action_pending && fake_settings_write_calls == 0);
    assert(drawn_color("TAP DELETE AGAIN") == COLOR_RED);
    next_slot(); previous_slot();
    assert(!active_state->action_pending);
    delete_slot();
    screen_job(); screen_job(); screen_job();
    assert(!active_state->action_pending);
    delete_slot();
    assert(fake_settings_write_calls == 0);
    back(); browse();
    assert(!active_state->action_pending && active_state->slot == 2);
    delete_slot();
    fake_overlay = 1;
    show_screen(active_state);
    assert(active_state->page && active_state->slot == 2 && !active_state->action_pending);
    save_here();
    fake_fixed = 0;
    save_here();
    assert(!active_state->action_pending && fake_settings_write_calls == 0);
    assert(near(active_state->waypoint.latitude_deg, 2, 1e-7));
    fix_at(45000000);
    save_here(); save_here();
    assert(fake_settings_write_calls == 1 && near(active_state->waypoint.latitude_deg, 15, 1e-7));
    delete_slot(); delete_slot();
    assert(fake_settings_write_calls == 2 && !(active_state->collection.occupied_mask & 4));
    assert(active_state->collection.selected == 0);
    delete_slot(); select_slot();
    assert(active_state->page && !active_state->action_pending && fake_settings_write_calls == 2);
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
    new_point();
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
    assert(active_state->page && active_state->collection.selected == 0);
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
    expect_distance(20000000, NAV_UNITS_METRIC, 200000, "KM", 36);
    assert(drawn_color("FIX READY") == COLOR_GREEN);
    assert(drawn_color("TO HOME") == COLOR_AQUA);
    active_state->has_fix = 0;
    draw_screen(active_state);
    assert(rectangles == 0 && drawn_color("NEAR TARGET") == -1);
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

static void test_render_scheduling_and_overlay(void)
{
    int before_paints, before_clears, before_polls, before_flags;
    reset_fakes(); enter_app();
    fix_at(45000000); new_point(); save_here(); back();
    fix_at(45000300); screen_job();
    assert(active_state->displayed.distance == 11 && rectangles == 4);
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
    click(44, 15);
    assert(!active_state->gps_on && fake_sensor_mode == 0);
    before_polls = polls; before_flags = fake_display_state_calls;
    screen_job();
    assert(polls == before_polls && fake_display_state_calls == before_flags);
    click(44, 15);
    assert(active_state->gps_on && fake_sensor_mode == 1);
    browse();
    key_press_screen();
    assert(fake_animated_exit == 1 && fake_sensor_mode == 0 && !fake_update_enabled);
}
#else
static void test_diagnostic_lifecycle_and_storage_isolation(void)
{
    reset_fakes();
    set_fix(0x0c, 45000000, NAVI_NORTH_HEMISPHERE,
            75000000, NAVI_EAST_HEMISPHERE);
    enter_app();
    assert(active_state->gps_on && active_state->has_fix);
    assert(fake_display_state_calls == 2);
    assert(fake_display_state_parameters[0] == 8 && fake_display_state_values[0] == 1);
    assert(fake_display_state_parameters[1] == 2 && fake_display_state_values[1] == 1);
    assert(fake_settings_size_calls == 0 && fake_settings_read_calls == 0 &&
           fake_settings_write_calls == 0);

    /* The former save hit target is display-only in the diagnostic build. */
    click(40, 140);
    assert(active_state->gps_on && active_state->has_fix);
    assert(fake_settings_size_calls == 0 && fake_settings_read_calls == 0 &&
           fake_settings_write_calls == 0);

    click(10, 30);
    assert(!active_state->gps_on && !active_state->has_fix && fake_sensor_mode == 0);
    click(10, 30);
    assert(active_state->gps_on && active_state->has_fix && fake_sensor_mode == 1);
    assert(fake_settings_size_calls == 0 && fake_settings_read_calls == 0 &&
           fake_settings_write_calls == 0);
}
#endif

/* This checks scheduling/calls, not the actual firmware's timeout semantics. */
static void test_active_gps_display_refresh(void)
{
    int before;
    reset_fakes();
    enter_app();
    before = fake_display_state_calls;
    screen_job();
    assert(fake_display_state_calls == before + 2);
    assert(fake_display_state_parameters[before] == 8 && fake_display_state_values[before] == 1);
    assert(fake_display_state_parameters[before + 1] == 2 && fake_display_state_values[before + 1] == 1);
    click(10, 30);
    assert(!active_state->gps_on);
    before = fake_display_state_calls;
    screen_job();
    assert(fake_display_state_calls == before);
    exit_app(active_state);
    assert(!fake_update_enabled && fake_sensor_mode == 0);
}

#ifndef HOST_TEST_MAIN
#define HOST_TEST_MAIN main
#endif
int HOST_TEST_MAIN(void)
{
    test_active_gps_display_refresh();
#if DIAGNOSTIC
    test_diagnostic_lifecycle_and_storage_isolation();
    puts("diagnostic watch integration tests passed");
#else
    test_gps_validation_and_fix_loss();
    test_create_select_reload_and_units();
    test_full_slots_and_safe_actions();
    test_storage_faults_and_legacy();
    test_distance_and_color();
    test_stationary_fix_guard();
    test_render_scheduling_and_overlay();
    puts("watch integration tests passed");
#endif
    return 0;
}
