#include <libbip.h>
#include "bip_display.h"

#include <math.h>
#include <stdint.h>

#define SENSOR_DISABLE 0
#define SENSOR_ENABLE 1
#define UPDATE_MS 2000
#define PRESS_READY 0x01
#define REF_DEFAULT_HPA 1013.25f
#define REF_MIN_HPA 800.0f
#define REF_MAX_HPA 1100.0f
#define SETTINGS_SIZE 10

typedef struct {
    void *ret_f;
    Elf_proc_ *proc;
    navi_struct_ navi;
    float pressure_hpa;
    float reference_hpa;
    float altitude_m;
    int pressure_ready;
    int sensor_on;
    int reference_custom;
    int settings_error;
} barometer_state_t;

static void show_screen(void *param);
static void screen_job(void);
static int dispatch_screen(void *param);
static void key_press_screen(void);
static void draw_screen(barometer_state_t *state);
static void poll_pressure(barometer_state_t *state);
static void exit_app(barometer_state_t *state);
static int load_reference(barometer_state_t *state);
static int save_reference(barometer_state_t *state);

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
    barometer_state_t **slot = (barometer_state_t **)get_ptr_temp_buf_2();
    barometer_state_t *state;
    Elf_proc_ *proc;
    if (!slot) return;
    if (*slot && param == *slot && get_var_menu_overlay()) {
        state = *slot;
        *slot = 0;
        reg_menu(&screen_data, 0);
        *slot = state;
    } else {
        reg_menu(&screen_data, 0);
        *slot = (barometer_state_t *)pvPortMalloc(sizeof(*state));
        state = *slot;
        if (!state) { show_watchface(); return; }
        _memclr(state, sizeof(*state));
        proc = get_proc_by_addr((void *)main);
        state->proc = proc;
        state->ret_f = (param && proc && proc->elf_finish) ?
                       (void *)proc->elf_finish : (void *)show_watchface;
        state->reference_hpa = REF_DEFAULT_HPA;
        state->reference_custom = load_reference(state);
        switch_gps_pressure_sensors(SENSOR_ENABLE);
        state->sensor_on = 1;
    }
    bip_keep_screen_open();
    poll_pressure(state);
    draw_screen(state);
    set_update_period(1, UPDATE_MS);
}

static void poll_pressure(barometer_state_t *state)
{
    _memclr(&state->navi, sizeof(state->navi));
    if (state->sensor_on) get_navi_data(&state->navi);
    state->pressure_ready = 0;
    if (!state->sensor_on || !(state->navi.ready & PRESS_READY)) return;
    /* Vendor demo reads pressure in Pascals and divides by 100 for hPa. */
    if (state->navi.pressure < 30000u || state->navi.pressure > 120000u) return;
    state->pressure_hpa = (float)state->navi.pressure / 100.0f;
    if (state->pressure_hpa < 300.0f || state->pressure_hpa > 1200.0f) return;
    state->altitude_m = 44330.0f * (1.0f -
        powf(state->pressure_hpa / state->reference_hpa, 0.19029495f));
    state->pressure_ready = 1;
}

static void screen_job(void)
{
    barometer_state_t **slot = (barometer_state_t **)get_ptr_temp_buf_2();
    if (!slot || !*slot) return;
    if ((*slot)->sensor_on) bip_keep_screen_open();
    poll_pressure(*slot);
    draw_screen(*slot);
    repaint_screen_lines(0, VIDEO_Y);
    set_update_period(1, UPDATE_MS);
}

static int dispatch_screen(void *param)
{
    barometer_state_t **slot = (barometer_state_t **)get_ptr_temp_buf_2();
    barometer_state_t *state;
    struct gesture_ *gesture = (struct gesture_ *)param;
    if (!slot || !(state = *slot) || !gesture) return 0;
    if (gesture->gesture == GESTURE_CLICK) {
        if (gesture->touch_pos_y >= 144 && gesture->touch_pos_y <= 172) {
            float previous = state->reference_hpa;
            if (gesture->touch_pos_x < 88 && state->reference_hpa > REF_MIN_HPA) {
                state->reference_hpa -= 1.0f;
                if (state->reference_hpa < REF_MIN_HPA) state->reference_hpa = REF_MIN_HPA;
            } else if (gesture->touch_pos_x >= 88 && state->reference_hpa < REF_MAX_HPA) {
                state->reference_hpa += 1.0f;
                if (state->reference_hpa > REF_MAX_HPA) state->reference_hpa = REF_MAX_HPA;
            }
            if (state->reference_hpa != previous) {
                state->settings_error = !save_reference(state);
                if (state->settings_error) {
                    state->reference_hpa = previous;
                    /* Best effort restore if a write completed but readback failed. */
                    (void)save_reference(state);
                } else state->reference_custom = 1;
            }
            poll_pressure(state);
        }
        draw_screen(state);
        repaint_screen_lines(0, VIDEO_Y);
    }
    return 0;
}

static void key_press_screen(void)
{
    barometer_state_t **slot = (barometer_state_t **)get_ptr_temp_buf_2();
    if (slot && *slot) exit_app(*slot);
}

static void exit_app(barometer_state_t *state)
{
    set_update_period(0, 0);
    if (state && state->sensor_on) {
        switch_gps_pressure_sensors(SENSOR_DISABLE);
        state->sensor_on = 0;
    }
    show_menu_animate(state ? state->ret_f : (void *)show_watchface,
                      (int)(uintptr_t)show_screen, ANIMATE_RIGHT);
}

static void draw_screen(barometer_state_t *state)
{
    char value[19];
    char ref[19];
    set_bg_color(COLOR_BLACK);
    fill_screen_bg();
    load_font();
    set_fg_color(COLOR_WHITE);
    GUI_UC_SetEncodeUTF8();
    text_out_center("BAROMETER", 88, 10);
    if (state->pressure_ready) {
        _sprintf(value, "%.1f hPa", state->pressure_hpa);
        _sprintf(ref, "%+.0f m", state->altitude_m);
        text_out_center(value, 88, 36);
        text_out_center(ref, 88, 60);
    } else {
        text_out_center("PRESSURE", 88, 36);
        text_out_center("UNAVAILABLE", 88, 60);
    }
    _sprintf(ref, "REF %.2f hPa", state->reference_hpa);
    text_out_center(ref, 88, 86);
    text_out_center(state->settings_error ? "REF SAVE FAILED" :
                    state->reference_custom ? "USER REF" : "STANDARD REF", 88, 108);
    text_out_center(state->reference_custom ? "PRESSURE ALT" : "UNCALIBRATED", 88, 128);
    text_out("- REF", 20, 150);
    text_out("+ REF", 124, 150);
}

static uint16_t record_crc(const uint8_t *bytes, unsigned int length)
{
    uint16_t crc = 0xffffu;
    unsigned int i;
    for (i = 0; i < length; ++i) {
        unsigned int bit;
        crc ^= (uint16_t)bytes[i] << 8;
        for (bit = 0; bit < 8; ++bit) {
            uint32_t shifted = (uint32_t)crc << 1;
            if (crc & 0x8000u) shifted ^= 0x1021u;
            crc = (uint16_t)shifted;
        }
    }
    return crc;
}

static int load_reference(barometer_state_t *state)
{
    uint8_t record[SETTINGS_SIZE];
    uint32_t centi_hpa;
    uint16_t stored_crc;
    int count;
    if (ElfGetSettingsSize(ELF_INDEX_SELF) < SETTINGS_SIZE) return 0;
    _memclr(&record, sizeof(record));
    count = ElfReadSettings(ELF_INDEX_SELF, record, 0, SETTINGS_SIZE);
    if (count != SETTINGS_SIZE || record[0] != 'B' || record[1] != 'R' || record[2] != 1) return 0;
    stored_crc = (uint16_t)(record[8] | ((uint16_t)record[9] << 8));
    if (stored_crc != record_crc(record, 8)) return 0;
    centi_hpa = (uint32_t)record[4] | ((uint32_t)record[5] << 8) |
                ((uint32_t)record[6] << 16) | ((uint32_t)record[7] << 24);
    state->reference_hpa = (float)centi_hpa / 100.0f;
    if (state->reference_hpa < REF_MIN_HPA || state->reference_hpa > REF_MAX_HPA) {
        state->reference_hpa = REF_DEFAULT_HPA;
        return 0;
    }
    return 1;
}

static int save_reference(barometer_state_t *state)
{
    uint8_t record[SETTINGS_SIZE], verify[SETTINGS_SIZE];
    uint32_t centi_hpa;
    uint16_t crc;
    int written, read;
    _memclr(&record, sizeof(record));
    record[0] = 'B'; record[1] = 'R'; record[2] = 1;
    centi_hpa = (uint32_t)(state->reference_hpa * 100.0f + 0.5f);
    record[4] = (uint8_t)centi_hpa; record[5] = (uint8_t)(centi_hpa >> 8);
    record[6] = (uint8_t)(centi_hpa >> 16); record[7] = (uint8_t)(centi_hpa >> 24);
    crc = record_crc(record, 8);
    record[8] = (uint8_t)crc; record[9] = (uint8_t)(crc >> 8);
    written = ElfWriteSettings(ELF_INDEX_SELF, &record, 0, SETTINGS_SIZE);
    if (written != SETTINGS_SIZE) return 0;
    _memclr(&verify, sizeof(verify));
    read = ElfReadSettings(ELF_INDEX_SELF, verify, 0, SETTINGS_SIZE);
    return read == SETTINGS_SIZE && _memcmp(record, verify, SETTINGS_SIZE) == 0;
}
