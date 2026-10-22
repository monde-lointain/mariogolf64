/*
 * Single-precision fdlibm routines: fabsf, acosf, atan2f and atanf.
 * They work on the bits of floats, unlike KMC's libm, which the game
 * also links and which works on doubles with CORDIC loops.
 */
#include "common.h"

typedef union {
    f32 value;
    s32 word;
} FloatShape;

#define GET_FLOAT_WORD(destination, source) \
    do {                                     \
        FloatShape shape;                   \
        shape.value = (source);             \
        (destination) = shape.word;         \
    } while (0)

#define SET_FLOAT_WORD(destination, source) \
    do {                                     \
        FloatShape shape;                   \
        shape.word = (source);              \
        (destination) = shape.value;        \
    } while (0)

#define SIGN_MASK 0x80000000
#define MAGNITUDE_MASK 0x7FFFFFFF
#define INFINITY_WORD 0x7F800000
#define ONE_WORD 0x3F800000

float atanf(float x);
float fabsf(float x);

/*
 * acos and atan2 split pi into a float and the error left over, so that
 * adding the error last keeps the result accurate to the last bit.
 */
static const float sOne = 1.0f;
static const float sPi = 3.1415925026e+00f;
static const float sHalfPiHigh = 1.5707962513e+00f;
static const float sHalfPiLow = 7.5497894159e-08f;
/* asin(sqrt(z)) ~ sqrt(z) + sqrt(z) * P(z) / Q(z) for z <= 1/4 */
static const float sAsinP0 = 1.6666667163e-01f;
static const float sAsinP1 = -3.2556581497e-01f;
static const float sAsinP2 = 2.0121252537e-01f;
static const float sAsinP3 = -4.0055535734e-02f;
static const float sAsinP4 = 7.9153501429e-04f;
static const float sAsinP5 = 3.4793309169e-05f;
static const float sAsinQ1 = -2.4033949375e+00f;
static const float sAsinQ2 = 2.0209457874e+00f;
static const float sAsinQ3 = -6.8828397989e-01f;
static const float sAsinQ4 = 7.7038154006e-02f;

float fabsf(float x) {
    s32 word;

    GET_FLOAT_WORD(word, x);
    SET_FLOAT_WORD(x, word & MAGNITUDE_MASK);
    return x;
}

float acosf(float x) {
    float z;
    float p;
    float q;
    float r;
    float w;
    float s;
    float c;
    float sqrtHigh;
    s32 word;
    s32 magnitude;

    GET_FLOAT_WORD(word, x);
    magnitude = word & MAGNITUDE_MASK;
    if (magnitude == ONE_WORD) {
        if (word > 0) {
            return 0.0f;
        } else {
            return sPi + 2.0f * sHalfPiLow;
        }
    } else if (magnitude > ONE_WORD) {
        /* NaN without fast math; the game's flags fold this to 0 */
        return (x - x) / (x - x);
    }
    if (magnitude < 0x3F000000) { /* |x| < 0.5 */
        if (magnitude <= 0x23000000) { /* |x| <= 2^-57 */
            return sHalfPiHigh + sHalfPiLow;
        }
        z = x * x;
        p = z * (sAsinP0 + z * (sAsinP1 + z * (sAsinP2 + z * (sAsinP3 + z * (sAsinP4 + z * sAsinP5)))));
        q = sOne + z * (sAsinQ1 + z * (sAsinQ2 + z * (sAsinQ3 + z * sAsinQ4)));
        r = p / q;
        return sHalfPiHigh - (x - (sHalfPiLow - x * r));
    } else if (word < 0) {
        z = (sOne + x) * 0.5f;
        p = z * (sAsinP0 + z * (sAsinP1 + z * (sAsinP2 + z * (sAsinP3 + z * (sAsinP4 + z * sAsinP5)))));
        q = sOne + z * (sAsinQ1 + z * (sAsinQ2 + z * (sAsinQ3 + z * sAsinQ4)));
        s = sqrtf(z);
        r = p / q;
        w = r * s - sHalfPiLow;
        return sPi - 2.0f * (s + w);
    } else {
        s32 sqrtHighWord;

        z = (sOne - x) * 0.5f;
        s = sqrtf(z);
        sqrtHigh = s;
        GET_FLOAT_WORD(sqrtHighWord, sqrtHigh);
        SET_FLOAT_WORD(sqrtHigh, sqrtHighWord & 0xFFFFF000);
        c = (z - sqrtHigh * sqrtHigh) / (s + sqrtHigh);
        p = z * (sAsinP0 + z * (sAsinP1 + z * (sAsinP2 + z * (sAsinP3 + z * (sAsinP4 + z * sAsinP5)))));
        q = sOne + z * (sAsinQ1 + z * (sAsinQ2 + z * (sAsinQ3 + z * sAsinQ4)));
        r = p / q;
        w = r * s + c;
        return 2.0f * (sqrtHigh + w);
    }
}

/*
 * fdlibm adds this to results to raise the inexact exception, but the
 * compiler folds the sums, which round to the other constant alone.
 */
static const float sTiny = 1.0e-30f;
static const float sZero = 0.0f;
static const float sQuarterPi = 7.8539818525e-01f;
static const float sHalfPi = 1.5707963705e+00f;
static const float sPiLow = 1.5099578832e-07f;

float atan2f(float y, float x) {
    float z;
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
    /* Bit 0 is the sign of y and bit 1 the sign of x */
    quadrant = ((yWord >> 31) & 1) | ((xWord >> 30) & 2);

    if (yMagnitude == 0) {
        switch (quadrant) {
            case 0:
            case 1:
                return y;
            case 2:
                return sPi + sTiny;
            case 3:
                return -sPi - sTiny;
        }
    }
    if (xMagnitude == 0) {
        return (yWord < 0) ? -sHalfPi - sTiny : sHalfPi + sTiny;
    }
    if (xMagnitude == INFINITY_WORD) {
        if (yMagnitude == INFINITY_WORD) {
            switch (quadrant) {
                case 0:
                    return sQuarterPi + sTiny;
                case 1:
                    return -sQuarterPi - sTiny;
                case 2:
                    return 3.0f * sQuarterPi + sTiny;
                case 3:
                    return -3.0f * sQuarterPi - sTiny;
            }
        } else {
            switch (quadrant) {
                case 0:
                    return sZero;
                case 1:
                    return -sZero;
                case 2:
                    return sPi + sTiny;
                case 3:
                    return -sPi - sTiny;
            }
        }
    }
    if (yMagnitude == INFINITY_WORD) {
        return (yWord < 0) ? -sHalfPi - sTiny : sHalfPi + sTiny;
    }

    exponentDifference = (yMagnitude - xMagnitude) >> 23;
    if (exponentDifference > 60) {
        z = sHalfPi + 0.5f * sPiLow;
    } else if (xWord < 0 && exponentDifference < -60) {
        z = 0.0f;
    } else {
        z = atanf(fabsf(y / x));
    }
    switch (quadrant) {
        case 0:
            return z;
        case 1: {
            u32 zWord;

            GET_FLOAT_WORD(zWord, z);
            SET_FLOAT_WORD(z, zWord ^ SIGN_MASK);
        }
            return z;
        case 2:
            return sPi - (z - sPiLow);
        default:
            return (z - sPiLow) - sPi;
    }
}

/* atan of 0.5, 1, 1.5 and infinity, split into a float and the error left over */
static const float sAtanHigh[] = {
    4.6364760399e-01f,
    7.8539812565e-01f,
    9.8279368877e-01f,
    1.5707962513e+00f,
};
static const float sAtanLow[] = {
    5.0121582440e-09f,
    3.7748947079e-08f,
    3.4473217170e-08f,
    7.5497894159e-08f,
};
/* atan(x) ~ x - x * (sum of sAtanTerms[i] * x^(2i+2)) for |x| <= 7/16 */
static const float sAtanTerms[] = {
    3.3333334327e-01f,  -2.0000000298e-01f, 1.4285714924e-01f,  -1.1111110449e-01f,
    9.0908870101e-02f,  -7.6918758452e-02f, 6.6610731184e-02f,  -5.8335702866e-02f,
    4.9768779427e-02f,  -3.6531571299e-02f, 1.6285819933e-02f,
};
static const float sHuge = 1.0e30f;

float atanf(float x) {
    float w;
    float oddSum;
    float evenSum;
    float z;
    s32 magnitude;
    s32 word;
    s32 reductionIndex;

    GET_FLOAT_WORD(word, x);
    magnitude = word & MAGNITUDE_MASK;
    if (magnitude >= 0x50800000) { /* |x| >= 2^34 */
        if (magnitude > INFINITY_WORD) {
            return x + x;
        }
        if (word > 0) {
            return sAtanHigh[3] + sAtanLow[3];
        } else {
            return -sAtanHigh[3] - sAtanLow[3];
        }
    }
    if (magnitude < 0x3EE00000) { /* |x| < 7/16 */
        if (magnitude < 0x31000000) { /* |x| < 2^-29 */
            if (sHuge + x > sOne) {
                return x;
            }
        }
        reductionIndex = -1;
    } else {
        x = fabsf(x);
        if (magnitude < 0x3F980000) { /* |x| < 19/16 */
            if (magnitude < 0x3F300000) { /* |x| < 11/16 */
                reductionIndex = 0;
                x = (2.0f * x - sOne) / (2.0f + x);
            } else {
                reductionIndex = 1;
                x = (x - sOne) / (x + sOne);
            }
        } else {
            if (magnitude < 0x401C0000) { /* |x| < 39/16 */
                reductionIndex = 2;
                x = (x - 1.5f) / (sOne + 1.5f * x);
            } else {
                reductionIndex = 3;
                x = -1.0f / x;
            }
        }
    }
    z = x * x;
    w = z * z;
    oddSum = z * (sAtanTerms[0] +
                  w * (sAtanTerms[2] + w * (sAtanTerms[4] + w * (sAtanTerms[6] + w * (sAtanTerms[8] + w * sAtanTerms[10])))));
    evenSum = w * (sAtanTerms[1] + w * (sAtanTerms[3] + w * (sAtanTerms[5] + w * (sAtanTerms[7] + w * sAtanTerms[9]))));
    if (reductionIndex < 0) {
        return x - x * (oddSum + evenSum);
    } else {
        z = sAtanHigh[reductionIndex] - ((x * (oddSum + evenSum) - sAtanLow[reductionIndex]) - x);
        return (word < 0) ? -z : z;
    }
}
