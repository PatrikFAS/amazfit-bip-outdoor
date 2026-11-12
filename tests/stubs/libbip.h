#ifndef TEST_STUB_LIBBIP_H
#define TEST_STUB_LIBBIP_H

#include <stddef.h>

typedef struct {
    int ready;
    unsigned int pressure;
    float altitude;
    int latitude;
    int ns;
    int longitude;
    int ew;
} navi_struct_;

typedef struct {
    void (*elf_finish)(void *);
} Elf_proc_;

struct gesture_ { char gesture; int touch_pos_x, touch_pos_y; };
struct regmenu_ {
    unsigned char curr_scr, swipe_scr, overlay;
    void *dispatch_func, *key_press, *scr_job_func, *scr_job_buff;
    void *show_menu_func, *show_menu_buff, *long_key_press;
};

#define NAVI_NORTH_HEMISPHERE 0
#define NAVI_SOUTH_HEMISPHERE 1
#define NAVI_WEST_HEMISPHERE 2
#define NAVI_EAST_HEMISPHERE 3
#define IS_NAVI_GPS_LAT_READY(navi) ((navi) & 0x04)
#define IS_NAVI_GPS_LONG_READY(navi) ((navi) & 0x08)
#define GESTURE_CLICK 1
#define GESTURE_SWIPE_LEFT 4
#define GESTURE_SWIPE_RIGHT 5
#define COLOR_BLACK 0
#define COLOR_WHITE 0xFFFFFF
#define COLOR_RED 0x0000FF
#define COLOR_GREEN 0x00FF00
#define COLOR_YELLOW 0x00FFFF
#define COLOR_AQUA 0xFFFF00
#define VIDEO_Y 176
#define ELF_INDEX_SELF 0
#define ANIMATE_RIGHT 1
#define SENSOR_TYPE_COMPASS 3
#define APP_STATE_CHARGE_PLUGGED 0x10
#define IS_CHARGE_PLUGGED check_app_state(APP_STATE_CHARGE_PLUGGED)

void *_memclr(void *ptr, size_t size);
int _memcmp(const void *a, const void *b, size_t size);
int _sprintf(char *out, const char *format, ...);
void *pvPortMalloc(size_t size);
void *get_ptr_temp_buf_2(void);
int get_var_menu_overlay(void);
void reg_menu(void *menu, int param);
Elf_proc_ *get_proc_by_addr(void *addr);
void show_watchface(void);
void switch_gps_pressure_sensors(int mode);
int sensors_enable(int sensor_type, int enable);
void compass_task_resume(void);
short compass_get_degree(void);
int compass_need_calibration(void);
void get_navi_data(navi_struct_ *data);
int is_gps_fixed(void);
unsigned int get_tick_count(void);
void set_update_period(int enable, int period);
int set_display_state_value(int parameter, int value);
int get_battery_charge(void);
int get_var_free_ram(void);
int get_fw_version(void);
int get_bipos_build(void);
int check_app_state(long long state);
void set_bg_color(int color);
void fill_screen_bg(void);
void draw_filled_rect(int x1, int y1, int x2, int y2);
void set_graph_callback_to_ram_1(void);
void load_font(void);
void set_fg_color(int color);
void GUI_UC_SetEncodeUTF8(void);
void text_out_center(const char *text, int x, int y);
void text_out(const char *text, int x, int y);
void repaint_screen_lines(int start, int end);
int show_menu_animate(void *fn, int param, int animation);
int ElfGetSettingsSize(int index);
int ElfReadSettings(int index, void *buffer, int offset, int length);
int ElfWriteSettings(int index, const void *buffer, int offset, int length);

#endif
