#ifndef TEST_TIDES_STUB_LIBBIP_H
#define TEST_TIDES_STUB_LIBBIP_H
#include <stddef.h>
#include <stdint.h>
typedef struct { void (*elf_finish)(void *); } Elf_proc_;
struct datetime_ { unsigned short year; unsigned char month,day,hour,min,sec,weekday,h24; };
struct gesture_ { char gesture; int touch_pos_x,touch_pos_y; };
struct regmenu_ { unsigned char curr_scr,swipe_scr,overlay; void *dispatch_func,*key_press,*scr_job_func,*scr_job_buff,*show_menu_func,*show_menu_buff,*long_key_press; };
#define GESTURE_CLICK 1
#define GESTURE_SWIPE_UP 2
#define GESTURE_SWIPE_LEFT 4
#define GESTURE_SWIPE_RIGHT 5
#define COLOR_BLACK 0
#define COLOR_WHITE 0xFFFFFF
#define COLOR_RED 0x0000FF
#define COLOR_GREEN 0x00FF00
#define COLOR_YELLOW 0x00FFFF
#define COLOR_AQUA 0xFFFF00
#define VIDEO_Y 176
#define ANIMATE_RIGHT 1
void *_memclr(void *,size_t);
int _sprintf(char *,const char *,...);
void *pvPortMalloc(size_t);
void *get_ptr_temp_buf_2(void);
int get_var_menu_overlay(void);
void reg_menu(void *,int);
Elf_proc_ *get_proc_by_addr(void *);
void show_watchface(void);
void get_current_date_time(struct datetime_ *);
void set_update_period(int,int);
int set_display_state_value(int,int);
void set_bg_color(int);
void fill_screen_bg(void);
void load_font(void);
void set_fg_color(int);
void GUI_UC_SetEncodeUTF8(void);
void text_out(const char *,int,int);
void text_out_center(const char *,int,int);
void draw_filled_rect(int,int,int,int);
int show_menu_animate(void *,int,int);
void repaint_screen_lines(int,int);
#endif
