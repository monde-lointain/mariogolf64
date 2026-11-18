/*
 * The sine and cosine of a float, the same on every host.
 *
 * As libultra did, each takes the multiple of pi nearest the angle (for the
 * cosine, the odd multiple of a half pi) away from it in double precision,
 * and sums a polynomial in what is left, from -pi/2 to pi/2. Here the
 * polynomial is the Taylor series of the sine to x^21, whose terms past
 * that add less than 10^-18 there, so the double sum misses the true sine
 * by little more than double's own rounding; the series to x^19 rounds
 * four cosines below 11 the wrong way. Rounded to float, these have given
 * the nearest float to the true value, with the host's double sine and
 * cosine rounded as the judge, for every float angle of magnitude 11 or
 * less, and for every thirteenth float's bits up to 2^28.
 *
 * IEEE double arithmetic gives the same results everywhere, as long as no
 * multiply and add are fused into one, which the build forbids.
 */

#include <math.h>

#include "port/gu_sine.h"

// Below this, the sine of an angle is nearest the angle itself, which is
// taken as it is, so that -0 keeps its sign
#define LINEAR_LIMIT 0x1p-12f
// From this on, the angle is too large to reduce: 2^28
#define ANGLE_LIMIT 0x1p28f

// Pi in two doubles: the nearest to it, and what that misses by
#define PI_HEAD 0x1.921fb54442d18p+1
#define PI_TAIL 0x1.1a62633145c07p-53
// Pi cut to 24 fractional bits, of which any multiple below 2^28 is exact
#define PI_HIGH 0x1.921fb5p+1
// The rest of pi past PI_HIGH, rounded once
#define PI_LOW ((PI_HEAD - PI_HIGH) + PI_TAIL)
#define INVERSE_PI (1.0 / PI_HEAD)

#define TERM_COUNT 10

// The Taylor series' coefficients past x: (-1)^n / (2n + 1)! for n from 1.
// Each factorial is a double exactly, so each is rounded only once.
static const double f_Terms[TERM_COUNT] = {
    -1.0 / 6.0,
    1.0 / 120.0,
    -1.0 / 5040.0,
    1.0 / 362880.0,
    -1.0 / 39916800.0,
    1.0 / 6227020800.0,
    -1.0 / 1307674368000.0,
    1.0 / 355687428096000.0,
    -1.0 / 121645100408832000.0,
    1.0 / 51090942171709440000.0,
};

// The sine of an angle from -pi/2 to pi/2
static double SumSineSeries(double angle) {
    double square = angle * angle;
    double sum = f_Terms[TERM_COUNT - 1];
    int i;

    for (i = TERM_COUNT - 2; i >= 0; i--) {
        sum = (sum * square) + f_Terms[i];
    }

    return angle + ((angle * square) * sum);
}

// The nearest whole number, halves away from 0, as libultra's ROUND
static int RoundToWhole(double value) {
    return (int)((value >= 0.0) ? (value + 0.5) : (value - 0.5));
}

// The angle less a multiple of pi, in two steps, the first exact
static double SubtractPis(double angle, double multiple) {
    return (angle - (multiple * PI_HIGH)) - (multiple * PI_LOW);
}

float GuSine(float angle) {
    float magnitude = fabsf(angle);
    double rest;
    int halfTurns;

    if (magnitude < LINEAR_LIMIT) {
        return angle;
    }

    if (magnitude < ANGLE_LIMIT) {
        halfTurns = RoundToWhole(angle * INVERSE_PI);
        rest = SumSineSeries(SubtractPis(angle, halfTurns));
        return ((halfTurns & 1) == 0) ? (float)rest : -(float)rest;
    }

    return isnan(angle) ? NAN : 0.0f;
}

// cos(x) is sin(x - (n - 1/2) pi) for the n that leaves the least, but
// with the opposite sign when n is odd
float GuCosine(float angle) {
    float magnitude = fabsf(angle);
    double rest;
    int n;

    if (magnitude < ANGLE_LIMIT) {
        n = RoundToWhole((magnitude * INVERSE_PI) + 0.5);
        rest = SumSineSeries(SubtractPis(magnitude, n - 0.5));
        return ((n & 1) == 0) ? (float)rest : -(float)rest;
    }

    return isnan(angle) ? NAN : 0.0f;
}
