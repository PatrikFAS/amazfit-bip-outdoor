/* Capture the real watch drawing calls using deterministic host inputs. */
#define TIDES_TEST_MAIN run_tides_sun_tests
#include "test_tides_sun.c"

static void snapshot(const char *name, int page)
{
    int index;
    active_state->page = page;
    render();
    assert_text_bounds();
    printf("SCREEN\t%s\n", name);
    for (index = 0; index < call_count; ++index) {
        printf("F\t%06X\n", calls[index].color & 0xFFFFFF);
        printf("%c\t%d\t%d\t%s\n", calls[index].center ? 'C' : 'T',
               calls[index].x, calls[index].y, calls[index].text);
    }
    for (index = 0; index < rect_count; ++index) {
        printf("F\t%06X\n", rects[index].color & 0xFFFFFF);
        printf("R\t%d\t%d\t%d\t%d\n", rects[index].x1,
               rects[index].y1, rects[index].x2, rects[index].y2);
    }
}

int main(void)
{
    run_tides_sun_tests();
    reset();
    enter();
    snapshot("Sun: Point Atkinson", PAGE_SUN);
    snapshot("Twilight: Point Atkinson", PAGE_TWILIGHT);
    snapshot("Tides: simulated events", PAGE_TIDES);
    snapshot("Tide provenance and expiry", PAGE_INFO);
    set_tide_list_fixture();
    snapshot("Tide list: first page", PAGE_TIDE_LIST);
    active_state->list_page = 1;
    snapshot("Tide list: middle page", PAGE_TIDE_LIST);
    active_state->list_page = 2;
    snapshot("Tide list: last page", PAGE_TIDE_LIST);
    for (int index = 0; index < active_state->tide_info.event_count; ++index)
        active_state->tide_events[index].utc = current_utc_seconds() - 1;
    active_state->list_page = 0;
    snapshot("Tide list: no upcoming events", PAGE_TIDE_LIST);
    active_state->tide_status = OUTDOOR_TIDE_EXPIRED;
    snapshot("Tide list: expired data", PAGE_TIDE_LIST);
    fake_datetime.day = 15;
    for (int tick = 0; tick < DATA_REFRESH_TICKS; ++tick) screen_job();
    snapshot("Tides: expired table", PAGE_TIDES);
    fake_datetime.month = 0;
    snapshot("Tides: invalid watch clock", PAGE_TIDES);
    return 0;
}
