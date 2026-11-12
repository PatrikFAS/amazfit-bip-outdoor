#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TIDES_SUN_TEST 1
#define outdoor_tide_decode tides_sun_counted_tide_decode
#define main tides_sun_entry
#include "../src/tides_sun.c"
#undef main
#undef outdoor_tide_decode

outdoor_tide_status_t outdoor_tide_decode(const uint8_t *,size_t,uint32_t,
    outdoor_tide_info_t *,outdoor_tide_event_t *,size_t);

#define MAX_CALLS 64
#define MAX_RECTS 32
unsigned char tides_config_data[TIDES_CONFIG_DATA_SIZE];
static struct { int x,y,center,color; char text[64]; } calls[MAX_CALLS];
static struct { int x1,y1,x2,y2,color; } rects[MAX_RECTS];
static int call_count, fake_overlay, fake_malloc_fail, fake_watchface;
static int rect_count, foreground;
static int fake_updates, fake_period, fake_exit, fake_registrations;
static int fake_display_calls, fake_repaints, fake_display_first, fake_display_second;
static int fake_decodes;
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
int set_display_state_value(int key,int value)
{
    if (fake_display_calls == 0) fake_display_first = key * 10 + value;
    else if (fake_display_calls == 1) fake_display_second = key * 10 + value;
    ++fake_display_calls;
    return 0;
}
outdoor_tide_status_t tides_sun_counted_tide_decode(const uint8_t *data,
    size_t length,uint32_t now_utc,outdoor_tide_info_t *info,
    outdoor_tide_event_t *events,size_t event_capacity)
{
    ++fake_decodes;
    return outdoor_tide_decode(data,length,now_utc,info,events,event_capacity);
}
void set_bg_color(int color) { (void)color; }
void fill_screen_bg(void) {}
void load_font(void) {}
void set_fg_color(int color) { foreground=color; }
void GUI_UC_SetEncodeUTF8(void) {}
static void capture_text(const char *s,int x,int y,int center)
{
    assert(call_count<MAX_CALLS); calls[call_count].x=x;calls[call_count].y=y;
    calls[call_count].center=center;calls[call_count].color=foreground;
    snprintf(calls[call_count].text,sizeof(calls[call_count].text),"%s",s);++call_count;
}
void text_out(const char *s,int x,int y) { capture_text(s,x,y,0); }
void text_out_center(const char *s,int x,int y) { capture_text(s,x,y,1); }
void draw_filled_rect(int x1,int y1,int x2,int y2)
{
    assert(rect_count<MAX_RECTS);
    rects[rect_count].x1=x1;rects[rect_count].y1=y1;
    rects[rect_count].x2=x2;rects[rect_count].y2=y2;
    rects[rect_count].color=foreground;++rect_count;
}
int show_menu_animate(void *fn,int param,int anim)
{ (void)fn;(void)param;(void)anim;++fake_exit;return 0; }
void repaint_screen_lines(int a,int b) { (void)a;(void)b;++fake_repaints; }

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
    rect_count=0;foreground=COLOR_WHITE;
    fake_updates=fake_period=fake_exit=fake_registrations=0;
    fake_display_calls=fake_repaints=fake_display_first=fake_display_second=0;
    fake_decodes=0;
    active_state=NULL;
    memset(&fake_proc,0,sizeof(fake_proc));
    fake_datetime.year=2026;fake_datetime.month=11;fake_datetime.day=8;
    fake_datetime.hour=10;fake_datetime.min=0;fake_datetime.sec=0;
    make_table(1794159000u,1794166200u,1794500000u);
}
static void enter(void) { show_screen(NULL);assert(active_state); }
static void render(void) { call_count=0;rect_count=0;draw_screen(active_state); }
static const char *exact_text(const char *text)
{
    int i;for(i=0;i<call_count;i++)if(strcmp(calls[i].text,text)==0)return calls[i].text;
    return NULL;
}
static int text_color(const char *text)
{
    int i;for(i=0;i<call_count;i++)if(strcmp(calls[i].text,text)==0)return calls[i].color;
    return -1;
}
static int text_x(const char *text)
{
    int i;for(i=0;i<call_count;i++)if(strcmp(calls[i].text,text)==0)return calls[i].x;
    return -1;
}
static int is_centered(const char *text)
{
    int i;for(i=0;i<call_count;i++)if(strcmp(calls[i].text,text)==0)return calls[i].center;
    return 0;
}
static int text_count(const char *text)
{
    int i,count=0;for(i=0;i<call_count;i++)if(strcmp(calls[i].text,text)==0)++count;
    return count;
}
static int text_at(const char *text,int x,int y)
{
    int i;for(i=0;i<call_count;i++)
        if(strcmp(calls[i].text,text)==0&&calls[i].x==x&&calls[i].y==y)return 1;
    return 0;
}
static void set_tide_list_fixture(void)
{
    uint32_t now=current_utc_seconds();
    active_state->tide_status=OUTDOOR_TIDE_OK;
    active_state->have_tide=1;
    active_state->tide_info.event_count=8;
    active_state->tide_info.utc_offset_minutes=-420;
    active_state->tide_info.expires_utc=now+86400;
    active_state->tide_events[0]=(outdoor_tide_event_t){now-1,99,OUTDOOR_TIDE_HIGH};
    active_state->tide_events[1]=(outdoor_tide_event_t){now,-10,OUTDOOR_TIDE_LOW};
    active_state->tide_events[2]=(outdoor_tide_event_t){now+3600,45,OUTDOOR_TIDE_HIGH};
    active_state->tide_events[3]=(outdoor_tide_event_t){now+7200,123,OUTDOOR_TIDE_LOW};
    active_state->tide_events[4]=(outdoor_tide_event_t){now+10800,-25,OUTDOOR_TIDE_HIGH};
    active_state->tide_events[5]=(outdoor_tide_event_t){now+14400,0,OUTDOOR_TIDE_LOW};
    active_state->tide_events[6]=(outdoor_tide_event_t){now+18000,10,OUTDOOR_TIDE_HIGH};
    active_state->tide_events[7]=(outdoor_tide_event_t){now+21600,20,OUTDOOR_TIDE_LOW};
}
static void assert_text_bounds(void)
{
    int i;for(i=0;i<call_count;i++){
        size_t n=strlen(calls[i].text);
        int left=calls[i].center?calls[i].x-(int)n*9/2:calls[i].x;
        assert(n<=18);assert(left>=0 && left+(int)n*9<=176);
        assert(calls[i].y>=0 && calls[i].y+16<=176);
    }
    for(i=0;i<rect_count;i++) {
        assert(rects[i].x1>=0&&rects[i].y1>=0);
        assert(rects[i].x2>=rects[i].x1&&rects[i].y2>=rects[i].y1);
        assert(rects[i].x2<176&&rects[i].y2<176);
    }
}
static void test_pages_and_display_bounds(void)
{
    struct gesture_ g={GESTURE_SWIPE_LEFT,0,0};
    const char *const pages[]={"TIDES","SUN","TWILIGHT","INFO"};
    int i;
    reset();enter();assert(fake_updates&&fake_period==2000);
    assert(fake_display_calls==2&&fake_display_first==81&&fake_display_second==21);
    assert(current_utc_seconds()==1794157200u);
    {
        outdoor_date_t d;int m;
        epoch_local(1794096000u,-420,&d,&m);
        assert(d.year==2026&&d.month==11&&d.day==7&&m==17*60);
    }
    assert(active_state->page==PAGE_TIDES);
    for(i=0;i<4;i++){
        render();assert_text_bounds();
        assert(exact_text(pages[i]));assert(exact_text("POINT ATK"));
        if(i<3) {
            assert(exact_text("NEXT >")&&!exact_text("< PREV"));
            assert(is_centered("NEXT >")&&text_color("NEXT >")==COLOR_AQUA);
        } else {
            assert(!exact_text("NEXT >")&&exact_text("< PREV"));
            assert(is_centered("< PREV")&&text_color("< PREV")==COLOR_AQUA);
        }
        assert(is_centered(pages[i])&&is_centered("POINT ATK"));
        assert(text_color(pages[i])==COLOR_AQUA);
        if(i==0){
            assert(exact_text("HIGH")&&exact_text("LOW"));
            assert(exact_text("10:30")&&exact_text("12:30"));
            assert(exact_text("-0.25 m")&&exact_text("11/08"));
            assert(text_color("10:30")==COLOR_GREEN);
            assert(text_x("10:30")==112&&text_x("11/08")==112);
        }
        if(i==1){
            assert(exact_text("SUNRISE")&&exact_text("SUNSET"));
            assert(exact_text("DAYLIGHT")&&exact_text("LEFT"));
            assert(exact_text("08:11")&&exact_text("17:43"));
            assert(exact_text("09:32")&&exact_text("07:43"));
            assert(text_x("08:11")==112&&text_color("07:43")==COLOR_GREEN);
        }
        if(i==2){
            assert(exact_text("CIVIL")&&exact_text("NAUT"));
            assert(exact_text("DAWN")&&exact_text("DUSK"));
            assert(exact_text("07:37")&&exact_text("18:17"));
            assert(exact_text("06:59")&&exact_text("18:55"));
            assert(text_x("07:37")==64&&text_x("18:17")==120);
        }
        if(i==3){
            assert(exact_text("CHS07795"));assert(exact_text("UTC -07:00"));
            assert(exact_text("CHS API"));assert(exact_text("Chart datum"));
            assert(exact_text("EXP 11/12 09:13"));
        }
        if(i<3) (void)dispatch_screen(&g);
    }
    assert(active_state->page==PAGE_INFO);
    call_count=0;
    g.gesture=GESTURE_SWIPE_LEFT;(void)dispatch_screen(&g);
    assert(active_state->page==PAGE_INFO); /* outward swipe stays */
    call_count=0;
    g.gesture=GESTURE_CLICK;g.touch_pos_x=88;g.touch_pos_y=154;
    (void)dispatch_screen(&g);
    assert(active_state->page==PAGE_TWILIGHT); /* Info PREV tap */
    call_count=0;
    g.gesture=GESTURE_SWIPE_RIGHT;(void)dispatch_screen(&g);
    assert(active_state->page==PAGE_SUN);
    call_count=0;
    g.gesture=GESTURE_SWIPE_RIGHT;(void)dispatch_screen(&g);
    assert(active_state->page==PAGE_TIDES);
    call_count=0;
    g.gesture=GESTURE_SWIPE_RIGHT;(void)dispatch_screen(&g);
    assert(active_state->page==PAGE_TIDES); /* outward swipe stays */
    call_count=0;
    g.gesture=GESTURE_SWIPE_UP;(void)dispatch_screen(&g);
    assert(active_state->page==PAGE_SUN); /* retained advance alias */
    key_press_screen();
    assert(!fake_updates&&fake_exit==1); /* physical button exits */
    reset();enter();
    g.gesture=GESTURE_CLICK;g.touch_pos_x=100;g.touch_pos_y=80;
    (void)dispatch_screen(&g);assert(active_state->page==PAGE_SUN);
    call_count=0;(void)dispatch_screen(&g);assert(active_state->page==PAGE_TWILIGHT);
    call_count=0;(void)dispatch_screen(&g);assert(active_state->page==PAGE_INFO);
    call_count=0;(void)dispatch_screen(&g);assert(active_state->page==PAGE_INFO);
    g.touch_pos_x=88;g.touch_pos_y=154;call_count=0;
    (void)dispatch_screen(&g);assert(active_state->page==PAGE_TWILIGHT);
}
static void test_conversion_empty_future_and_expiry(void)
{
    reset();enter();active_state->page=PAGE_TIDES;render();
    assert(exact_text("10:30")&&exact_text("-0.25 m")&&exact_text("11/08"));
    make_table(1794140000u,1794145000u,1794500000u);
    call_count=0;
    {
        int initial_decodes=fake_decodes;
        for(int i=0;i<14;i++) {
            screen_job();
            assert(active_state->tide_status==OUTDOOR_TIDE_OK);
            assert(fake_decodes==initial_decodes);
            assert(call_count==0&&fake_repaints==0);
        }
    }
    assert(fake_period==2000&&fake_display_calls==30);
    {
        int prior_decodes=fake_decodes;
        screen_job();
        assert(fake_decodes==prior_decodes+1);
    }
    assert(fake_repaints==1&&call_count>0);
    render();assert(text_count("--:--")==2);
    assert(text_color("--:--")==COLOR_YELLOW);
    assert(!exact_text("NO NEXT HIGH")&&!exact_text("NO NEXT LOW"));
    assert(!exact_text("11/08")&&!exact_text("-0.25 m")&&!exact_text("1.23 m"));
    make_table(1794140000u,1794145000u,1794157199u);
    /* Expiry is evaluated after 15 heartbeats, about 30 seconds later. */
    {
        int i,prior_decodes=fake_decodes;
        for(i=0;i<14;i++) {
            screen_job();
            assert(active_state->tide_status==OUTDOOR_TIDE_OK);
            assert(fake_decodes==prior_decodes);
        }
    }
    assert(fake_repaints==1);
    screen_job();
    assert(active_state->tide_status==OUTDOOR_TIDE_EXPIRED);
    assert(fake_decodes==3);
    assert(fake_repaints==2);
    render();assert(exact_text("TIDES EXPIRED"));assert(exact_text("UPDATE DATA"));
    assert(text_color("TIDES EXPIRED")==COLOR_YELLOW);
    active_state->tide_status=OUTDOOR_TIDE_EMPTY;active_state->have_tide=0;
    render();assert(exact_text("NO TIDE DATA"));
    active_state->tide_status=OUTDOOR_TIDE_INVALID;render();
    assert(exact_text("TIDE DATA INVALID"));
    assert(text_color("TIDE DATA INVALID")==COLOR_RED);
    fake_datetime.month=2;fake_datetime.day=30;
    active_state->page=PAGE_TIDES;render();assert(exact_text("CLOCK UNAVAILABLE"));
    assert(text_color("CLOCK UNAVAILABLE")==COLOR_RED);
}
static void test_tide_list_navigation_and_content(void)
{
    struct gesture_ tap={GESTURE_CLICK,44,160};
    struct gesture_ swipe={GESTURE_SWIPE_LEFT,0,0};
    struct gesture_ swipe_up={GESTURE_SWIPE_UP,0,0};
    reset();enter();set_tide_list_fixture();
    active_state->page=PAGE_TIDES;render();assert_text_bounds();
    assert(exact_text("ALL TIDES")&&text_x("ALL TIDES")==44);
    assert(exact_text("NEXT >")&&text_x("NEXT >")==132);
    assert(is_centered("ALL TIDES")&&is_centered("NEXT >"));
    (void)dispatch_screen(&tap);
    assert(active_state->page==PAGE_TIDE_LIST&&active_state->list_page==0);
    render();assert_text_bounds();
    assert(exact_text("TIDE LIST"));
    assert(text_at("LOW",8,31)&&text_at("10:00",112,31));
    assert(text_at("-0.10 m",8,53)&&text_at("11/08",112,53));
    assert(text_at("HIGH",8,71)&&text_at("11:00",112,71));
    assert(text_at("0.45 m",8,93)&&text_at("11/08",112,93));
    assert(text_at("LOW",8,111)&&text_at("12:00",112,111));
    assert(text_at("1.23 m",8,133)&&text_at("11/08",112,133));
    assert(text_color("LOW")==COLOR_AQUA&&text_color("10:00")==COLOR_GREEN);
    assert(text_color("-0.10 m")==COLOR_WHITE);
    assert(exact_text("BACK")&&exact_text("NEXT")&&!exact_text("PREV"));
    assert(text_x("BACK")==88&&text_x("NEXT")==147);

    (void)dispatch_screen(&swipe);
    assert(active_state->list_page==1);render();assert_text_bounds();
    assert(text_at("HIGH",8,31)&&text_at("13:00",112,31));
    assert(text_at("-0.25 m",8,53));
    assert(text_at("LOW",8,71)&&text_at("14:00",112,71));
    assert(text_at("0.00 m",8,93));
    assert(text_at("HIGH",8,111)&&text_at("15:00",112,111));
    assert(text_at("0.10 m",8,133));
    assert(exact_text("PREV")&&exact_text("NEXT"));

    (void)dispatch_screen(&swipe_up);
    assert(active_state->list_page==2);render();assert_text_bounds();
    assert(text_at("LOW",8,31)&&text_at("16:00",112,31));
    assert(text_at("0.20 m",8,53));
    assert(text_count("HIGH")==0&&text_count("LOW")==1);
    assert(exact_text("PREV")&&!exact_text("NEXT"));
    (void)dispatch_screen(&swipe);
    assert(active_state->list_page==2);render();
    assert(text_at("LOW",8,31)&&!exact_text("NEXT"));
    tap.touch_pos_x=29;tap.touch_pos_y=154;(void)dispatch_screen(&tap);
    assert(active_state->list_page==1);render();assert(exact_text("NEXT")&&exact_text("PREV"));
    tap.touch_pos_x=147;(void)dispatch_screen(&tap);
    assert(active_state->list_page==2);render();assert(!exact_text("NEXT")&&exact_text("PREV"));
    call_count=0;
    swipe.gesture=GESTURE_SWIPE_RIGHT;(void)dispatch_screen(&swipe);
    assert(active_state->list_page==1);
    call_count=0;
    swipe.gesture=GESTURE_SWIPE_RIGHT;(void)dispatch_screen(&swipe);
    assert(active_state->list_page==0);
    call_count=0;
    swipe.gesture=GESTURE_SWIPE_RIGHT;(void)dispatch_screen(&swipe);
    assert(active_state->page==PAGE_TIDES&&active_state->list_page==0);
    active_state->page=PAGE_TIDE_LIST;
    call_count=0;
    swipe.gesture=GESTURE_SWIPE_LEFT;(void)dispatch_screen(&swipe);
    assert(active_state->list_page==1);
    call_count=0;
    swipe.gesture=GESTURE_SWIPE_LEFT;(void)dispatch_screen(&swipe);
    assert(active_state->list_page==2);
    call_count=0;
    swipe.gesture=GESTURE_SWIPE_LEFT;(void)dispatch_screen(&swipe);
    assert(active_state->list_page==2); /* last list page stays put */

    /* Advancing the watch clock removes the equal-to-now event from page one. */
    fake_datetime.sec=1;active_state->list_page=0;render();
    assert(!exact_text("-0.10 m"));
    assert(text_at("HIGH",8,31)&&text_at("11:00",112,31));
    /* A valid but wholly past table produces an explicit empty-list state. */
    for(int i=0;i<8;i++) active_state->tide_events[i].utc=current_utc_seconds()-1;
    active_state->list_page=0;render();
    assert(exact_text("NO UPCOMING TIDES"));
    assert(!exact_text("PREV")&&!exact_text("NEXT")&&exact_text("BACK"));
    active_state->tide_info.expires_utc=current_utc_seconds();
    render();assert(exact_text("NO UPCOMING TIDES"));
    fake_datetime.sec=2;render();
    assert(exact_text("TIDES EXPIRED")&&exact_text("UPDATE DATA"));

    active_state->tide_status=OUTDOOR_TIDE_INVALID;active_state->have_tide=0;render();
    assert(exact_text("TIDE DATA INVALID")&&!exact_text("NO UPCOMING TIDES"));
    active_state->tide_status=OUTDOOR_TIDE_EXPIRED;render();
    assert(exact_text("TIDES EXPIRED")&&exact_text("UPDATE DATA"));
    fake_datetime.month=2;fake_datetime.day=30;render();
    assert(exact_text("CLOCK UNAVAILABLE"));

    /* BACK always returns to the summary. Swipe-right pages backward, and
     * from the first list page returns to the summary. */
    fake_datetime.month=11;fake_datetime.day=8;fake_datetime.hour=10;fake_datetime.min=0;fake_datetime.sec=0;
    set_tide_list_fixture();active_state->page=PAGE_TIDE_LIST;
    tap.touch_pos_x=88;tap.touch_pos_y=154;(void)dispatch_screen(&tap);
    assert(active_state->page==PAGE_TIDES);
    active_state->page=PAGE_TIDE_LIST;active_state->list_page=1;
    swipe.gesture=GESTURE_SWIPE_RIGHT;(void)dispatch_screen(&swipe);
    assert(active_state->page==PAGE_TIDE_LIST&&active_state->list_page==0);
    (void)dispatch_screen(&swipe);
    assert(active_state->page==PAGE_TIDES);
}
static void test_overlay_navigation_and_exit(void)
{
    tides_sun_state_t *saved;struct gesture_ g={GESTURE_SWIPE_UP,0,0};
    reset();enter();saved=active_state;fake_overlay=1;show_screen(saved);
    assert(active_state==saved);assert(fake_registrations==2);
    assert(fake_display_calls==4&&fake_period==2000);
    (void)dispatch_screen(&g);assert(active_state->page==1);
    g.gesture=GESTURE_SWIPE_RIGHT;(void)dispatch_screen(&g);
    assert(fake_updates&&fake_exit==0&&active_state->page==PAGE_TIDES);
    key_press_screen();assert(!fake_updates&&fake_exit==1);
    reset();fake_malloc_fail=1;show_screen(NULL);assert(fake_watchface==1);
}
#ifndef TIDES_TEST_MAIN
#define TIDES_TEST_MAIN main
#endif
int TIDES_TEST_MAIN(void)
{
    test_pages_and_display_bounds();
    test_conversion_empty_future_and_expiry();
    test_tide_list_navigation_and_content();
    test_overlay_navigation_and_exit();
    puts("tides_sun tests passed");return 0;
}
