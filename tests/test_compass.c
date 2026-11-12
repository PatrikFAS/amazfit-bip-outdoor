#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define main compass_entry
#include "../src/compass_diagnostic.c"
#undef main

static compass_state_t *active;
static Elf_proc_ proc;
static short sample_heading;
static int sample_calibration, enabled, enable_calls, disable_calls;
static int resume_calls, update_enabled, exited, overlay, allocation_fails;
static char drawn[8][20];
static int drawn_count, display_calls;
#ifdef WATCH_PREVIEW
static int capture_screen;
#endif

void *_memclr(void *p, size_t n) { return memset(p, 0, n); }
int _sprintf(char *o, const char *f, ...) { va_list a; int n; va_start(a, f); n = vsprintf(o, f, a); va_end(a); return n; }
void *pvPortMalloc(size_t n) { if (allocation_fails) return NULL; active = calloc(1, n); return active; }
void *get_ptr_temp_buf_2(void) { return &active; }
int get_var_menu_overlay(void) { return overlay; }
void reg_menu(void *m, int p) { (void)m; (void)p; }
Elf_proc_ *get_proc_by_addr(void *a) { (void)a; return &proc; }
void show_watchface(void) {}
int sensors_enable(int type, int on) { assert(type == SENSOR_TYPE_COMPASS); enabled = on; if (on) ++enable_calls; else ++disable_calls; return 0; }
int set_display_state_value(int parameter, int value) { assert((parameter == 8 || parameter == 2) && value == 1); ++display_calls; return 0; }
void compass_task_resume(void) { ++resume_calls; }
short compass_get_degree(void) { return sample_heading; }
int compass_need_calibration(void) { return sample_calibration; }
void set_update_period(int e, int p) { (void)p; update_enabled = e; }
void set_bg_color(int c) { (void)c; } void fill_screen_bg(void) {} void load_font(void) {} void set_fg_color(int c) { (void)c; } void GUI_UC_SetEncodeUTF8(void) {}
void text_out_center(const char *s, int x, int y)
{
    size_t n = strlen(s);
    int left = x - (int)n * 9 / 2;
    assert(n <= 18 && left >= 0 && left + (int)n * 9 <= 176);
    assert(y >= 0 && y + 16 <= 176);
    if (drawn_count < 8) snprintf(drawn[drawn_count++], sizeof(drawn[0]), "%s", s);
#ifdef WATCH_PREVIEW
    if (capture_screen) printf("C\t%d\t%d\t%s\n", x, y, s);
#endif
}
void repaint_screen_lines(int a, int b) { (void)a; (void)b; }
int show_menu_animate(void *f, int p, int a) { (void)f; (void)p; (void)a; ++exited; return 0; }

static int drew(const char *s)
{
    int i;
    for (i = 0; i < drawn_count; ++i) if (!strcmp(drawn[i], s)) return 1;
    return 0;
}
static void clear_drawn(void) { memset(drawn, 0, sizeof(drawn)); drawn_count = 0; }
static void reset(void)
{
    active = NULL; sample_heading = 0; sample_calibration = 0; enabled = 0;
    enable_calls = disable_calls = resume_calls = update_enabled = exited = 0;
    overlay = allocation_fails = display_calls = 0; clear_drawn();
}

static void test_heading_and_calibration(void)
{
    compass_state_t s;
    memset(&s, 0, sizeof(s)); s.sensor_on = 1;
    sample_heading = 0; poll_compass(&s); assert(s.heading_valid && s.heading == 0);
    draw_screen(&s); assert(drew("0 deg") && drew("REF UNVERIFIED"));
    clear_drawn(); sample_heading = 359; poll_compass(&s); assert(s.heading_valid);
    draw_screen(&s); assert(drew("359 deg"));
    sample_heading = -1; poll_compass(&s); assert(!s.heading_valid);
    sample_heading = 360; poll_compass(&s); assert(!s.heading_valid);
    sample_heading = 42; sample_calibration = 1; poll_compass(&s);
    assert(!s.heading_valid && s.calibration_needed);
    clear_drawn(); draw_screen(&s); assert(drew("CALIBRATION NEEDED") && !drew("42 deg"));
}

static void test_lifecycle_overlay_and_exit(void)
{
    struct gesture_ g = { GESTURE_SWIPE_RIGHT, 0, 0 };
    reset(); sample_heading = 359;
    show_screen(NULL);
    assert(active && enabled && enable_calls == 1 && resume_calls == 1);
    assert(active->heading_valid && update_enabled && display_calls == 2);
    screen_job();
    assert(display_calls == 4);
    overlay = 1; show_screen(active);
    assert(active && enable_calls == 1 && resume_calls == 1);
    dispatch_screen(&g);
    assert(enabled && active->sensor_on && update_enabled && disable_calls == 0 && exited == 0);
    key_press_screen();
    assert(!enabled && disable_calls == 1 && !update_enabled && exited == 1);
    free(active); active = NULL;
}

static void test_allocation_failure_and_key_exit(void)
{
    reset(); allocation_fails = 1; show_screen(NULL);
    assert(!active && !enabled && !enable_calls);
    allocation_fails = 0; show_screen(NULL);
    assert(active && enabled);
    key_press_screen(); assert(!enabled && disable_calls == 1 && exited == 1);
    free(active); active = NULL;
}

#ifdef WATCH_PREVIEW
static void preview(const char *name, compass_state_t *state)
{
    printf("SCREEN\t%s\n", name);
    capture_screen = 1;
    draw_screen(state);
    capture_screen = 0;
}
static void preview_screens(void)
{
    compass_state_t state;
    memset(&state, 0, sizeof(state));
    state.sensor_on = 1;
    state.calibration_needed = 1; state.heading = 75;
    preview("Compass: calibration needed", &state);
    state.calibration_needed = 0; state.heading = 0; state.heading_valid = 1;
    preview("Compass: raw 0 degrees", &state);
    state.heading = 359;
    preview("Compass: raw 359 degrees", &state);
    state.heading = 360; state.heading_valid = 0;
    preview("Compass: invalid heading", &state);
}
#endif

int main(void)
{
    test_heading_and_calibration();
    test_lifecycle_overlay_and_exit();
    test_allocation_failure_and_key_exit();
#ifdef WATCH_PREVIEW
    preview_screens();
#endif
    puts("compass tests passed");
    return 0;
}
