#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TIDES_SUN_TEST 1
#define main tides_sun_entry
#include "../src/tides_sun.c"
#undef main

#define MAX_CALLS 64
unsigned char tides_config_data[TIDES_CONFIG_DATA_SIZE];
static struct { int x,y; char text[48]; } calls[MAX_CALLS];
static int call_count, fake_overlay, fake_malloc_fail, fake_watchface;
static int fake_updates, fake_period, fake_exit, fake_registrations;
static struct datetime_ fake_datetime;
static tides_sun_state_t *active_state;
static Elf_proc_ fake_proc;

void *_memclr(void *p, size_t n) { return memset(p, 0, n); }
int _sprintf(char *out, const char *fmt, ...)
{
    int n; va_list ap; va_start(ap,fmt); n=vsnprintf(out,48,fmt,ap); va_end(ap); return n;
}
void *pvPortMalloc(size_t n)
{
    static tides_sun_state_t state;
    assert(n == sizeof(state));
    if (fake_malloc_fail) return NULL;
    memset(&state,0,sizeof(state)); active_state=&state; return &state;
}
void *get_ptr_temp_buf_2(void) { return &active_state; }
int get_var_menu_overlay(void) { return fake_overlay; }
void reg_menu(void *menu,int param) { (void)menu;(void)param;++fake_registrations; }
Elf_proc_ *get_proc_by_addr(void *addr) { (void)addr; return &fake_proc; }
void show_watchface(void) { ++fake_watchface; }
void get_current_date_time(struct datetime_ *dt) { *dt=fake_datetime; }
void set_update_period(int on,int period) { fake_updates=on;fake_period=period; }
void set_bg_color(int color) { (void)color; }
void fill_screen_bg(void) {}
void load_font(void) {}
void set_fg_color(int color) { (void)color; }
void GUI_UC_SetEncodeUTF8(void) {}
void text_out(const char *s,int x,int y)
{
    assert(call_count<MAX_CALLS); calls[call_count].x=x;calls[call_count].y=y;
    snprintf(calls[call_count].text,sizeof(calls[call_count].text),"%s",s);++call_count;
}
int show_menu_animate(void *fn,int param,int anim)
{ (void)fn;(void)param;(void)anim;++fake_exit;return 0; }
void repaint_screen_lines(int a,int b) { (void)a;(void)b; }

static void put16(unsigned char *p,unsigned v) { p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8); }
static void put32(unsigned char *p,uint32_t v)
{ p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24); }
static uint32_t crc32(const unsigned char *p,size_t n)
{
    uint32_t c=UINT32_MAX;size_t i;int b;
    for(i=0;i<n;i++){c^=p[i];for(b=0;b<8;b++)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}
    return c^UINT32_MAX;
}
static void bytestr(unsigned char *dst,const char *s,size_t cap,unsigned char *len)
{
    size_t n=strlen(s);assert(n<cap);*len=(unsigned char)n;memcpy(dst,s,n);
}
static void make_table(uint32_t high_utc,uint32_t low_utc,uint32_t expiry)
{
    unsigned char *d=tides_config_data;
    memset(d,0,TIDES_CONFIG_DATA_SIZE);memcpy(d,"BIPT",4);d[4]=1;d[5]=2;
    put32(d+8,1794000000u);put32(d+12,expiry);
    put32(d+16,(uint32_t)493375000);put32(d+20,(uint32_t)-1232535830);
    put16(d+24,(unsigned)(int16_t)-420);
    bytestr(d+32,"CHS07795",32,&d[26]);bytestr(d+64,"CHS API",32,&d[27]);
    bytestr(d+96,"CHART DATUM",48,&d[28]);
    put32(d+144,high_utc);put16(d+148,(unsigned)(int16_t)-25);d[150]=OUTDOOR_TIDE_HIGH;
    put32(d+152,low_utc);put16(d+156,(unsigned)(int16_t)123);d[158]=OUTDOOR_TIDE_LOW;
    put32(d+160,crc32(d,160));
}
static void reset(void)
{
    call_count=fake_overlay=fake_malloc_fail=fake_watchface=0;
    fake_updates=fake_period=fake_exit=fake_registrations=0;active_state=NULL;
    memset(&fake_proc,0,sizeof(fake_proc));
    fake_datetime.year=2026;fake_datetime.month=11;fake_datetime.day=8;
    fake_datetime.hour=10;fake_datetime.min=0;fake_datetime.sec=0;
    make_table(1794159000u,1794166200u,1794500000u);
}
static void enter(void) { show_screen(NULL);assert(active_state); }
static void render(void) { call_count=0;draw_screen(active_state); }
static const char *find_text(const char *prefix)
{
    int i;for(i=0;i<call_count;i++)if(strncmp(calls[i].text,prefix,strlen(prefix))==0)return calls[i].text;
    return NULL;
}
static void assert_text_bounds(void)
{
    int i;for(i=0;i<call_count;i++){
        size_t n=strlen(calls[i].text);
        assert(n<=18);assert(calls[i].x>=0 && calls[i].x+(int)n*9<=176);
        assert(calls[i].y>=0 && calls[i].y+16<=176);
    }
}
static void test_pages_and_display_bounds(void)
{
    struct gesture_ g={GESTURE_CLICK,100,80};int i;
    reset();enter();assert(fake_updates&&fake_period==UPDATE_MS);
    assert(current_utc_seconds()==1794157200u);
    {
        outdoor_date_t d;int m;
        epoch_local(1794096000u,-420,&d,&m);
        assert(d.year==2026&&d.month==11&&d.day==7&&m==17*60);
    }
    for(i=0;i<4;i++){
        render();assert_text_bounds();
        if(i==0){assert(find_text("RISE "));assert(find_text("DAY LEFT "));assert(find_text("DAYLENGTH "));}
        if(i==1){assert(find_text("HIGH 11/08 10:30"));assert(find_text("HEIGHT -0.25M"));assert(find_text("LOW  11/08 12:30"));}
        if(i==2){assert(find_text("POINT ATK"));assert(find_text("UTC -07:00"));assert(find_text("EXP 11/12"));}
        if(i==3){assert(find_text("NAUT AM "));assert(find_text("NAUT PM "));}
        (void)dispatch_screen(&g);
    }
    assert(active_state->page==0);
}
static void test_conversion_empty_future_and_expiry(void)
{
    struct gesture_ g={GESTURE_SWIPE_LEFT,0,0};
    reset();enter();active_state->page=PAGE_TIDES;render();
    assert(find_text("HIGH 11/08 10:30"));
    make_table(1794140000u,1794145000u,1794500000u);
    screen_job();render();assert(find_text("NO NEXT HIGH"));assert(find_text("NO NEXT LOW"));
    make_table(1794140000u,1794145000u,1794157199u);
    /* Future expiry is evaluated on refresh, against the local watch clock converted to UTC. */
    screen_job();
    assert(active_state->tide_status==OUTDOOR_TIDE_EXPIRED);
    render();assert(find_text("TIDE DATA EXPIRED"));
    active_state->tide_status=OUTDOOR_TIDE_EMPTY;active_state->have_tide=0;
    render();assert(find_text("NO TIDE DATA"));assert(find_text("NO CURVE ESTIMATE"));
    active_state->tide_status=OUTDOOR_TIDE_INVALID;render();
    assert(find_text("TIDE DATA INVALID"));
    fake_datetime.month=2;fake_datetime.day=30;
    (void)dispatch_screen(&g);active_state->page=PAGE_TIDES;render();
    assert(find_text("CLOCK UNAVAILABLE"));
}
static void test_overlay_navigation_and_exit(void)
{
    tides_sun_state_t *saved;struct gesture_ g={GESTURE_SWIPE_UP,0,0};
    reset();enter();saved=active_state;fake_overlay=1;show_screen(saved);
    assert(active_state==saved);assert(fake_registrations==2);
    (void)dispatch_screen(&g);assert(active_state->page==1);
    g.gesture=GESTURE_SWIPE_RIGHT;(void)dispatch_screen(&g);
    assert(!fake_updates&&fake_exit==1);
    reset();fake_malloc_fail=1;show_screen(NULL);assert(fake_watchface==1);
}
#ifndef TIDES_TEST_MAIN
#define TIDES_TEST_MAIN main
#endif
int TIDES_TEST_MAIN(void)
{
    test_pages_and_display_bounds();
    test_conversion_empty_future_and_expiry();
    test_overlay_navigation_and_exit();
    puts("tides_sun tests passed");return 0;
}
