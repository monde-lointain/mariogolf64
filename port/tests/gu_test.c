/*
 * Tests of the port's gu routines against transforms worked out by hand:
 * where each maps a few points, and the sequence that guRandom gives.
 */

#include <math.h>
#include <ultra64.h>

#include "test_support.h"

#define TOLERANCE 1e-5

// A link seam for the game's guNormalize, which lives in main.c
void guNormalize(f32 *x, f32 *y, f32 *z) {
    f32 inverse = 1.0f / sqrtf(*x * *x + *y * *y + *z * *z);

    *x *= inverse;
    *y *= inverse;
    *z *= inverse;
}

static void ExpectTransform(f32 mf[4][4], f32 x, f32 y, f32 z, f32 ex, f32 ey, f32 ez) {
    f32 ox;
    f32 oy;
    f32 oz;

    guMtxXFMF(mf, x, y, z, &ox, &oy, &oz);
    EXPECT_NEAR(ox, ex, TOLERANCE);
    EXPECT_NEAR(oy, ey, TOLERANCE);
    EXPECT_NEAR(oz, ez, TOLERANCE);
}

static void TestTranslateAndScale(void) {
    f32 translate[4][4];
    f32 scale[4][4];
    f32 both[4][4];

    guTranslateF(translate, 1.0f, 2.0f, 3.0f);
    ExpectTransform(translate, 1.0f, 1.0f, 1.0f, 2.0f, 3.0f, 4.0f);
    guScaleF(scale, 2.0f, 3.0f, 4.0f);
    ExpectTransform(scale, 1.0f, 1.0f, 1.0f, 2.0f, 3.0f, 4.0f);
    // Scaling first, then translating
    guMtxCatF(scale, translate, both);
    ExpectTransform(both, 1.0f, 1.0f, 1.0f, 3.0f, 5.0f, 7.0f);
}

static void TestRotations(void) {
    f32 mf[4][4];

    // A quarter turn about z takes x to y, counterclockwise from above
    guRotateF(mf, 90.0f, 0.0f, 0.0f, 5.0f);
    ExpectTransform(mf, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    // About x, y goes to z
    guRotateF(mf, 90.0f, 1.0f, 0.0f, 0.0f);
    ExpectTransform(mf, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    // A heading turns about z as guRotateF does
    guRotateRPYF(mf, 0.0f, 0.0f, 90.0f);
    ExpectTransform(mf, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    // Roll comes before pitch: rolling y onto z, then pitching z onto x
    guRotateRPYF(mf, 90.0f, 90.0f, 0.0f);
    ExpectTransform(mf, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f);
}

static void TestAlign(void) {
    f32 mf[4][4];

    // Facing down -z already, with no turn, is the identity
    guAlignF(mf, 0.0f, 0.0f, 0.0f, -1.0f);
    ExpectTransform(mf, 1.0f, 2.0f, 3.0f, 1.0f, 2.0f, 3.0f);
    // The z axis turns against the direction
    guAlignF(mf, 0.0f, 1.0f, 0.0f, 0.0f);
    ExpectTransform(mf, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f);
    // The turn spins x towards y
    guAlignF(mf, 90.0f, 0.0f, 0.0f, -1.0f);
    ExpectTransform(mf, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
}

static void TestLookAt(void) {
    f32 mf[4][4];
    LookAt lookAt;
    Hilite hilite;

    // From the origin down -z with y up is the identity
    guLookAtF(mf, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f);
    ExpectTransform(mf, 1.0f, 2.0f, 3.0f, 1.0f, 2.0f, 3.0f);
    // From +x looking at the origin, the origin lies ahead, at -5 in z
    guLookAtF(mf, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    ExpectTransform(mf, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -5.0f);
    ExpectTransform(mf, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, -5.0f);

    guLookAtHiliteF(
        mf,
        &lookAt,
        &hilite,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        -1.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        32,
        16
    );
    // A light along the view's back axis puts the highlight in the middle
    EXPECT(hilite.h.x1 == 32 * 4 && hilite.h.y1 == 16 * 4);
    // A light along x moves it half way to the right
    EXPECT(hilite.h.x2 == (int)(32 * 4 + sqrtf(0.5f) * 32 * 2));
    EXPECT(lookAt.l[0].l.dir[0] == 127 && lookAt.l[0].l.dir[1] == 0);
    EXPECT(lookAt.l[1].l.dir[1] == 127 && lookAt.l[1].l.col[1] == 0x80);
}

static void TestProjections(void) {
    f32 mf[4][4];
    u16 perspNorm;

    guOrthoF(mf, 0.0f, 320.0f, 0.0f, 240.0f, 1.0f, 10.0f, 1.0f);
    ExpectTransform(mf, 0.0f, 0.0f, -1.0f, -1.0f, -1.0f, -1.0f);
    ExpectTransform(mf, 320.0f, 240.0f, -10.0f, 1.0f, 1.0f, 1.0f);

    guPerspectiveF(mf, &perspNorm, 90.0f, 2.0f, 1.0f, 100.0f, 1.0f);
    EXPECT_NEAR(mf[0][0], 0.5, TOLERANCE);
    EXPECT_NEAR(mf[1][1], 1.0, TOLERANCE);
    EXPECT_NEAR(mf[2][3], -1.0, TOLERANCE);
    EXPECT(perspNorm == (u16)(131072.0 / 101.0));
    guPerspectiveF(mf, &perspNorm, 60.0f, 1.0f, 0.5f, 1.0f, 1.0f);
    EXPECT(perspNorm == 0xFFFF);
}

static void TestFixedPointForms(void) {
    Mtx m;
    f32 mf[4][4];

    guTranslate(&m, 1.5f, -2.25f, 3.0f);
    guMtxL2F(mf, &m);
    EXPECT_NEAR(mf[3][0], 1.5, TOLERANCE);
    EXPECT_NEAR(mf[3][1], -2.25, TOLERANCE);
    EXPECT_NEAR(mf[0][0], 1.0, TOLERANCE);
}

static void TestRandomSequence(void) {
    // The first values of libultra's generator, worked out apart from it
    EXPECT((u32)guRandom() == 992786854u);
    EXPECT((u32)guRandom() == 379911887u);
    EXPECT((u32)guRandom() == 479635344u);
}

int main(void) {
    TestTranslateAndScale();
    TestRotations();
    TestAlign();
    TestLookAt();
    TestProjections();
    TestFixedPointForms();
    TestRandomSequence();
    return ReportTestResults("gu_test");
}
