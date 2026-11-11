#include <libbip.h>

#include <stdint.h>

#define SENSOR_DISABLE 0
#define SENSOR_ENABLE 1
#define UPDATE_MS 1000

typedef struct {
    void *ret_f;
    Elf_proc_ *proc;
    short heading;
    int sensor_on;
    int calibration_needed;
    int heading_valid;
} compass_state_t;

static void show_screen(void *param);
static void screen_job(void);
static int dispatch_screen(void *param);
static void key_press_screen(void);
static void poll_compass(compass_state_t *state);
static void draw_screen(compass_state_t *state);
static void exit_app(compass_state_t *state);

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
    compass_state_t **slot = (compass_state_t **)get_ptr_temp_buf_2();
    compass_state_t *state;
    Elf_proc_ *proc;
    if (!slot) return;
    if (*slot && param == *slot && get_var_menu_overlay()) {
        state = *slot;
        *slot = 0;
        reg_menu(&screen_data, 0);
        *slot = state;
    } else {
        reg_menu(&screen_data, 0);
        *slot = (compass_state_t *)pvPortMalloc(sizeof(*state));
        state = *slot;
        if (!state) { show_watchface(); return; }
        _memclr(state, sizeof(*state));
        proc = get_proc_by_addr((void *)main);
        state->proc = proc;
        state->ret_f = (param && proc && proc->elf_finish) ?
                       (void *)proc->elf_finish : (void *)show_watchface;
        compass_task_resume();
        sensors_enable(SENSOR_TYPE_COMPASS, SENSOR_ENABLE);
        state->sensor_on = 1;
    }
    poll_compass(state);
    draw_screen(state);
    set_display_state_value(8, 1);
    set_display_state_value(2, 1);
    set_update_period(1, UPDATE_MS);
}

static void poll_compass(compass_state_t *state)
{
    if (!state || !state->sensor_on) {
        if (state) { state->heading_valid = 0; state->calibration_needed = 0; }
        return;
    }
    state->calibration_needed = compass_need_calibration() != 0;
    state->heading = compass_get_degree();
    state->heading_valid = !state->calibration_needed &&
                           state->heading >= 0 && state->heading < 360;
}

static void screen_job(void)
{
    compass_state_t **slot = (compass_state_t **)get_ptr_temp_buf_2();
    if (!slot || !*slot) return;
    poll_compass(*slot);
    draw_screen(*slot);
    repaint_screen_lines(0, VIDEO_Y);
    if ((*slot)->sensor_on) {
        set_display_state_value(8, 1);
        set_display_state_value(2, 1);
    }
    set_update_period(1, UPDATE_MS);
}

static int dispatch_screen(void *param)
{
    compass_state_t **slot = (compass_state_t **)get_ptr_temp_buf_2();
    struct gesture_ *gesture = (struct gesture_ *)param;
    if (!slot || !*slot || !gesture) return 0;
    if (gesture->gesture == GESTURE_SWIPE_RIGHT) exit_app(*slot);
    return 0;
}

static void key_press_screen(void)
{
    compass_state_t **slot = (compass_state_t **)get_ptr_temp_buf_2();
    if (slot && *slot) exit_app(*slot);
}

static void exit_app(compass_state_t *state)
{
    set_update_period(0, 0);
    if (state && state->sensor_on) {
        sensors_enable(SENSOR_TYPE_COMPASS, SENSOR_DISABLE);
        state->sensor_on = 0;
    }
    show_menu_animate(state ? state->ret_f : (void *)show_watchface,
                      (int)(uintptr_t)show_screen, ANIMATE_RIGHT);
}

static void draw_screen(compass_state_t *state)
{
    char value[19];
    set_bg_color(COLOR_BLACK);
    fill_screen_bg();
    load_font();
    set_fg_color(COLOR_WHITE);
    GUI_UC_SetEncodeUTF8();
    text_out_center("COMPASS CHECK", 88, 12);
    text_out_center("RAW HEADING", 88, 40);
    if (state->calibration_needed) {
        text_out_center("CALIBRATION NEEDED", 88, 68);
        text_out_center("REF UNVERIFIED", 88, 94);
    } else if (state->heading_valid) {
        _sprintf(value, "%d deg", (int)state->heading);
        text_out_center(value, 88, 68);
        text_out_center("REF UNVERIFIED", 88, 94);
    } else {
        text_out_center("UNAVAILABLE", 88, 68);
        text_out_center("REF UNVERIFIED", 88, 94);
    }
}
