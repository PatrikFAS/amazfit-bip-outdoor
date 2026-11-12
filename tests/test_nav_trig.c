#include "../src/nav_trig.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    const double pi = 3.14159265358979323846;
    double max_sin_error = 0.0, max_cos_error = 0.0, max_atan2_error = 0.0;
    int i;

    for (i = 0; i <= 1000000; ++i) {
        const double x = -pi + (2.0 * pi * (double)i / 1000000.0);
        const float s = nav_sin_bounded((float)x);
        const float c = nav_cos_bounded((float)x);
        const float ax = (float)cos(x), ay = (float)sin(x);
        const double atan2_error = fabs((double)nav_atan2_bounded(ay, ax) - (double)atan2f(ay, ax));
        const double se = fabs((double)s - sin(x));
        const double ce = fabs((double)c - cos(x));
        if (se > max_sin_error) max_sin_error = se;
        if (ce > max_cos_error) max_cos_error = ce;
        if (atan2_error > max_atan2_error) max_atan2_error = atan2_error;
        assert(se <= 5e-7);
        assert(ce <= 5e-7);
        assert(atan2_error <= 5e-7);
        assert(fabs((double)s) <= 1.0 + 5e-7);
        assert(fabs((double)c) <= 1.0 + 5e-7);
    }

    assert(nav_sin_bounded(0.0f) == 0.0f);
    assert(nav_cos_bounded(0.0f) == 1.0f);
    assert(fabsf(nav_sin_bounded(1.57079632679f) - 1.0f) <= 5e-7f);
    assert(fabsf(nav_cos_bounded(1.57079632679f)) <= 5e-7f);
    assert(fabsf(nav_sin_bounded(-1.57079632679f) + 1.0f) <= 5e-7f);
    assert(fabsf(nav_cos_bounded(3.14159265359f) + 1.0f) <= 5e-7f);
    assert(fabsf(nav_sin_bounded(-3.14159265359f)) <= 5e-7f);
    assert(fabsf(nav_sin_bounded(-2.4f) + nav_sin_bounded(2.4f)) <= 5e-7f);
    assert(fabsf(nav_cos_bounded(-2.4f) - nav_cos_bounded(2.4f)) <= 5e-7f);

    /* Exercise signed axes, quadrants, and the Taylor range reduction boundary. */
    assert(nav_atan2_bounded(0.0f, 1.0f) == 0.0f);
    assert(fabsf(nav_atan2_bounded(0.0f, -1.0f) - (float)pi) <= 5e-7f);
    assert(fabsf(nav_atan2_bounded(1.0f, 0.0f) - (float)(pi / 2.0)) <= 5e-7f);
    assert(fabsf(nav_atan2_bounded(-1.0f, 0.0f) + (float)(pi / 2.0)) <= 5e-7f);
    for (i = 0; i <= 100000; ++i) {
        const float ratio = (float)i / 100000.0f;
        const float ys[] = {ratio, ratio, -ratio, -ratio};
        const float xs[] = {1.0f, -1.0f, -1.0f, 1.0f};
        int quadrant;
        for (quadrant = 0; quadrant < 4; ++quadrant) {
            double error = fabs((double)nav_atan2_bounded(ys[quadrant], xs[quadrant]) -
                                (double)atan2f(ys[quadrant], xs[quadrant]));
            if (error > max_atan2_error) max_atan2_error = error;
            assert(error <= 5e-7);
        }
    }

    printf("bounded navigation trig tests passed (max errors sin %.3g, cos %.3g, atan2 %.3g)\n",
           max_sin_error, max_cos_error, max_atan2_error);
    return 0;
}
