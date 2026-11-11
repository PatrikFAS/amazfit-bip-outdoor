#include <libbip.h>
#include <stdint.h>

static void hello_screen(void *param);
static int hello_dispatch(void *param);
static void hello_exit(void);

static void *return_to;
static struct regmenu_ hello_data = {
    55, 1, 0,
    hello_dispatch, hello_exit, 0, 0,
    hello_screen, 0, 0
};

int main(int param0, char **argv)
{
    Elf_proc_ *proc;
    (void)argv;
    proc = get_proc_by_addr((void *)main);
    return_to = (param0 && proc && proc->elf_finish) ?
                (void *)proc->elf_finish : (void *)show_watchface;
    hello_screen((void *)(uintptr_t)(unsigned int)param0);
    return 0;
}

static void hello_screen(void *param)
{
    (void)param;
    reg_menu(&hello_data, 0);
    set_bg_color(COLOR_BLACK);
    fill_screen_bg();
    GUI_UC_SetEncodeUTF8();
    load_font();
    set_fg_color(COLOR_WHITE);
    text_out_center("HELLO BIP", VIDEO_X / 2, 68);
}

static int hello_dispatch(void *param)
{
    struct gesture_ *gesture = (struct gesture_ *)param;
    if (gesture && gesture->gesture == GESTURE_SWIPE_RIGHT) hello_exit();
    return 0;
}

static void hello_exit(void)
{
    show_menu_animate(return_to, (int)(uintptr_t)hello_screen, ANIMATE_RIGHT);
}
