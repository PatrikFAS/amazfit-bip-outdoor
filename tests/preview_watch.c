/* Run the real C app using the host test adapters and capture its draw calls. */
#define WATCH_PREVIEW 1
#define HOST_TEST_MAIN run_host_tests
#include "test_watch.c"

static void snapshot(const char *name)
{
    printf("SCREEN\t%s\n", name);
    capture_screen = 1;
    draw_screen(active_state);
    capture_screen = 0;
}

int main(void)
{
    run_host_tests();
    reset_fakes();
    enter_app();
    snapshot(DIAGNOSTIC ? "Diagnostic: acquiring" : "Navigator: acquiring");
    set_fix(0x0c, 147848100, NAVI_NORTH_HEMISPHERE,
            369362100, NAVI_WEST_HEMISPHERE);
    poll_gps(active_state);
#if DIAGNOSTIC
    snapshot("Diagnostic: simulated fix");
#else
    snapshot("Navigator: fix, no saved target");
    new_point();
    snapshot("Waypoints: empty HOME");
    save_here();
    snapshot("Waypoints: first point saved");
    save_here();
    snapshot("Waypoints: replace confirmation");
    delete_slot();
    snapshot("Waypoints: delete confirmation");
    back();
    set_fix(0x0c, 147857100, NAVI_NORTH_HEMISPHERE,
            369368100, NAVI_WEST_HEMISPHERE);
    screen_job();
    snapshot("Navigator: walking to HOME");
    click(132, 15);
    snapshot("Navigator: nautical units, nearby metres");
    set_fix(0x0c, 147848130, NAVI_NORTH_HEMISPHERE,
            369362100, NAVI_WEST_HEMISPHERE);
    screen_job();
    snapshot("Navigator: near target");
    set_fix(0x0c, 147857100, NAVI_NORTH_HEMISPHERE,
            369368100, NAVI_WEST_HEMISPHERE);
    fake_fixed = 0;
    screen_job();
    snapshot("Navigator: fix lost");
    fake_fixed = 1;
    poll_gps(active_state);
    fake_tick += 15301;
    screen_job();
    snapshot("Navigator: unchanged fix");
    click(44, 15);
    snapshot("Navigator: GPS paused");
    click(44, 15);
    fake_write_result = -1;
    new_point(); save_here();
    snapshot("Waypoints: storage failure");
#endif
    return 0;
}
