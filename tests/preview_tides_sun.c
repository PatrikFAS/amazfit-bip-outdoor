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
    for (index = 0; index < call_count; ++index)
        printf("T\t%d\t%d\t%s\n", calls[index].x,
               calls[index].y, calls[index].text);
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
    fake_datetime.day = 15;
    screen_job();
    snapshot("Tides: expired table", PAGE_TIDES);
    fake_datetime.month = 0;
    snapshot("Tides: invalid watch clock", PAGE_TIDES);
    return 0;
}
