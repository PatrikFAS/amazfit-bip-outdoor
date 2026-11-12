#ifndef BAROMETER_TEST_LIBBIP_H
#define BAROMETER_TEST_LIBBIP_H
#include <stddef.h>
typedef struct { int ready; unsigned int pressure; float altitude; int latitude, ns, longitude, ew; } navi_struct_;
typedef struct { void (*elf_finish)(void *); } Elf_proc_;
struct gesture_ { char gesture; int touch_pos_x, touch_pos_y; };
struct regmenu_ { unsigned char curr_scr, swipe_scr, overlay; void *dispatch_func, *key_press, *scr_job_func, *scr_job_buff, *show_menu_func, *show_menu_buff, *long_key_press; };
#define GESTURE_CLICK 1
#define GESTURE_SWIPE_RIGHT 5
#define COLOR_BLACK 0
#define COLOR_WHITE 1
#define VIDEO_Y 176
#define ELF_INDEX_SELF 0
#define ANIMATE_RIGHT 1
void *_memclr(void *, size_t); int _memcmp(const void *, const void *, size_t); int _sprintf(char *, const char *, ...);
void *pvPortMalloc(size_t); void *get_ptr_temp_buf_2(void); int get_var_menu_overlay(void); void reg_menu(void *, int);
Elf_proc_ *get_proc_by_addr(void *); void show_watchface(void); void switch_gps_pressure_sensors(int); void get_navi_data(navi_struct_ *);
void set_update_period(int, int); void set_bg_color(int); void fill_screen_bg(void); void set_graph_callback_to_ram_1(void); void load_font(void); void set_fg_color(int); void GUI_UC_SetEncodeUTF8(void); void text_out_center(const char *, int, int); void text_out(const char *, int, int); void repaint_screen_lines(int, int); int show_menu_animate(void *, int, int);
int set_display_state_value(int, int);
int ElfGetSettingsSize(int); int ElfReadSettings(int, void *, int, int); int ElfWriteSettings(int, const void *, int, int);
#endif
