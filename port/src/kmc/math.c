/*
 * The KMC C library's trigonometry, which the game's physics and camera
 * call. KMC computes it by CORDIC in fixed point, so its results differ in
 * their low bits from any host's maths library, and hosts' libraries differ
 * from each other. These give the N64's results on every host.
 *
 * Each routine turns a vector through 52 known angles, atan(2^-i), in
 * fixed point with 60 fractional bits: towards an angle for the sine and
 * cosine, or onto the x axis for the arc tangent. The N64 converted the
 * fixed point back through KMC's own routine, which truncates where the
 * host's would round, and that is done here too.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "port/kmc.h"

#define KMC_PI 3.14159265358979323846

#define FRACTION_BITS 60
#define FIXED_ONE (INT64_C(1) << FRACTION_BITS)
// 2^60 and 2^-60, by which a double scales exactly
#define FIXED_SCALE 1152921504606846976.0
#define FIXED_UNIT (1.0 / FIXED_SCALE)
#define DOUBLE_SIGNIFICAND_BITS 53

#define ROTATION_COUNT 52

// The sine and the tangent of an angle closer to 0 are the angle
#define SINE_LINEAR_LIMIT 1e-8
// As is the arc tangent of a number closer to 0
#define ARC_TANGENT_LINEAR_LIMIT 1e-7

// The rotations stretch a vector by about 1.6468, so the sine and cosine
// start from this, 1 / that gain. Like each angle below, it is the
// nearest double to its true value, scaled and truncated to fixed point.
#define INVERSE_GAIN INT64_C(0x09B74EDA8435E580)

// atan(2^-i), for rotation i
static const int64_t f_RotationAngles[ROTATION_COUNT] = {
    INT64_C(0x0C90FDAA22168C00),
    INT64_C(0x076B19C1586ED3C0),
    INT64_C(0x03EB6EBF25901BA0),
    INT64_C(0x01FD5BA9AAC2F6E0),
    INT64_C(0x00FFAADDB967EF50),
    INT64_C(0x007FF556EEA5D894),
    INT64_C(0x003FFEAAB776E536),
    INT64_C(0x001FFFD555BBBA97),
    INT64_C(0x000FFFFAAAADDDDB),
    INT64_C(0x0007FFFF55556EEF),
    INT64_C(0x0003FFFFEAAAAB77),
    INT64_C(0x0001FFFFFD55555B),
    INT64_C(0x0000FFFFFFAAAAAA),
    INT64_C(0x00007FFFFFF55555),
    INT64_C(0x00003FFFFFFEAAAA),
    INT64_C(0x00001FFFFFFFD555),
    INT64_C(0x00000FFFFFFFFAAA),
    INT64_C(0x000007FFFFFFFF55),
    INT64_C(0x000003FFFFFFFFEA),
    INT64_C(0x000001FFFFFFFFFD),
    INT64_C(0x000000FFFFFFFFFF),
    INT64_C(0x0000007FFFFFFFFF),
    INT64_C(0x0000003FFFFFFFFF),
    INT64_C(0x0000001FFFFFFFFF),
    INT64_C(0x0000000FFFFFFFFF),
    INT64_C(0x00000007FFFFFFFF),
    INT64_C(0x00000003FFFFFFFF),
    INT64_C(0x0000000200000000),
    INT64_C(0x0000000100000000),
    INT64_C(0x0000000080000000),
    INT64_C(0x0000000040000000),
    INT64_C(0x0000000020000000),
    INT64_C(0x0000000010000000),
    INT64_C(0x0000000008000000),
    INT64_C(0x0000000004000000),
    INT64_C(0x0000000002000000),
    INT64_C(0x0000000001000000),
    INT64_C(0x0000000000800000),
    INT64_C(0x0000000000400000),
    INT64_C(0x0000000000200000),
    INT64_C(0x0000000000100000),
    INT64_C(0x0000000000080000),
    INT64_C(0x0000000000040000),
    INT64_C(0x0000000000020000),
    INT64_C(0x0000000000010000),
    INT64_C(0x0000000000008000),
    INT64_C(0x0000000000004000),
    INT64_C(0x0000000000002000),
    INT64_C(0x0000000000001000),
    INT64_C(0x0000000000000800),
    INT64_C(0x0000000000000400),
    INT64_C(0x0000000000000200),
};

typedef struct {
    int64_t x;
    int64_t y;
} FixedVector;

/*
 * KMC's long long to double conversion: the magnitude cut to a double's 53
 * bits of significand by dropping the bits below, then signed, then scaled
 * from fixed point. The host's conversion rounds instead.
 */
static double ConvertFixedToDouble(int64_t value) {
    uint64_t magnitude = (value < 0) ? 0u - (uint64_t)value : (uint64_t)value;
    double scale = FIXED_UNIT;
    double result;

    while ((magnitude >> DOUBLE_SIGNIFICAND_BITS) != 0) {
        magnitude >>= 1;
        scale *= 2.0;
    }
    // Both factors are exact, and so is their product
    result = (double)magnitude * scale;
    return (value < 0) ? -result : result;
}

// Toward zero, as KMC's conversion does; the callers keep value within
// the fixed point's range
static int64_t ConvertDoubleToFixed(double value) {
    return (int64_t)(value * FIXED_SCALE);
}

// The sine and cosine of an angle within about 1.74 radians of 0, as a
// vector from the origin
static FixedVector RotateUnitVector(double angle) {
    FixedVector vector = {INVERSE_GAIN, 0};
    int64_t remaining = ConvertDoubleToFixed(angle);
    int i;

    for (i = 0; i < ROTATION_COUNT; i++) {
        // Arithmetic shifts, which every supported compiler gives
        int64_t xStep = vector.x >> i;
        int64_t yStep = vector.y >> i;

        if (remaining >= 0) {
            vector.x -= yStep;
            vector.y += xStep;
            remaining -= f_RotationAngles[i];
        }
        else {
            vector.x += yStep;
            vector.y -= xStep;
            remaining += f_RotationAngles[i];
        }
    }
    return vector;
}

// atan(y / x) of a vector with x > 0, both no more than 1 in fixed point
static double MeasureVectorAngle(FixedVector vector) {
    int64_t angle = 0;
    int i;

    for (i = 0; i < ROTATION_COUNT; i++) {
        int64_t xStep = vector.x >> i;
        int64_t yStep = vector.y >> i;

        if (vector.y >= 0) {
            vector.x += yStep;
            vector.y -= xStep;
            angle += f_RotationAngles[i];
        }
        else {
            vector.x -= yStep;
            vector.y += xStep;
            angle -= f_RotationAngles[i];
        }
    }
    return ConvertFixedToDouble(angle);
}

/*
 * Brings angle within a quarter turn of 0 by whole half turns, which flip
 * the sine and the cosine when their count is odd. False where the N64's
 * conversion of the count would raise an exception: an angle that isn't
 * finite, or of 2^31 half turns or more.
 */
static bool ReduceAngle(double angle, double *reduced, bool *isFlipped) {
    double halfTurns = angle / KMC_PI + ((angle >= 0.0) ? 0.5 : -0.5);
    int halfTurnCount;

    if (!(halfTurns > (double)INT32_MIN - 1.0 && halfTurns < (double)INT32_MAX + 1.0)) {
        return false;
    }
    halfTurnCount = (int)halfTurns;
    *reduced = angle - halfTurnCount * KMC_PI;
    *isFlipped = (halfTurnCount & 1) != 0;
    return true;
}

static bool IsNearZero(double value, double limit) {
    return value < limit && value > -limit;
}

double KmcSine(double angle) {
    double reduced;
    bool isFlipped;
    double sine;

    if (!ReduceAngle(angle, &reduced, &isFlipped)) {
        return NAN;
    }
    if (IsNearZero(reduced, SINE_LINEAR_LIMIT)) {
        return reduced;
    }
    sine = ConvertFixedToDouble(RotateUnitVector(reduced).y);
    return isFlipped ? -sine : sine;
}

double KmcCosine(double angle) {
    double reduced;
    bool isFlipped;
    double cosine;

    if (!ReduceAngle(angle, &reduced, &isFlipped)) {
        return NAN;
    }
    cosine = ConvertFixedToDouble(RotateUnitVector(reduced).x);
    return isFlipped ? -cosine : cosine;
}

// The tangent's period is a half turn, so the flip doesn't matter
double KmcTangent(double angle) {
    double reduced;
    bool isFlipped;
    FixedVector vector;

    if (!ReduceAngle(angle, &reduced, &isFlipped)) {
        return NAN;
    }
    if (IsNearZero(reduced, SINE_LINEAR_LIMIT)) {
        return reduced;
    }
    vector = RotateUnitVector(reduced);
    return ConvertFixedToDouble(vector.y) / ConvertFixedToDouble(vector.x);
}

double KmcArcTangent(double value) {
    double magnitude = (value < 0.0) ? -value : value;
    double angle;

    if (magnitude > ARC_TANGENT_LINEAR_LIMIT) {
        // Past 1, the vector (1 / value, 1) keeps both within range
        FixedVector vector;

        if (magnitude >= 1.0) {
            vector.x = ConvertDoubleToFixed(1.0 / magnitude);
            vector.y = FIXED_ONE;
        }
        else {
            vector.x = FIXED_ONE;
            vector.y = ConvertDoubleToFixed(magnitude);
        }
        angle = MeasureVectorAngle(vector);
    }
    else {
        angle = magnitude;
    }
    return (value < 0.0) ? -angle : angle;
}

/*
 * The angle of (x, y), from -pi to pi. With both 0 the N64 also set the C
 * library's errno, which the game never reads.
 */
double KmcArcTangent2(double y, double x) {
    double angle;

    if (x == 0.0) {
        if (y == 0.0) {
            return 0.0;
        }
        return (y > 0.0) ? KMC_PI / 2 : -KMC_PI / 2;
    }
    angle = KmcArcTangent(y / x);
    if (x > 0.0) {
        return angle;
    }
    if (y == 0.0) {
        return KMC_PI;
    }
    return (y > 0.0) ? angle + KMC_PI : angle - KMC_PI;
}
