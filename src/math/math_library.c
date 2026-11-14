/*
 * Single-precision fdlibm routines: fabsf, acosf, atan2f and atanf.
 * They work on the bits of floats, unlike KMC's libm, which the game
 * also links and which works on doubles with CORDIC loops.
 */

#include <ultra64.h>

typedef union {
    f32 value;
    s32 word;
} FloatShape;

// As in fdlibm, these are do-while (0) loops, which the original needs: as
// plain blocks, fabsf gives 2 words differing, 2 in shape, and acosf 18, 2 in
// shape
#define GET_FLOAT_WORD(destination, source) \
    do { \
        FloatShape shape; \
        shape.value = (source); \
        (destination) = shape.word; \
    } \
    while (FALSE)

#define SET_FLOAT_WORD(destination, source) \
    do { \
        FloatShape shape; \
        shape.word = (source); \
        (destination) = shape.value; \
    } \
    while (FALSE)

#define SIGN_MASK 0x80000000
#define MAGNITUDE_MASK 0x7FFFFFFF
#define INFINITY_WORD 0x7F800000
#define ONE_WORD 0x3F800000

float atanf(float x);

// acos and atan2 split pi into a float and the error left over, so that
// adding the error last keeps the result accurate to the last bit.
static const float f_One = 1.0f;
static const float f_Pi = 3.1415925026e+00f;
static const float f_HalfPiHigh = 1.5707962513e+00f;
static const float f_HalfPiLow = 7.5497894159e-08f;
// asin(sqrt(z)) ~ sqrt(z) + sqrt(z) * P(z) / Q(z) for z <= 1/4
static const float f_AsinP0 = 1.6666667163e-01f;
static const float f_AsinP1 = -3.2556581497e-01f;
static const float f_AsinP2 = 2.0121252537e-01f;
static const float f_AsinP3 = -4.0055535734e-02f;
static const float f_AsinP4 = 7.9153501429e-04f;
static const float f_AsinP5 = 3.4793309169e-05f;
static const float f_AsinQ1 = -2.4033949375e+00f;
static const float f_AsinQ2 = 2.0209457874e+00f;
static const float f_AsinQ3 = -6.8828397989e-01f;
static const float f_AsinQ4 = 7.7038154006e-02f;

// fdlibm's routines, computed in floats
float fabsf(float x) {
    s32 word;

    GET_FLOAT_WORD(word, x);
    SET_FLOAT_WORD(x, word & MAGNITUDE_MASK);
    return x;
}

float acosf(float x) {
    float z;
    float numerator;
    float denominator;
    float ratio;
    float correction;
    float sqrtZ;
    float sqrtLow;
    float sqrtHigh;
    s32 word;
    s32 magnitude;

    GET_FLOAT_WORD(word, x);
    magnitude = word & MAGNITUDE_MASK;
    if (magnitude == ONE_WORD) {
        if (word > 0) {
            return 0.0f;
        }
        else {
            return f_Pi + (2.0f * f_HalfPiLow);
        }
    }
    else if (magnitude > ONE_WORD) {
        // NaN without fast math; the game's flags fold this to 0
        return (x - x) / (x - x);
    }

    if (magnitude < 0x3F000000) { // |x| < 0.5
        if (magnitude <= 0x23000000) { // |x| <= 2^-57
            return f_HalfPiHigh + f_HalfPiLow;
        }

        z = x * x;
        numerator = z *
            (f_AsinP0 +
                (z *
                    (f_AsinP1 +
                        (z * (f_AsinP2 + (z * (f_AsinP3 + (z * (f_AsinP4 + (z * f_AsinP5))))))))));
        denominator =
            f_One + (z * (f_AsinQ1 + (z * (f_AsinQ2 + (z * (f_AsinQ3 + (z * f_AsinQ4)))))));
        ratio = numerator / denominator;
        return f_HalfPiHigh - (x - (f_HalfPiLow - (x * ratio)));
    }
    else if (word < 0) {
        z = (f_One + x) * 0.5f;
        numerator = z *
            (f_AsinP0 +
                (z *
                    (f_AsinP1 +
                        (z * (f_AsinP2 + (z * (f_AsinP3 + (z * (f_AsinP4 + (z * f_AsinP5))))))))));
        denominator =
            f_One + (z * (f_AsinQ1 + (z * (f_AsinQ2 + (z * (f_AsinQ3 + (z * f_AsinQ4)))))));
        sqrtZ = sqrtf(z);
        ratio = numerator / denominator;
        correction = (ratio * sqrtZ) - f_HalfPiLow;
        return f_Pi - (2.0f * (sqrtZ + correction));
    }
    else {
        s32 sqrtHighWord;

        z = (f_One - x) * 0.5f;
        sqrtZ = sqrtf(z);
        sqrtHigh = sqrtZ;
        GET_FLOAT_WORD(sqrtHighWord, sqrtHigh);
        SET_FLOAT_WORD(sqrtHigh, sqrtHighWord & 0xFFFFF000);
        sqrtLow = (z - (sqrtHigh * sqrtHigh)) / (sqrtZ + sqrtHigh);
        numerator = z *
            (f_AsinP0 +
                (z *
                    (f_AsinP1 +
                        (z * (f_AsinP2 + (z * (f_AsinP3 + (z * (f_AsinP4 + (z * f_AsinP5))))))))));
        denominator =
            f_One + (z * (f_AsinQ1 + (z * (f_AsinQ2 + (z * (f_AsinQ3 + (z * f_AsinQ4)))))));
        ratio = numerator / denominator;
        correction = (ratio * sqrtZ) + sqrtLow;
        return 2.0f * (sqrtHigh + correction);
    }
}

// fdlibm adds this to results to raise the inexact exception, but the
// compiler folds the sums, which round to the other constant alone.
static const float f_Tiny = 1.0e-30f;
static const float f_Zero = 0.0f;
static const float f_QuarterPi = 7.8539818525e-01f;
static const float f_HalfPi = 1.5707963705e+00f;
static const float f_PiLow = 1.5099578832e-07f;

float atan2f(float y, float x) {
    float angle;
    s32 exponentDifference;
    s32 quadrant;
    s32 xWord;
    s32 yWord;
    s32 xMagnitude;
    s32 yMagnitude;

    GET_FLOAT_WORD(xWord, x);
    xMagnitude = xWord & MAGNITUDE_MASK;
    GET_FLOAT_WORD(yWord, y);
    yMagnitude = yWord & MAGNITUDE_MASK;
    if ((xMagnitude > INFINITY_WORD) || (yMagnitude > INFINITY_WORD)) {
        return x + y;
    }

    if (xWord == ONE_WORD) {
        return atanf(y);
    }

    // Bit 0 is the sign of y and bit 1 the sign of x
    quadrant = ((yWord >> 31) & 1) | ((xWord >> 30) & 2);

    if (yMagnitude == 0) {
        switch (quadrant) {
            case 0:
            case 1:
                return y;
            case 2:
                return f_Pi + f_Tiny;
            case 3:
                return -f_Pi - f_Tiny;
        }
    }

    if (xMagnitude == 0) {
        return (yWord < 0) ? -f_HalfPi - f_Tiny : f_HalfPi + f_Tiny;
    }

    if (xMagnitude == INFINITY_WORD) {
        if (yMagnitude == INFINITY_WORD) {
            switch (quadrant) {
                case 0:
                    return f_QuarterPi + f_Tiny;
                case 1:
                    return -f_QuarterPi - f_Tiny;
                case 2:
                    return (3.0f * f_QuarterPi) + f_Tiny;
                case 3:
                    return (-3.0f * f_QuarterPi) - f_Tiny;
            }
        }
        else {
            switch (quadrant) {
                case 0:
                    return f_Zero;
                case 1:
                    return -f_Zero;
                case 2:
                    return f_Pi + f_Tiny;
                case 3:
                    return -f_Pi - f_Tiny;
            }
        }
    }

    if (yMagnitude == INFINITY_WORD) {
        return (yWord < 0) ? -f_HalfPi - f_Tiny : f_HalfPi + f_Tiny;
    }

    exponentDifference = (yMagnitude - xMagnitude) >> 23;
    if (exponentDifference > 60) {
        angle = f_HalfPi + (0.5f * f_PiLow);
    }
    else if (xWord < 0 && exponentDifference < -60) {
        angle = 0.0f;
    }
    else {
        angle = atanf(fabsf(y / x));
    }

    switch (quadrant) {
        case 0:
            return angle;
        case 1: {
            u32 angleWord;

            GET_FLOAT_WORD(angleWord, angle);
            SET_FLOAT_WORD(angle, angleWord ^ SIGN_MASK);
        }
            return angle;
        case 2:
            return f_Pi - (angle - f_PiLow);
        default:
            return (angle - f_PiLow) - f_Pi;
    }
}

// atan of 0.5, 1, 1.5 and infinity, split into a float and the error left over
static const float f_AtanHigh[] = {
    4.6364760399e-01f,
    7.8539812565e-01f,
    9.8279368877e-01f,
    1.5707962513e+00f,
};
static const float f_AtanLow[] = {
    5.0121582440e-09f,
    3.7748947079e-08f,
    3.4473217170e-08f,
    7.5497894159e-08f,
};
// atan(x) ~ x - x * (sum of f_AtanTerms[i] * x^(2i+2)) for |x| <= 7/16
static const float f_AtanTerms[] = {
    3.3333334327e-01f,
    -2.0000000298e-01f,
    1.4285714924e-01f,
    -1.1111110449e-01f,
    9.0908870101e-02f,
    -7.6918758452e-02f,
    6.6610731184e-02f,
    -5.8335702866e-02f,
    4.9768779427e-02f,
    -3.6531571299e-02f,
    1.6285819933e-02f,
};
static const float f_Huge = 1.0e30f;

float atanf(float x) {
    float xFourth;
    float oddSum;
    float evenSum;
    float xSquared;
    float angle;
    s32 magnitude;
    s32 word;
    s32 reductionIndex;

    GET_FLOAT_WORD(word, x);
    magnitude = word & MAGNITUDE_MASK;
    if (magnitude >= 0x50800000) { // |x| >= 2^34
        if (magnitude > INFINITY_WORD) {
            return x + x;
        }

        if (word > 0) {
            return f_AtanHigh[3] + f_AtanLow[3];
        }
        else {
            return -f_AtanHigh[3] - f_AtanLow[3];
        }
    }

    if (magnitude < 0x3EE00000) { // |x| < 7/16
        if (magnitude < 0x31000000) { // |x| < 2^-29
            if (f_Huge + x > f_One) {
                return x;
            }
        }

        reductionIndex = -1;
    }
    else {
        x = fabsf(x);
        if (magnitude < 0x3F980000) { // |x| < 19/16
            if (magnitude < 0x3F300000) { // |x| < 11/16
                reductionIndex = 0;
                x = ((2.0f * x) - f_One) / (2.0f + x);
            }
            else {
                reductionIndex = 1;
                x = (x - f_One) / (x + f_One);
            }
        }
        else {
            if (magnitude < 0x401C0000) { // |x| < 39/16
                reductionIndex = 2;
                x = (x - 1.5f) / (f_One + (1.5f * x));
            }
            else {
                reductionIndex = 3;
                x = -1.0f / x;
            }
        }
    }

    xSquared = x * x;
    xFourth = xSquared * xSquared;
    oddSum = xSquared *
        (f_AtanTerms[0] +
            (xFourth *
                (f_AtanTerms[2] +
                    (xFourth *
                        (f_AtanTerms[4] +
                            (xFourth *
                                (f_AtanTerms[6] +
                                    (xFourth *
                                        (f_AtanTerms[8] + (xFourth * f_AtanTerms[10]))))))))));
    evenSum = xFourth *
        (f_AtanTerms[1] +
            (xFourth *
                (f_AtanTerms[3] +
                    (xFourth *
                        (f_AtanTerms[5] +
                            (xFourth * (f_AtanTerms[7] + (xFourth * f_AtanTerms[9]))))))));
    if (reductionIndex < 0) {
        return x - (x * (oddSum + evenSum));
    }
    else {
        angle = f_AtanHigh[reductionIndex] -
            (((x * (oddSum + evenSum)) - f_AtanLow[reductionIndex]) - x);
        return (word < 0) ? -angle : angle;
    }
}
