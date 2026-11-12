#ifndef BIP_OUTDOOR_NAV_TRIG_H
#define BIP_OUTDOOR_NAV_TRIG_H

#include <math.h>

/*
 * Small, bounded-angle trigonometry for navigation. Inputs must be finite
 * radians in [-pi, pi]. Callers establish that precondition from validated
 * geographic coordinates; these helpers intentionally do not normalize or
 * check arbitrary angles.
 */
static inline float nav_sin_bounded(float radians)
{
    const float pi = 3.14159265358979323846f;
    const float half_pi = 1.57079632679489661923f;
    float x = radians;
    float x2;

    if (x > half_pi) x = pi - x;
    else if (x < -half_pi) x = -pi - x;
    x2 = x * x;
    /* x * (1 + x²*(-1/3! + x²*(1/5! ... + x²*(1/13!)))). */
    return x * (1.0f + x2 * (-0.16666666666666666667f + x2 *
        (0.00833333333333333333f + x2 * (-0.00019841269841269841f +
        x2 * (0.00000275573192239859f + x2 *
        (-0.00000002505210838544f + x2 * 0.00000000016059043837f))))));
}

static inline float nav_cos_bounded(float radians)
{
    const float pi = 3.14159265358979323846f;
    const float half_pi = 1.57079632679489661923f;
    float x = radians;
    float sign = 1.0f;
    float x2;

    if (x > half_pi) { x = pi - x; sign = -1.0f; }
    else if (x < -half_pi) { x = -pi - x; sign = -1.0f; }
    x2 = x * x;
    /* 1 + x²*(-1/2! + x²*(1/4! ... + x²*(1/12!))). */
    return sign * (1.0f + x2 * (-0.5f + x2 *
        (0.04166666666666666667f + x2 * (-0.00138888888888888889f +
        x2 * (0.00002480158730158730f + x2 *
        (-0.00000027557319223986f + x2 * 0.00000000208767569879f))))));
}

/* atan(z) for z in [0, 1], transformed so the Taylor input stays <= tan(pi/8). */
static inline float nav_atan_unit_bounded(float z)
{
    const float tan_pi_over_8 = 0.41421356237309504880f;
    const float quarter_pi = 0.78539816339744830962f;
    float x = z;
    float x2;
    float angle;
    int complement = z > tan_pi_over_8;

    if (complement) x = (z - 1.0f) / (z + 1.0f);
    x2 = x * x;
    /* atan(x) through x^15; after reduction truncation is below 2e-8 before float rounding. */
    angle = x * (1.0f + x2 * (-0.33333333333333333333f + x2 *
        (0.2f + x2 * (-0.14285714285714285714f + x2 *
        (0.11111111111111111111f + x2 * (-0.09090909090909090909f + x2 *
        (0.07692307692307692308f + x2 * -0.06666666666666666667f)))))));
    return complement ? quarter_pi + angle : angle;
}

/* Inputs are finite bounded components; output follows atan2's [-pi, pi] range. */
static inline float nav_atan2_bounded(float y, float x)
{
    const float pi = 3.14159265358979323846f;
    const float half_pi = 1.57079632679489661923f;
    float ax = x < 0.0f ? -x : x;
    float ay = y < 0.0f ? -y : y;
    float angle;
    int x_negative = signbit(x);
    int y_negative = signbit(y);

    /* Only the all-zero case needs a guard before taking the ratio. */
    angle = ay > ax ? half_pi - nav_atan_unit_bounded(ax / ay)
                    : ax == 0.0f ? 0.0f : nav_atan_unit_bounded(ay / ax);
    if (x_negative) angle = pi - angle;
    return y_negative ? -angle : angle;
}

#endif
