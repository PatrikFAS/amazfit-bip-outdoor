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
#if DIAGNOSTIC
    snapshot("Diagnostics: menu");
    fake_fixed = 0;
    click(88, 68);
    snapshot("GPS diagnostic: acquiring");
    set_fix(0x0c, 147848100, NAVI_NORTH_HEMISPHERE,
            369362100, NAVI_WEST_HEMISPHERE);
    poll_gps(active_state);
    snapshot("GPS diagnostic: simulated fix");
    click(88, 160);
    click(88, 108);
    fake_compass_degree = 359;
    snapshot("Compass diagnostic: raw heading");
    fake_compass_calibration = 1;
    screen_job();
    snapshot("Compass diagnostic: calibration needed");
    click(88, 160);
    click(88, 150);
    snapshot("Watch diagnostic: firmware, build, memory, battery, charging");
#else
    snapshot("Navigator: acquiring");
    swipe(GESTURE_SWIPE_RIGHT);
    snapshot("Legend: overview");
    swipe(GESTURE_SWIPE_LEFT);
    swipe(GESTURE_SWIPE_LEFT);
    snapshot("Compass: no saved waypoint");
    click(88, 150);
    set_fix(0x0c, 147848100, NAVI_NORTH_HEMISPHERE,
            369362100, NAVI_WEST_HEMISPHERE);
    poll_gps(active_state);
    snapshot("Navigator: fix, no saved target");
    click(88, 144); /* The Waypoints row opens HOME. */
    snapshot("Waypoints: empty HOME");
    click(113, 15);
    snapshot("Variation: off by default");
    for (int i = 0; i < 15; ++i) click(147, 75);
    snapshot("Variation: east correction");
    back();
    save_here();
    snapshot("Waypoints: first point saved");
    open_options();
    snapshot("Waypoints: active slot options");
    replace_here();
    snapshot("Waypoints: replace confirmation");
    delete_slot();
    snapshot("Waypoints: delete confirmation");
    back(); back();
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
    screen_job();
    browse();
    fake_fixed = 0;
    screen_job();
    back();
    snapshot("Navigator: brief GPS gap");
    fake_tick += LAST_FIX_TICKS + 1;
    screen_job();
    snapshot("Navigator: waiting after GPS gap");
    fake_fixed = 1;
    poll_gps(active_state);
    fake_tick += 15301;
    screen_job();
    snapshot("Navigator: unchanged fix");
    click(12, 15);
    snapshot("Navigator: units row tapped");

    browse(); open_coords();
    snapshot("Coordinates: latitude entry");
    enter_coordinate("49.33750");
    click(88, 15);
    snapshot("Coordinates: southern latitude");
    click(147, 15);
    snapshot("Coordinates: longitude entry");
    enter_coordinate("123.25358");
    click(88, 15);
    click(147, 15);
    snapshot("Coordinates: review signed point");
    click(88, 158);
    snapshot("Coordinates: replacement saved");

    set_fix(0x0c, 147857101, NAVI_NORTH_HEMISPHERE,
            369368101, NAVI_WEST_HEMISPHERE);
    poll_gps(active_state);
    fake_write_result = -1;
    back();
    browse(); open_options(); replace_here(); replace_here();
    snapshot("Waypoints: storage failure");

    reset_fakes(); enter_app();
    expect_distance(40, NAV_UNITS_METRIC, 40, "M", 10);
    snapshot("Navigator: distance in metres");
    expect_distance(20000000, NAV_UNITS_METRIC, 200000, "20000.0 KM", 0);
    snapshot("Navigator: wide distance with unit");

    reset_fakes(); enter_app();
    save_target_at(45001000, 75001400);
    active_state->course_origin_valid = 0; /* Start observing GPS from the simulated test position. */
    fix_at(45000000);
    fake_compass_degree = 38;
    screen_job();
    snapshot("Navigator: stationary compass and target");
    fake_tick += 5100;
    fix_at(45001000); screen_job();
    snapshot("Navigator: GPS course while moving");
    swipe(GESTURE_SWIPE_LEFT);
    snapshot("Compass: target and GPS track");
    swipe(GESTURE_SWIPE_RIGHT);
    fake_compass_degree = 128;
    screen_job();
    snapshot("Navigator: edge markers after a clockwise turn");
    fake_compass_degree = 180;
    screen_job();
    snapshot("Navigator: north at bottom");
#endif
    return 0;
}
