#include "common.h"
#include "asset.h"
#include "camera.h"
#include "character_slot.h"
#include "current_shot.h"
#include "effects.h"
#include "golf_balls.h"
#include "heap.h"
#include "math_util.h"
#include "movie.h"
#include "round_hud.h"
#include "shot_effects.h"
#include "sound.h"

/*
 * The sparks thrown off the ball by a driven shot or a hit on the flagstick,
 * and the trail that the club leaves through the swing. Each burst of sparks
 * spreads from a point that stays fixed in the world.
 */

#define SPARK_COUNT 30
#define SPARK_BURST_COUNT 5
#define NO_SPARK_BURST -1
/* The frames a spark lives and a burst's timer starts at */
#define SPARK_LIFE 10
#define SPARK_BURST_TIME 20

/* The impact flash's sparks, spread around the ball at random speeds */
#define IMPACT_SPARK_COUNT 20
#define IMPACT_SPARK_SPACING (M_PI * (2.0f / IMPACT_SPARK_COUNT))
#define IMPACT_SPARK_MIN_SPEED 8.0f
#define IMPACT_SPARK_SPEED_RANGE 16
/* The flagstick's sparks, spread evenly at one speed */
#define FLAGSTICK_SPARK_COUNT 10
#define FLAGSTICK_SPARK_SPACING (M_PI * (2.0f / FLAGSTICK_SPARK_COUNT))
#define FLAGSTICK_SPARK_SPEED 12.0f

enum SparkKind {
    SPARK_KIND_FLAGSTICK,
    SPARK_KIND_IMPACT
};

#define IMPACT_FLASH_FULL_LEVEL 255

/* The ways that ClearShotEffects ends the round's effects */
#define CLEAR_KIND_ROUND_END 1
#define CLEAR_KIND_HOLE_END 2

#define SOUND_CHANNEL_COUNT 16

#define SPARK_IMAGE_ASSET 0x561
#define SWING_TRAIL_IMAGE_ASSET 0x973
#define SHARED_EFFECT_IMAGES_SIZE 0x2180
#define SHARED_EFFECT_IMAGE_ASSET 0x6AD
#define SHARED_EFFECT_SECOND_IMAGE_ASSET 0x969
#define SHARED_EFFECT_SECOND_IMAGE_OFFSET 0x648

extern u8 *g_SharedEffectImages;
extern s32 D_802246F8_ovl_00;
/* Counts the frames from the last impact flash, back to 0 after 100 */
extern s32 sImpactFrameCount;

/* Each spark's offset from its burst's point on screen, and its speed */
extern f32 sSparkOffsets[SPARK_COUNT][2];
extern f32 sSparkVelocities[SPARK_COUNT][2];
extern f32 sSparkAngles[SPARK_COUNT];
/* The frames each spark has left; 0 when unused */
extern u8 sSparkLives[SPARK_COUNT];
extern u8 sSparkKinds[SPARK_COUNT];
extern u8 sSparkBursts[SPARK_COUNT];
/* Each burst's age in frames, or NO_SPARK_BURST */
extern s8 sSparkBurstAges[SPARK_BURST_COUNT];
extern s8 sSparkBurstTimers[SPARK_BURST_COUNT];
extern CoursePoint sSparkBurstPoints[SPARK_BURST_COUNT];
extern u8 *sSparkImage;
extern u8 *sSwingTrailImage;

/* The arguments of StartSwingTrail */
#define SWING_TRAIL_CLEAR -1
#define SWING_TRAIL_KIND_UNK1 1
#define SWING_TRAIL_KIND_UNK2 2

enum SwingTrailState {
    SWING_TRAIL_NONE,
    SWING_TRAIL_SHOWN,
    SWING_TRAIL_FADING
};

/* The character points at the two edges of the trail */
#define SWING_TRAIL_OUTER_POINT 8
#define SWING_TRAIL_INNER_POINT 7
/* The frames of the swing recorded, each a sample of both points */
#define SWING_TRAIL_SAMPLES 14
#define SWING_TRAIL_MAX_SAMPLES 56
#define SWING_TRAIL_AXIS_COUNT 6
/* The path between the samples, two steps to a sample */
#define SWING_TRAIL_PATH_LENGTH (SWING_TRAIL_SAMPLES * 2)
#define SWING_TRAIL_MATRIX_COUNT 64
#define SWING_TRAIL_SEGMENTS 15
#define SWING_TRAIL_FULL_ALPHA 255
#define SWING_TRAIL_IMAGE_WIDTH 64
#define SWING_TRAIL_IMAGE_HEIGHT 32
/* The S coordinate across the whole trail, in 10.5 fixed point */
#define SWING_TRAIL_TEXTURE_LENGTH 0x800

extern s32 sSwingTrailPathLength;
extern s32 sSwingTrailSampleCount;
/* The ring of matrices that sSwingTrailHead turns backwards through */
extern s32 sSwingTrailHead;
extern s32 sSwingTrailKind;
extern s32 sSwingTrailPathIndex;
extern s32 sSwingTrailLength;
extern s32 sSwingTrailState;
extern s32 sSwingTrailAlpha;
/* A vertex at the origin for each edge of the trail */
extern Vtx sSwingTrailVertices[2];
extern Mtx sSwingTrailOrigin;
extern Mtx sSwingTrailProjections[DRAW_BUFFER_COUNT];
extern Mtx sSwingTrailMatrices[SWING_TRAIL_MATRIX_COUNT][2];
extern f32 sSwingTrailOuterPath[SWING_TRAIL_PATH_LENGTH][3];
extern f32 sSwingTrailInnerPath[SWING_TRAIL_PATH_LENGTH][3];

void func_80213D70_ovl_00(s32);
void func_8020E7B4_ovl_00(s32);
void func_8020D9F0_ovl_00(void);
void func_80216130_ovl_00(s32);
f32 InterpolateHermite(f32 values[], f32 position);

void ResetShotEffects(void) {
    s32 i;

    sImpactFrameCount = 0;
    g_ImpactFlashState = IMPACT_FLASH_NONE;
    D_802246F8_ovl_00 = -1;
    for (i = 0; i != SPARK_BURST_COUNT; i++) {
        sSparkBurstAges[i] = NO_SPARK_BURST;
    }
    for (i = 0; i != SPARK_COUNT; i++) {
        sSparkLives[i] = 0;
    }
}

void ClearShotEffects(s32 kind) {
    osSyncPrintf("effect Clear!%d\n", kind);
    func_80216130_ovl_00(-1);
    StartShotEffect(-1);
    ClearEffects();
    sImpactFrameCount = 0;
    g_ImpactFlashState = IMPACT_FLASH_NONE;
    func_8020D9F0_ovl_00();
    func_80213D70_ovl_00(-1);
    if (kind == CLEAR_KIND_HOLE_END) {
        func_8020E7B4_ovl_00(-2);
    }
    if (kind == CLEAR_KIND_ROUND_END) {
        StartMovie(-1);
        func_8020E7B4_ovl_00(-1);
    }
    ResetBallAfterimage();
    StartSwingTrail(-1);
}

void StopAllSounds(void) {
    s32 channel;

    for (channel = 0; channel != SOUND_CHANNEL_COUNT; channel++) {
        StopSound(channel);
    }
}

/* Returns the index of a burst that was free and is now started, or NO_SPARK_BURST */
s32 StartSparkBurst(void) {
    s32 found = NO_SPARK_BURST;
    s32 i;

    for (i = 0; i != SPARK_BURST_COUNT; i++) {
        if (sSparkBurstAges[i] == NO_SPARK_BURST) {
            found = i;
            sSparkBurstAges[i] = 1;
            sSparkBurstTimers[found] = SPARK_BURST_TIME;
            break;
        }
    }
    return found;
}

/* Throws sparks off the ball as a driver strikes it, and flashes the golfer */
void StartImpactSparks(void) {
    s32 count = 0;
    s32 burst;
    s32 i;

    sImpactFrameCount = 1;
    burst = StartSparkBurst();
    if (burst != NO_SPARK_BURST) {
        sSparkBurstPoints[burst][AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
        sSparkBurstPoints[burst][AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
        sSparkBurstPoints[burst][AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
        for (i = 0; i != SPARK_COUNT; i++) {
            if (sSparkLives[i] == 0) {
                f32 speed;

                sSparkOffsets[i][0] = 0;
                sSparkOffsets[i][1] = 0;
                sSparkAngles[i] = (f32)count * IMPACT_SPARK_SPACING + DEGREES_TO_RADIANS(guRandom() % 20 - 10);
                sSparkKinds[i] = SPARK_KIND_IMPACT;
                speed = guRandom() % IMPACT_SPARK_SPEED_RANGE + IMPACT_SPARK_MIN_SPEED;
                sSparkVelocities[i][0] = sinf(sSparkAngles[i]) * speed;
                count++;
                sSparkVelocities[i][1] = -cosf(sSparkAngles[i]) * speed;
                sSparkLives[i] = SPARK_LIFE;
                sSparkBursts[i] = burst;
            }
            if (count == IMPACT_SPARK_COUNT) {
                break;
            }
        }
    }
    g_ImpactFlashState = IMPACT_FLASH_FADING;
    g_ImpactFlashLevel = IMPACT_FLASH_FULL_LEVEL;
}

void LoadShotEffectImages(void) {
    EncodedAsset asset;

    sSparkImage = AllocMemory(OpenEncodedAsset(SPARK_IMAGE_ASSET, &asset));
    ReadEncodedAsset(sSparkImage, &asset);
    sSwingTrailImage = AllocMemory(OpenEncodedAsset(SWING_TRAIL_IMAGE_ASSET, &asset));
    ReadEncodedAsset(sSwingTrailImage, &asset);
    g_SharedEffectImages = AllocMemory(SHARED_EFFECT_IMAGES_SIZE);
    OpenEncodedAsset(SHARED_EFFECT_IMAGE_ASSET, &asset);
    ReadEncodedAsset(g_SharedEffectImages, &asset);
    OpenEncodedAsset(SHARED_EFFECT_SECOND_IMAGE_ASSET, &asset);
    ReadEncodedAsset(g_SharedEffectImages + SHARED_EFFECT_SECOND_IMAGE_OFFSET, &asset);
}

void FreeShotEffectImages(void) {
    FreeMemory(&sSparkImage);
    FreeMemory(&sSwingTrailImage);
    FreeMemory(&g_SharedEffectImages);
}

/* Does nothing; the flag gives off no sparks */
void StartFlagSparks(CoursePoint point) {
}

/* Throws a ring of sparks off the flagstick at point */
void StartFlagstickSparks(CoursePoint point) {
    s32 count = 0;
    s32 burst = StartSparkBurst();
    s32 i;

    if (burst != NO_SPARK_BURST) {
        sSparkBurstPoints[burst][AXIS_X] = point[AXIS_X];
        sSparkBurstPoints[burst][AXIS_Y] = point[AXIS_Y];
        sSparkBurstPoints[burst][AXIS_Z] = point[AXIS_Z];
        for (i = 0; i != SPARK_COUNT; i++) {
            if (sSparkLives[i] == 0) {
                sSparkOffsets[i][0] = 0;
                sSparkOffsets[i][1] = 0;
                sSparkAngles[i] = (f32)count * FLAGSTICK_SPARK_SPACING;
                sSparkKinds[i] = SPARK_KIND_FLAGSTICK;
                count++;
                sSparkVelocities[i][0] = sinf(sSparkAngles[i]) * FLAGSTICK_SPARK_SPEED;
                sSparkVelocities[i][1] = -cosf(sSparkAngles[i]) * FLAGSTICK_SPARK_SPEED;
                sSparkLives[i] = SPARK_LIFE;
                sSparkBursts[i] = burst;
            }
            if (count == FLAGSTICK_SPARK_COUNT) {
                break;
            }
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/shot_effects", func_80217164_ovl_00);

void func_80217F44_ovl_00(void) {
}

void func_80217F4C_ovl_00(void) {
}

void func_80217F54_ovl_00(void) {
}

/*
 * Starts the trail of a swing of the given kind, recording the path of the
 * club through the swing's next frames, or with SWING_TRAIL_CLEAR stops it
 * at once. Any other kind lets the trail fade out quickly.
 */
#ifdef NON_MATCHING
void StartSwingTrail(s32 kind) {
    f32 samples[SWING_TRAIL_AXIS_COUNT][SWING_TRAIL_MAX_SAMPLES];
    f32 point[3];
    s32 i;
    f32 position;

    if (kind == SWING_TRAIL_CLEAR) {
        sSwingTrailState = SWING_TRAIL_NONE;
    } else if (kind == SWING_TRAIL_KIND_UNK1 || kind == SWING_TRAIL_KIND_UNK2) {
        sSwingTrailState = SWING_TRAIL_SHOWN;
        sSwingTrailHead = 0;
        sSwingTrailPathIndex = 0;
        sSwingTrailAlpha = SWING_TRAIL_FULL_ALPHA;
        sSwingTrailLength = 0;
        sSwingTrailKind = kind;
        sSwingTrailSampleCount = SWING_TRAIL_SAMPLES;
        for (i = 0; i != sSwingTrailSampleCount; i++) {
            GetCharacterPointPosition(0, SWING_TRAIL_OUTER_POINT, point);
            samples[0][i] = point[AXIS_X];
            samples[1][i] = point[AXIS_Y];
            samples[2][i] = point[AXIS_Z];
            GetCharacterPointPosition(0, SWING_TRAIL_INNER_POINT, point);
            samples[3][i] = point[AXIS_X];
            samples[4][i] = point[AXIS_Y];
            samples[5][i] = point[AXIS_Z];
            AnimateCharacterInSameCopy(0);
        }
        /* Rewinds the swing to where it was */
        SetAnimationSpeed(0, -sSwingTrailSampleCount);
        AnimateCharacterInSameCopy(0);
        SetAnimationSpeed(0, 1.0f);
        position = 0.0f;
        sSwingTrailPathLength = sSwingTrailSampleCount * 2 - 4;
        for (i = 0; i != sSwingTrailPathLength; i++) {
            sSwingTrailOuterPath[i][AXIS_X] = InterpolateHermite(samples[0], position);
            sSwingTrailOuterPath[i][AXIS_Y] = InterpolateHermite(samples[1], position);
            sSwingTrailOuterPath[i][AXIS_Z] = InterpolateHermite(samples[2], position);
            sSwingTrailInnerPath[i][AXIS_X] = InterpolateHermite(samples[3], position);
            sSwingTrailInnerPath[i][AXIS_Y] = InterpolateHermite(samples[4], position);
            sSwingTrailInnerPath[i][AXIS_Z] = InterpolateHermite(samples[5], position);
            position += 0.5f;
        }
        for (i = 0; i != SWING_TRAIL_MATRIX_COUNT; i++) {
            guTranslate(&sSwingTrailMatrices[i][0], sSwingTrailOuterPath[0][AXIS_X], sSwingTrailOuterPath[0][AXIS_Y],
                        sSwingTrailOuterPath[0][AXIS_Z]);
            guTranslate(&sSwingTrailMatrices[i][1], sSwingTrailInnerPath[0][AXIS_X], sSwingTrailInnerPath[0][AXIS_Y],
                        sSwingTrailInnerPath[0][AXIS_Z]);
        }
    } else if (sSwingTrailState == SWING_TRAIL_SHOWN) {
        sSwingTrailState = SWING_TRAIL_FADING;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/shot_effects", StartSwingTrail);
#endif

/*
 * Advances the swing trail two steps along its path and draws it as a
 * ribbon between the club's two points, fading towards its tail.
 */
#ifdef NON_MATCHING
void DrawSwingTrail(Gfx **displayListHead, CameraMatrices *matrices) {
    f32 outerPoint[3];
    f32 innerPoint[3];
    Mat4 projection;
    s32 step;
    s32 segment;
    Gfx *gfx = *displayListHead;
    s32 textureStep;

    /*
     * Loads the matrices and vertices of the trail's newest segments, from
     * first to last, and sets their alpha and texture coordinates.
     */
    void LoadSwingTrailSegments(s32 first, s32 last, s32 increment) {
        last += increment;
        for (step = first; step != last; step += increment) {
            s32 matrixIndex = (sSwingTrailHead + segment) % SWING_TRAIL_MATRIX_COUNT;
            s32 alpha;
            f32 nearness;
            f32 fade;

            gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sSwingTrailMatrices[matrixIndex][0]),
                      G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
            gSPVertex(gfx++, &sSwingTrailVertices[0], 1, step * 2);
            gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
            gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sSwingTrailMatrices[matrixIndex][1]),
                      G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
            gSPVertex(gfx++, &sSwingTrailVertices[1], 1, step * 2 + 1);
            gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
            /* Fades the segments towards the tail of the trail */
            nearness = (SWING_TRAIL_SEGMENTS - segment) / 255.0f;
            fade = sSwingTrailAlpha * nearness / SWING_TRAIL_SEGMENTS;
            alpha = fade * 191.0f;
            gSPModifyVertex(gfx++, step * 2, G_MWO_POINT_RGBA, 0xFFFFFF00);
            if (alpha > 255) {
                alpha = 255;
            }
            gSPModifyVertex(gfx++, step * 2 + 1, G_MWO_POINT_RGBA, 0xFFFFFF00 | alpha);
            gSPModifyVertex(gfx++, step * 2, G_MWO_POINT_ST, ((textureStep * segment - 1) << 16) + 0x400);
            gSPModifyVertex(gfx++, step * 2 + 1, G_MWO_POINT_ST, (textureStep * segment - 1) << 16);
            segment++;
        }
        for (step = 0; step != SWING_TRAIL_SEGMENTS; step++) {
            gSP2Triangles(gfx++, step * 2, step * 2 + 2, step * 2 + 1, 0, step * 2 + 2, step * 2 + 3, step * 2 + 1, 0);
        }
    }

    if (sSwingTrailState == SWING_TRAIL_NONE) {
        return;
    }
    sSwingTrailAlpha = (sSwingTrailState == SWING_TRAIL_FADING) ? sSwingTrailAlpha - 16 : sSwingTrailAlpha - 4;
    if (sSwingTrailAlpha < 0) {
        sSwingTrailState = SWING_TRAIL_NONE;
        return;
    }
    step = 0;
    do {
        sSwingTrailPathIndex++;
        if (sSwingTrailPathIndex > sSwingTrailPathLength - 1) {
            sSwingTrailPathIndex = sSwingTrailPathLength - 1;
            GetCharacterPointPosition(0, SWING_TRAIL_OUTER_POINT, outerPoint);
            GetCharacterPointPosition(0, SWING_TRAIL_INNER_POINT, innerPoint);
            guTranslate(&sSwingTrailMatrices[sSwingTrailHead][0], outerPoint[AXIS_X], outerPoint[AXIS_Y],
                        outerPoint[AXIS_Z]);
            guTranslate(&sSwingTrailMatrices[sSwingTrailHead][1], innerPoint[AXIS_X], innerPoint[AXIS_Y],
                        innerPoint[AXIS_Z]);
        } else {
            guTranslate(&sSwingTrailMatrices[sSwingTrailHead][0], sSwingTrailOuterPath[sSwingTrailPathIndex][AXIS_X],
                        sSwingTrailOuterPath[sSwingTrailPathIndex][AXIS_Y],
                        sSwingTrailOuterPath[sSwingTrailPathIndex][AXIS_Z]);
            guTranslate(&sSwingTrailMatrices[sSwingTrailHead][1], sSwingTrailInnerPath[sSwingTrailPathIndex][AXIS_X],
                        sSwingTrailInnerPath[sSwingTrailPathIndex][AXIS_Y],
                        sSwingTrailInnerPath[sSwingTrailPathIndex][AXIS_Z]);
        }
        sSwingTrailLength++;
        if (sSwingTrailLength > SWING_TRAIL_SEGMENTS - 1) {
            sSwingTrailLength = SWING_TRAIL_SEGMENTS - 1;
        }
        textureStep = SWING_TRAIL_TEXTURE_LENGTH / sSwingTrailLength;
        if (step == 1) {
            break;
        }
        sSwingTrailHead--;
        if (sSwingTrailHead < 0) {
            sSwingTrailHead = SWING_TRAIL_MATRIX_COUNT - 1;
        }
        step++;
    } while (TRUE);

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0x01);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, ((u32)sSwingTrailImage + IMAGE_HEADER_SIZE) & ~7);
    gDPLoadTextureBlock(gfx++, ((u32)sSwingTrailImage + LARGE_PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI, G_IM_SIZ_8b,
                        SWING_TRAIL_IMAGE_WIDTH, SWING_TRAIL_IMAGE_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    if (sSwingTrailKind == SWING_TRAIL_KIND_UNK1) {
        gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, TEXEL0, 0, SHADE, 0, 0, 0, 0, TEXEL0, TEXEL0, 0, SHADE, 0);
    } else {
        gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    }
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetEnvColor(gfx++, 0xFF, 0x7F, 0x00, 0xFF);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0x00, sSwingTrailAlpha);
    /* Draws the trail a little nearer than it is, so that the club does not hide it */
    guMtxL2F(projection, &matrices->projection);
    projection[3][2] -= 2.0f;
    guMtxF2L(projection, &sSwingTrailProjections[g_DrawBufferIndex]);
    segment = 0;
    gSPMatrix(gfx++, &sSwingTrailProjections[g_DrawBufferIndex], G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sSwingTrailOrigin), G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    LoadSwingTrailSegments(0, SWING_TRAIL_SEGMENTS, 1);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, &matrices->projection, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    sSwingTrailHead--;
    if (sSwingTrailHead < 0) {
        sSwingTrailHead = SWING_TRAIL_MATRIX_COUNT - 1;
    }
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/shot_effects", LoadSwingTrailSegments);
INCLUDE_ASM("asm/nonmatchings/ovl_00/shot_effects", DrawSwingTrail);
#endif
