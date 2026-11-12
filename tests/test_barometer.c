#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define main barometer_entry
#include "../src/barometer.c"
#undef main

static barometer_state_t *active;
static navi_struct_ sample;
static Elf_proc_ proc;
static unsigned char settings[32];
static int settings_size, sensor_mode, update_enabled, exited, keep_open_calls;
static int fail_readback;
static int max_text_y;
#ifdef WATCH_PREVIEW
static int capture_screen;
#endif
static int overlay;
static int allocation_fails;

void *_memclr(void *p, size_t n) { return memset(p, 0, n); }
int _memcmp(const void *a, const void *b, size_t n) { return memcmp(a, b, n); }
int _sprintf(char *o, const char *f, ...) { va_list a; int n; va_start(a, f); n = vsprintf(o, f, a); va_end(a); return n; }
void *pvPortMalloc(size_t n) { if (allocation_fails) return NULL; active = calloc(1, n); return active; }
void *get_ptr_temp_buf_2(void) { return &active; }
int get_var_menu_overlay(void) { return overlay; }
void reg_menu(void *m, int p) { (void)m; (void)p; }
Elf_proc_ *get_proc_by_addr(void *a) { (void)a; return &proc; }
void show_watchface(void) {}
void switch_gps_pressure_sensors(int m) { sensor_mode = m; }
void get_navi_data(navi_struct_ *d) { *d = sample; }
void set_update_period(int e, int p) { (void)p; update_enabled = e; }
int set_display_state_value(int item, int value) { assert((item == 8 || item == 2) && value == 1); ++keep_open_calls; return 0; }
void set_bg_color(int c) { (void)c; } void fill_screen_bg(void) {} void set_graph_callback_to_ram_1(void) {} void load_font(void) {} void set_fg_color(int c) { (void)c; } void GUI_UC_SetEncodeUTF8(void) {}
static void check_text(const char *s, int x, int y, int centered)
{
    int width = (int)strlen(s) * 9;
    int left = centered ? x - width / 2 : x;
    assert(strlen(s) <= 18 && left >= 0 && left + width <= 176);
    assert(y >= 0 && y + 16 <= 176);
    if (y > max_text_y) max_text_y = y;
}
void text_out_center(const char *s, int x, int y)
{
    check_text(s, x, y, 1);
#ifdef WATCH_PREVIEW
    if (capture_screen) printf("C\t%d\t%d\t%s\n", x, y, s);
#endif
}
void text_out(const char *s, int x, int y)
{
    check_text(s, x, y, 0);
#ifdef WATCH_PREVIEW
    if (capture_screen) printf("T\t%d\t%d\t%s\n", x, y, s);
#endif
}
void repaint_screen_lines(int a, int b) { (void)a; (void)b; }
int show_menu_animate(void *f, int p, int a) { (void)f; (void)p; (void)a; ++exited; return 0; }
int ElfGetSettingsSize(int i) { (void)i; return settings_size; }
int ElfReadSettings(int i, void *b, int o, int n) { (void)i; if (n > settings_size - o) n = settings_size - o; if (n <= 0) return 0; memcpy(b, settings + o, (size_t)n); if (fail_readback) { ((unsigned char *)b)[0] ^= 1; fail_readback = 0; } return n; }
int ElfWriteSettings(int i, const void *b, int o, int n) { (void)i; memcpy(settings + o, b, (size_t)n); if (settings_size < o + n) settings_size = o + n; return n; }

static void reset(void)
{
    memset(&sample, 0, sizeof(sample)); memset(settings, 0, sizeof(settings));
    active = NULL; settings_size = 0; sensor_mode = -1; update_enabled = 0; exited = 0; keep_open_calls = 0; fail_readback = 0; max_text_y = 0; overlay = 0; allocation_fails = 0;
}

static void test_pressure_and_reference(void)
{
    barometer_state_t state;
    memset(&state, 0, sizeof(state)); state.reference_hpa = REF_DEFAULT_HPA; state.sensor_on = 1;
    sample.ready = PRESS_READY; sample.pressure = 101325; poll_pressure(&state);
    assert(state.pressure_ready && fabsf(state.pressure_hpa - 1013.25f) < 0.01f);
    assert(fabsf(state.altitude_m) < 0.01f);
    sample.pressure = 90000; poll_pressure(&state);
    assert(state.pressure_ready && state.altitude_m > 900.0f && state.altitude_m < 1100.0f);
    sample.pressure = 130000; poll_pressure(&state); assert(!state.pressure_ready);
    sample.pressure = 100000; sample.ready = 0; poll_pressure(&state); assert(!state.pressure_ready);
    state.sensor_on = 0; sample.ready = PRESS_READY; poll_pressure(&state); assert(!state.pressure_ready);
}

static void test_settings_and_exit(void)
{
    struct gesture_ gesture;
    barometer_state_t loaded;
    reset();
    memset(&loaded, 0, sizeof(loaded)); loaded.reference_hpa = REF_DEFAULT_HPA;
    assert(!load_reference(&loaded) && loaded.reference_hpa == REF_DEFAULT_HPA);
    active = calloc(1, sizeof(*active)); active->reference_hpa = REF_DEFAULT_HPA; active->sensor_on = 1;
    sensor_mode = SENSOR_ENABLE; update_enabled = 1;
    gesture.gesture = GESTURE_CLICK; gesture.touch_pos_x = 150; gesture.touch_pos_y = 150;
    dispatch_screen(&gesture);
    assert(active->reference_hpa == REF_DEFAULT_HPA + 1.0f && settings_size == SETTINGS_SIZE);
    memset(&loaded, 0, sizeof(loaded)); assert(load_reference(&loaded));
    assert(fabsf(loaded.reference_hpa - active->reference_hpa) < .01f);
    settings[9] ^= 0x80;
    memset(&loaded, 0, sizeof(loaded)); loaded.reference_hpa = REF_DEFAULT_HPA;
    assert(!load_reference(&loaded) && loaded.reference_hpa == REF_DEFAULT_HPA);
    settings[9] ^= 0x80;
    active->reference_hpa = REF_MAX_HPA;
    dispatch_screen(&gesture);
    assert(active->reference_hpa == REF_MAX_HPA);
    fail_readback = 1; active->reference_hpa = REF_DEFAULT_HPA;
    dispatch_screen(&gesture);
    assert(active->reference_hpa == REF_DEFAULT_HPA && active->settings_error);
    memset(&loaded, 0, sizeof(loaded)); loaded.reference_hpa = REF_DEFAULT_HPA;
    assert(load_reference(&loaded) && loaded.reference_hpa == REF_DEFAULT_HPA);
    draw_screen(active); assert(max_text_y <= 158);
    gesture.gesture = GESTURE_SWIPE_RIGHT; dispatch_screen(&gesture);
    assert(sensor_mode == SENSOR_ENABLE && active->sensor_on && update_enabled && exited == 0);
    key_press_screen();
    assert(sensor_mode == SENSOR_DISABLE && !active->sensor_on && !update_enabled && exited == 1);
    free(active); active = NULL;
}

static void test_overlay_and_allocation_failure(void)
{
    reset();
    allocation_fails = 1;
    show_screen(NULL);
    assert(active == NULL);
    allocation_fails = 0;
    show_screen(NULL);
    assert(active != NULL && active->sensor_on && sensor_mode == SENSOR_ENABLE && keep_open_calls == 2);
    screen_job();
    assert(!active->pressure_ready && keep_open_calls == 4);
    sample.ready = PRESS_READY; sample.pressure = 101325;
    screen_job();
    assert(active->pressure_ready && keep_open_calls == 6);
    {
        barometer_state_t *retained = active;
        overlay = 1;
        show_screen(retained);
        assert(active == retained && active->sensor_on && keep_open_calls == 8);
    }
    exit_app(active);
    assert(sensor_mode == SENSOR_DISABLE && keep_open_calls == 8);
    free(active); active = NULL;
}

#ifdef WATCH_PREVIEW
static void preview(const char *name, barometer_state_t *state)
{
    printf("SCREEN\t%s\n", name);
    capture_screen = 1;
    draw_screen(state);
    capture_screen = 0;
}

static void preview_screens(void)
{
    barometer_state_t state;
    memset(&state, 0, sizeof(state)); state.reference_hpa = REF_DEFAULT_HPA;
    preview("barometer standard reference unavailable", &state);
    state.sensor_on = 1; sample.ready = PRESS_READY; sample.pressure = 100860;
    poll_pressure(&state);
    preview("barometer standard reference valid", &state);
    state.reference_custom = 1; state.reference_hpa = 1018.25f;
    poll_pressure(&state);
    preview("barometer user reference valid", &state);
    state.settings_error = 1;
    preview("barometer failed save", &state);
}
#endif

int main(void)
{
    reset(); test_pressure_and_reference(); test_settings_and_exit(); test_overlay_and_allocation_failure();
#ifdef WATCH_PREVIEW
    preview_screens();
#endif
    puts("barometer tests passed");
    return 0;
}
