/*
 * The game's copy of libultra's matrix utilities, which sits among
 * the game's code and was built at -O2 like it, where libultra is
 * built at -O3. Unlike libultra's, guMtxF2L clamps each element to
 * between FIXED_ELEMENT_MIN and FIXED_ELEMENT_MAX before converting
 * it.
 */

#include <ultra64.h>

// Converts between floats and the 16.16 fixed point of a Mtx
#define FloatToFixed(value) (s32)((value) * (f32)0x10000)
#define FixedToFloat(value) ((f32)(value) / (f32)0x10000)

// The range guMtxF2L clamps elements to
#define FIXED_ELEMENT_MIN (-32768.0f)
#define FIXED_ELEMENT_MAX 32766.0f

// Clamps the float matrix in place, then converts it into the fixed-point
// matrix
void guMtxF2L(f32 mf[4][4], Mtx *m) {
    s32 row;
    s32 pair;
    s32 first;
    s32 second;
    s32 *integerParts;
    s32 *fractionParts;

    integerParts = (s32 *)&m->m[0][0];
    fractionParts = (s32 *)&m->m[2][0];
    for (row = 0; row < 4; row++) {
        for (pair = 0; pair < 2; pair++) {
            if (mf[row][pair * 2] < FIXED_ELEMENT_MIN) {
                mf[row][pair * 2] = FIXED_ELEMENT_MIN;
            }

            if (mf[row][pair * 2] > FIXED_ELEMENT_MAX) {
                mf[row][pair * 2] = FIXED_ELEMENT_MAX;
            }

            if (mf[row][(pair * 2) + 1] < FIXED_ELEMENT_MIN) {
                mf[row][(pair * 2) + 1] = FIXED_ELEMENT_MIN;
            }

            if (mf[row][(pair * 2) + 1] > FIXED_ELEMENT_MAX) {
                mf[row][(pair * 2) + 1] = FIXED_ELEMENT_MAX;
            }

            first = FloatToFixed(mf[row][pair * 2]);
            second = FloatToFixed(mf[row][(pair * 2) + 1]);
            *(integerParts++) = (first & 0xFFFF0000) | ((second >> 16) & 0xFFFF);
#ifdef AVOID_UB
            // A fraction of a half or more reaches the sign bit, where C
            // leaves a signed shift undefined
            *(fractionParts++) = (((u32)first << 16) & 0xFFFF0000) | (second & 0xFFFF);
#else
            *(fractionParts++) = ((first << 16) & 0xFFFF0000) | (second & 0xFFFF);
#endif
        }
    }
}

// Converts the fixed-point matrix into the float matrix
void guMtxL2F(f32 mf[4][4], Mtx *m) {
    s32 row;
    s32 pair;
    u32 first;
    u32 second;
    u32 *integerParts;
    u32 *fractionParts;
    s32 signedFirst;
    s32 signedSecond;

    integerParts = (u32 *)&m->m[0][0];
    fractionParts = (u32 *)&m->m[2][0];
    for (row = 0; row < 4; row++) {
        for (pair = 0; pair < 2; pair++) {
            first = (*integerParts & 0xFFFF0000) | ((*fractionParts >> 16) & 0xFFFF);
            second = ((*(integerParts++) << 16) & 0xFFFF0000) | (*(fractionParts++) & 0xFFFF);
            signedFirst = *((s32 *)&first);
            signedSecond = *((s32 *)&second);
            mf[row][pair * 2] = FixedToFloat(signedFirst);
            mf[row][(pair * 2) + 1] = FixedToFloat(signedSecond);
        }
    }
}

// Sets the matrix to the identity
void guMtxIdentF(f32 mf[4][4]) {
    s32 row;
    s32 column;

    for (row = 0; row < 4; row++) {
        for (column = 0; column < 4; column++) {
            if (row == column) {
                mf[row][column] = 1.0;
            }
            else {
                mf[row][column] = 0.0;
            }
        }
    }
}

void guMtxIdent(Mtx *m) {
    f32 mf[4][4];

    guMtxIdentF(mf);
    guMtxF2L(mf, m);
}
