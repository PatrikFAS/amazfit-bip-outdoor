#ifndef BIP_OUTDOOR_NAV_TRIG_H
#define BIP_OUTDOOR_NAV_TRIG_H

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

#endif
