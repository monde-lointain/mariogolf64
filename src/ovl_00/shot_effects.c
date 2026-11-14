#include "shot_effects.h"

#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "camera.h"
#include "character_slot.h"
#include "compiler.h"
#include "course_space.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "curve.h"
#include "effects.h"
#include "game.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "golf_balls.h"
#include "heap.h"
#include "hole_objects.h"
#include "math_util.h"
#include "movie.h"
#include "round_events.h"
#include "round_hud.h"
#include "screen.h"
#include "sound.h"

#define SPARK_COUNT 30
#define SPARK_BURST_COUNT 5
#define NO_SPARK_BURST (-1)
#define SPARK_LIFE 10
#define SPARK_BURST_TIME 20

#define IMPACT_SPARK_COUNT 20
#define IMPACT_SPARK_SPACING (M_PI * (2.0f / IMPACT_SPARK_COUNT))
#define IMPACT_SPARK_MIN_SPEED 8.0f
#define IMPACT_SPARK_SPEED_RANGE 16
#define FLAGSTICK_SPARK_COUNT 10
#define FLAGSTICK_SPARK_SPACING (M_PI * (2.0f / FLAGSTICK_SPARK_COUNT))
#define FLAGSTICK_SPARK_SPEED 12.0f

enum SparkKind {
    SPARK_KIND_FLAGSTICK,
    SPARK_KIND_IMPACT
};

#define IMPACT_FLASH_FULL_LEVEL 255

#define SPARK_IMAGE_ASSET 0x561
#define SWING_TRAIL_IMAGE_ASSET 0x973
#define SHARED_EFFECT_IMAGES_SIZE 0x2180
#define SHARED_EFFECT_IMAGE_ASSET 0x6AD
#define SHARED_EFFECT_SECOND_IMAGE_ASSET 0x969
#define SHARED_EFFECT_SECOND_IMAGE_OFFSET 0x648

enum SwingTrailState {
    SWING_TRAIL_NONE,
    SWING_TRAIL_SHOWN,
    SWING_TRAIL_FADING
};

#define SWING_TRAIL_OUTER_POINT 8
#define SWING_TRAIL_INNER_POINT 7
#define SWING_TRAIL_SAMPLES 14
#define POWER_SHOT_SWING_TRAIL_SAMPLES 14
#define NORMAL_SHOT_SWING_TRAIL_SAMPLES 14
#define SWING_TRAIL_MAX_SAMPLES 56
#define SWING_TRAIL_AXIS_COUNT 6
#define SWING_TRAIL_PATH_LENGTH (SWING_TRAIL_SAMPLES * 2)
#define SWING_TRAIL_MATRIX_COUNT 64
#define SWING_TRAIL_SEGMENTS 15
#define SWING_TRAIL_FULL_ALPHA 255
#define SWING_TRAIL_IMAGE_WIDTH 64
#define SWING_TRAIL_IMAGE_HEIGHT 32
#define SWING_TRAIL_TEXTURE_LENGTH 0x800

#define SPARK_IMAGE_SIZE 32
#define SPARK_DRAG 0.98f
#define SPARK_MODEL_Z (-100.0f)
#define SPARK_MODEL_SCALE 0.1f
#define SPARK_MAX_SCREEN_COORD 511
#define IsSparkCoordInRange(coord) \
    ((coord) >= -SPARK_MAX_SCREEN_COORD && (coord) <= SPARK_MAX_SCREEN_COORD)

static s32 f_ImpactFrameCount = 0;
static Vtx f_SparkVertices[3] = {
    {{{0, 160, 0}, 0, {0, 0}, {0xFF, 0x40, 0x00, 0xFF}}},
    {{{-12, -480, 0}, 0, {0, 0}, {0xFF, 0xFA, 0x7F, 0xDC}}},
    {{{12, -480, 0}, 0, {0, 0}, {0xFF, 0xFA, 0x7F, 0xDC}}},
};
static s32 f_UnusedWordsAfterSparkVertices[2] UNUSED = {0};
static s32 f_SwingTrailPathLength = 0;
static s32 f_SwingTrailSampleCount = SWING_TRAIL_SAMPLES;
static s32 f_SwingTrailHead = 0;
static s32 f_SwingTrailKind = 0;
static s32 f_SwingTrailPathIndex = 0;
static s32 f_SwingTrailLength = 0;
static s32 f_SwingTrailState = SWING_TRAIL_NONE;
static s32 f_SwingTrailAlpha = SWING_TRAIL_FULL_ALPHA;
static Vtx f_SwingTrailVertices[2] = {
    {{{0, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x00}}},
    {{{0, 0, 0}, 0, {0, 0}, {0xC8, 0xDC, 0xC8, 0xBF}}},
};
static Mtx f_SwingTrailOrigin = IDENTITY_MTX;

static Mtx f_SwingTrailProjections[DRAW_BUFFER_COUNT];
static Mtx f_SparkMatrices[DRAW_BUFFER_COUNT][SPARK_COUNT];
static Vec2 f_SparkOffsets[SPARK_COUNT];
static Vec2 f_SparkVelocities[SPARK_COUNT];
static Radians f_SparkAngles[SPARK_COUNT];
static u8 f_SparkLives[SPARK_COUNT];
static u8 f_SparkKinds[SPARK_COUNT];
static u8 f_SparkBursts[SPARK_COUNT];
static s8 f_SparkBurstAges[SPARK_BURST_COUNT];
static u8 f_SparkBurstTimers[SPARK_BURST_COUNT];
static CoursePoint f_SparkBurstPoints[SPARK_BURST_COUNT];
static u8 *f_SparkImage;
static u8 *f_SwingTrailImage;
static Mtx f_SwingTrailMatrices[SWING_TRAIL_MATRIX_COUNT][2];
static f32 f_SwingTrailOuterPath[SWING_TRAIL_PATH_LENGTH][3];
static f32 f_SwingTrailInnerPath[SWING_TRAIL_PATH_LENGTH][3];

u8 g_UnusedBytesAfterInnerTrailPath[0x18];

void ResetShotEffects(void) {
    /* Counts spark bursts and sparks in turn; a variable for each
       allocates registers differently. */
    s32 sharedIndex;

    f_ImpactFrameCount = 0;
    g_ImpactFlashState = IMPACT_FLASH_NONE;
    g_ContactEffect = CONTACT_EFFECT_NONE;
    for (sharedIndex = 0; sharedIndex != SPARK_BURST_COUNT; sharedIndex++) {
        f_SparkBurstAges[sharedIndex] = NO_SPARK_BURST;
    }

    for (sharedIndex = 0; sharedIndex != SPARK_COUNT; sharedIndex++) {
        f_SparkLives[sharedIndex] = 0;
    }
}

void ClearShotEffects(s32 kind) {
    osSyncPrintf("effect Clear!%d\n", kind);
    ShowWindFlyGuy(FLY_GUY_HIDDEN);
    StartShotEffect(SHOT_EFFECT_NONE);
    ClearEffects();
    f_ImpactFrameCount = 0;
    g_ImpactFlashState = IMPACT_FLASH_NONE;
    ClearHoleCreatures();
    StartContactEffect(CONTACT_EFFECT_NONE);
    if (kind == CLEAR_KIND_NICE_SHOT_BANNER) {
        StartBanner(BANNER_HIDE_NICE_SHOT);
    }

    if (kind == CLEAR_KIND_ALL) {
        StartMovie(MOVIE_NONE);
        StartBanner(BANNER_NONE);
    }

    ResetBallAfterimage();
    StartSwingTrail(SWING_TRAIL_KIND_CLEAR);
}

void StopAllSounds(void) {
    SoundChannel channel;

    for (channel = 0; channel != SOUND_CHANNEL_COUNT; channel++) {
        StopSound(channel);
    }
}

s32 StartSparkBurst(void) {
    s32 found = NO_SPARK_BURST;
    s32 burstIndex;

    for (burstIndex = 0; burstIndex != SPARK_BURST_COUNT; burstIndex++) {
        if (f_SparkBurstAges[burstIndex] == NO_SPARK_BURST) {
            found = burstIndex;
            f_SparkBurstAges[burstIndex] = 1;
            f_SparkBurstTimers[found] = SPARK_BURST_TIME;
            break;
        }
    }

    return found;
}

void StartImpactSparks(void) {
    s32 count = 0;
    s32 burst;
    s32 sparkIndex;

    f_ImpactFrameCount = 1;
    burst = StartSparkBurst();
    if (burst != NO_SPARK_BURST) {
        f_SparkBurstPoints[burst][AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
        f_SparkBurstPoints[burst][AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
        f_SparkBurstPoints[burst][AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
        for (sparkIndex = 0; sparkIndex != SPARK_COUNT; sparkIndex++) {
            if (f_SparkLives[sparkIndex] == 0) {
                f32 speed;

                f_SparkOffsets[sparkIndex][AXIS_X] = 0;
                f_SparkOffsets[sparkIndex][AXIS_Y] = 0;
                f_SparkAngles[sparkIndex] =
                    ((f32)count * IMPACT_SPARK_SPACING) + DegreesToRadians((guRandom() % 20) - 10);
                f_SparkKinds[sparkIndex] = SPARK_KIND_IMPACT;
                speed = (guRandom() % IMPACT_SPARK_SPEED_RANGE) + IMPACT_SPARK_MIN_SPEED;
                f_SparkVelocities[sparkIndex][AXIS_X] = sinf(f_SparkAngles[sparkIndex]) * speed;
                count++;
                f_SparkVelocities[sparkIndex][AXIS_Y] = -cosf(f_SparkAngles[sparkIndex]) * speed;
                f_SparkLives[sparkIndex] = SPARK_LIFE;
                f_SparkBursts[sparkIndex] = burst;
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

    f_SparkImage = AllocMemory(OpenEncodedAsset(SPARK_IMAGE_ASSET, &asset));
    ReadEncodedAsset(f_SparkImage, &asset);
    f_SwingTrailImage = AllocMemory(OpenEncodedAsset(SWING_TRAIL_IMAGE_ASSET, &asset));
    ReadEncodedAsset(f_SwingTrailImage, &asset);
    g_SharedEffectImages = AllocMemory(SHARED_EFFECT_IMAGES_SIZE);
    OpenEncodedAsset(SHARED_EFFECT_IMAGE_ASSET, &asset);
    ReadEncodedAsset(g_SharedEffectImages, &asset);
    OpenEncodedAsset(SHARED_EFFECT_SECOND_IMAGE_ASSET, &asset);
    ReadEncodedAsset(g_SharedEffectImages + SHARED_EFFECT_SECOND_IMAGE_OFFSET, &asset);
}

void FreeShotEffectImages(void) {
    FreeMemory(&f_SparkImage);
    FreeMemory(&f_SwingTrailImage);
    FreeMemory(&g_SharedEffectImages);
}

void StartFlagSparks(CoursePoint point UNUSED) {
}

void StartFlagstickSparks(CoursePoint point) {
    s32 count = 0;
    s32 burst = StartSparkBurst();
    s32 sparkIndex;

    if (burst != NO_SPARK_BURST) {
        f_SparkBurstPoints[burst][AXIS_X] = point[AXIS_X];
        f_SparkBurstPoints[burst][AXIS_Y] = point[AXIS_Y];
        f_SparkBurstPoints[burst][AXIS_Z] = point[AXIS_Z];
        for (sparkIndex = 0; sparkIndex != SPARK_COUNT; sparkIndex++) {
            if (f_SparkLives[sparkIndex] == 0) {
                f_SparkOffsets[sparkIndex][AXIS_X] = 0;
                f_SparkOffsets[sparkIndex][AXIS_Y] = 0;
                f_SparkAngles[sparkIndex] = (f32)count * FLAGSTICK_SPARK_SPACING;
                f_SparkKinds[sparkIndex] = SPARK_KIND_FLAGSTICK;
                count++;
                f_SparkVelocities[sparkIndex][AXIS_X] =
                    sinf(f_SparkAngles[sparkIndex]) * FLAGSTICK_SPARK_SPEED;
                f_SparkVelocities[sparkIndex][AXIS_Y] =
                    -cosf(f_SparkAngles[sparkIndex]) * FLAGSTICK_SPARK_SPEED;
                f_SparkLives[sparkIndex] = SPARK_LIFE;
                f_SparkBursts[sparkIndex] = burst;
            }

            if (count == FLAGSTICK_SPARK_COUNT) {
                break;
            }
        }
    }
}

#define IMPACT_FRAME_COUNT_END 100
#define IMPACT_FLASH_RISE_STEP 64
#define IMPACT_FLASH_FADE_STEP 32
#define IMPACT_FLASH_LOWEST_LEVEL 1
#define SPARK_BURST_SHOWN 1
#define SPARK_BURST_HIDDEN (-1)
#define SPARK_BURST_MAX_AGE 127
#define SPARK_DISTANCE_SCALE 0.3f
#define SPARK_MAX_SCALE 2.0f
#define SPARK_AGE_GROWTH 0.0625f
#define SPARK_START_SIZE 0.3f
#define SPARK_IMAGE_HALF_SIZE 8.0f
#define SPARK_TEXTURE_STEP_SCALE 2048.0f
#define SPARK_NO_TEXTURE_STEP 0xFFFF

void DrawSparks(Gfx **displayListHead) {
    Vec3 rotation;
    Vec3 translation;
    Vec3 scale;
    Mat4 matrix;
    ScreenPoint point;
    f32 burstScales[SPARK_BURST_COUNT];
    s32 burstShown[SPARK_BURST_COUNT];
    s32 burstPoints[SPARK_BURST_COUNT][3];
    s32 burstHalfSizes[SPARK_BURST_COUNT];
    s32 burstTextureSteps[SPARK_BURST_COUNT];
    Gfx *gfx = *displayListHead;
    f32 size;
    f32 halfSize;
    s32 burst;
    s32 left;
    s32 top;
    /* Counts spark bursts and sparks in turn; a variable for each
       allocates registers differently. */
    s32 sharedIndex;

    if (f_ImpactFrameCount != 0) {
        if (++f_ImpactFrameCount == IMPACT_FRAME_COUNT_END) {
            f_ImpactFrameCount = 0;
        }

        if (g_ImpactFlashState == IMPACT_FLASH_RISING) {
            g_ImpactFlashLevel += IMPACT_FLASH_RISE_STEP;
            if (g_ImpactFlashLevel > IMPACT_FLASH_FULL_LEVEL) {
                g_ImpactFlashLevel = IMPACT_FLASH_FULL_LEVEL;
                g_ImpactFlashState = IMPACT_FLASH_FADING;
            }
        }
        else if (g_ImpactFlashState == IMPACT_FLASH_FADING) {
            g_ImpactFlashLevel -= IMPACT_FLASH_FADE_STEP;
            if (g_ImpactFlashLevel < 0) {
                g_ImpactFlashLevel = IMPACT_FLASH_LOWEST_LEVEL;
                g_ImpactFlashState = IMPACT_FLASH_NONE;
            }
        }
    }

    for (sharedIndex = 0; sharedIndex != SPARK_BURST_COUNT; sharedIndex++) {
        if (f_SparkBurstAges[sharedIndex] != NO_SPARK_BURST) {
            if (ProjectPoint(
                    f_SparkBurstPoints[sharedIndex][AXIS_X] * COURSE_TO_WORLD_SCALE,
                    f_SparkBurstPoints[sharedIndex][AXIS_Y] * COURSE_TO_WORLD_SCALE,
                    f_SparkBurstPoints[sharedIndex][AXIS_Z] * COURSE_TO_WORLD_SCALE,
                    &point
                ) < 0) {
                size = (f_SparkBurstAges[sharedIndex] * SPARK_AGE_GROWTH) + SPARK_START_SIZE;
                burstShown[sharedIndex] = SPARK_BURST_SHOWN;
                burstPoints[sharedIndex][0] = point.x;
                burstPoints[sharedIndex][1] = point.y;
                burstPoints[sharedIndex][2] = point.depth;
                burstScales[sharedIndex] = point.scaleX * SPARK_DISTANCE_SCALE;
                if (burstScales[sharedIndex] > SPARK_MAX_SCALE) {
                    burstScales[sharedIndex] = SPARK_MAX_SCALE;
                }

                halfSize = size * SPARK_IMAGE_HALF_SIZE;
                burstHalfSizes[sharedIndex] = burstScales[sharedIndex] * halfSize;
                if (burstHalfSizes[sharedIndex] > 0) {
                    burstTextureSteps[sharedIndex] =
                        SPARK_TEXTURE_STEP_SCALE / (burstScales[sharedIndex] * size);
                }
                else {
                    burstTextureSteps[sharedIndex] = SPARK_NO_TEXTURE_STEP;
                }
            }
            else {
                burstShown[sharedIndex] = SPARK_BURST_HIDDEN;
            }

            if (f_SparkBurstAges[sharedIndex] < SPARK_BURST_MAX_AGE) {
                f_SparkBurstAges[sharedIndex]++;
            }
        }
        else {
            burstShown[sharedIndex] = SPARK_BURST_HIDDEN;
        }
    }

    gDPPipeSync(gfx++);
    AppendSyncedRdpState(gfx, gDPSetCycleType, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_DISABLE);
    AppendSyncedRdpState(gfx, gDPSetTextureLUT, G_TT_NONE);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    AppendSyncedRdpState(gfx, gDPSetTexturePersp, G_TP_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 0, OPAQUE_ALPHA);
    gDPSetCombineLERP(
        gfx++,
        0,
        0,
        0,
        PRIMITIVE,
        0,
        0,
        0,
        TEXEL0,
        0,
        0,
        0,
        PRIMITIVE,
        0,
        0,
        0,
        TEXEL0
    );
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 255, 255, 255, 1);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_SparkImage)),
        G_IM_FMT_I,
        SPARK_IMAGE_SIZE,
        SPARK_IMAGE_SIZE,
        0,
        G_TX_CLAMP,
        G_TX_CLAMP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    for (sharedIndex = 0; sharedIndex != SPARK_COUNT; sharedIndex++) {
        if (f_SparkLives[sharedIndex] != 0) {
            burst = f_SparkBursts[sharedIndex];
            f_SparkOffsets[sharedIndex][AXIS_X] += f_SparkVelocities[sharedIndex][AXIS_X];
            f_SparkOffsets[sharedIndex][AXIS_Y] += f_SparkVelocities[sharedIndex][AXIS_Y];
            f_SparkVelocities[sharedIndex][AXIS_X] *= SPARK_DRAG;
            f_SparkVelocities[sharedIndex][AXIS_Y] *= SPARK_DRAG;
            f_SparkLives[sharedIndex]--;
            if (burstShown[burst] == SPARK_BURST_SHOWN) {
                rotation[AXIS_X] = 0;
                rotation[AXIS_Y] = 0;
                rotation[AXIS_Z] = f_SparkAngles[sharedIndex];
                translation[AXIS_X] = (f_SparkOffsets[sharedIndex][AXIS_X] * burstScales[burst]) +
                    burstPoints[burst][0] - (SCREEN_WIDTH / 2);
                translation[AXIS_Y] = (f_SparkOffsets[sharedIndex][AXIS_Y] * burstScales[burst]) -
                    burstPoints[burst][1] + (SCREEN_HEIGHT / 2);
                translation[AXIS_Z] = SPARK_MODEL_Z;
                scale[AXIS_X] = burstScales[burst] * SPARK_MODEL_SCALE;
                scale[AXIS_Y] = burstScales[burst] * SPARK_MODEL_SCALE;
                scale[AXIS_Z] = burstScales[burst] * SPARK_MODEL_SCALE;
                if (f_SparkKinds[sharedIndex] == SPARK_KIND_IMPACT) {
                    BuildTransformMatrix(matrix, rotation, translation, scale);
                    ConvertMatrixToFixed(
                        matrix,
                        &f_SparkMatrices[g_MatrixBufferIndex][sharedIndex]
                    );
                }

                if (f_SparkKinds[sharedIndex] == SPARK_KIND_FLAGSTICK &&
                    IsSparkCoordInRange(burstPoints[burst][0]) &&
                    IsSparkCoordInRange(burstPoints[burst][1])) {
                    left = (f_SparkOffsets[sharedIndex][AXIS_X] * burstScales[burst]) +
                        burstPoints[burst][0];
                    top = (f_SparkOffsets[sharedIndex][AXIS_Y] * burstScales[burst]) +
                        burstPoints[burst][1];
                    gSPScisTextureRectangle(
                        gfx++,
                        QuarterPixels(left - burstHalfSizes[burst]),
                        QuarterPixels(top - burstHalfSizes[burst]),
                        QuarterPixels(left + burstHalfSizes[burst]),
                        QuarterPixels(top + burstHalfSizes[burst]),
                        G_TX_RENDERTILE,
                        0,
                        0,
                        burstTextureSteps[burst],
                        burstTextureSteps[burst]
                    );
                }
            }
        }
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_CLD_SURF2);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_OFF);
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    gDPSetCombineLERP(
        gfx++,
        PRIMITIVE,
        0,
        SHADE,
        0,
        0,
        0,
        0,
        SHADE,
        PRIMITIVE,
        0,
        SHADE,
        0,
        0,
        0,
        0,
        SHADE
    );
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    gDPPipeSync(gfx++);
    gSPMatrix(
        gfx++,
        PhysicalAddress(&g_ScreenOrthoMatrix),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    for (sharedIndex = 0; sharedIndex != SPARK_COUNT; sharedIndex++) {
        if (f_SparkKinds[sharedIndex] == SPARK_KIND_IMPACT &&
            burstShown[f_SparkBursts[sharedIndex]] == SPARK_BURST_SHOWN) {
            gDPSetPrimDepth(gfx++, burstPoints[f_SparkBursts[sharedIndex]][2], 0);
            if (f_SparkLives[sharedIndex] != 0) {
                gSPMatrix(
                    gfx++,
                    PhysicalAddress(&f_SparkMatrices[g_MatrixBufferIndex][sharedIndex]),
                    G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
                );
                gSPVertex(gfx++, f_SparkVertices, 3, 0);
                gSP1Triangle(gfx++, 0, 1, 2, 0);
            }
        }
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    gDPPipeSync(gfx++);
    for (sharedIndex = 0; sharedIndex != SPARK_BURST_COUNT; sharedIndex++) {
        if (f_SparkBurstTimers[sharedIndex] == 0) {
            f_SparkBurstAges[sharedIndex] = NO_SPARK_BURST;
        }
        else {
            f_SparkBurstTimers[sharedIndex]--;
        }
    }

    *displayListHead = gfx;
}

void FirstUnusedShotEffectsStub(void) {
}

void SecondUnusedShotEffectsStub(void) {
}

void DrawShotEffectsHud(Gfx **displayListHead UNUSED) {
}

void StartSwingTrail(s32 kind) {
    f32 samples[SWING_TRAIL_AXIS_COUNT][SWING_TRAIL_MAX_SAMPLES];
    f32 point[3];
    /* Counts samples, path points and matrices in turn; a variable for each
       allocates registers differently. */
    s32 sharedIndex;
    f32 position;

    if (kind == SWING_TRAIL_KIND_CLEAR) {
        f_SwingTrailState = SWING_TRAIL_NONE;
    }
    else if (kind == SWING_TRAIL_KIND_POWER_SHOT || kind == SWING_TRAIL_KIND_NORMAL_SHOT) {
        f_SwingTrailState = SWING_TRAIL_SHOWN;
        f_SwingTrailHead = 0;
        f_SwingTrailPathIndex = 0;
        f_SwingTrailAlpha = SWING_TRAIL_FULL_ALPHA;
        f_SwingTrailLength = 0;
        f_SwingTrailKind = kind;
        if (kind == SWING_TRAIL_KIND_POWER_SHOT) {
            f_SwingTrailSampleCount = POWER_SHOT_SWING_TRAIL_SAMPLES;
        }
        else {
            f_SwingTrailSampleCount = NORMAL_SHOT_SWING_TRAIL_SAMPLES;
        }

        for (sharedIndex = 0; sharedIndex != f_SwingTrailSampleCount; sharedIndex++) {
            GetCharacterPointPosition(0, SWING_TRAIL_OUTER_POINT, point);
            samples[0][sharedIndex] = point[AXIS_X];
            samples[1][sharedIndex] = point[AXIS_Y];
            samples[2][sharedIndex] = point[AXIS_Z];
            GetCharacterPointPosition(0, SWING_TRAIL_INNER_POINT, point);
            samples[3][sharedIndex] = point[AXIS_X];
            samples[4][sharedIndex] = point[AXIS_Y];
            samples[5][sharedIndex] = point[AXIS_Z];
            AnimateCharacterInSameCopy(0);
        }

        SetAnimationSpeed(0, -f_SwingTrailSampleCount);
        AnimateCharacterInSameCopy(0);
        SetAnimationSpeed(0, 1.0f);
        position = 0.0f;
        f_SwingTrailPathLength = (f_SwingTrailSampleCount * 2) - 4;
        for (sharedIndex = 0; sharedIndex != f_SwingTrailPathLength; sharedIndex++) {
            f_SwingTrailOuterPath[sharedIndex][AXIS_X] = InterpolateHermite(samples[0], position);
            f_SwingTrailOuterPath[sharedIndex][AXIS_Y] = InterpolateHermite(samples[1], position);
            f_SwingTrailOuterPath[sharedIndex][AXIS_Z] = InterpolateHermite(samples[2], position);
            f_SwingTrailInnerPath[sharedIndex][AXIS_X] = InterpolateHermite(samples[3], position);
            f_SwingTrailInnerPath[sharedIndex][AXIS_Y] = InterpolateHermite(samples[4], position);
            f_SwingTrailInnerPath[sharedIndex][AXIS_Z] = InterpolateHermite(samples[5], position);
            position += 0.5f;
        }

        for (sharedIndex = 0; sharedIndex != SWING_TRAIL_MATRIX_COUNT; sharedIndex++) {
            guTranslate(
                &f_SwingTrailMatrices[sharedIndex][0],
                f_SwingTrailOuterPath[0][AXIS_X],
                f_SwingTrailOuterPath[0][AXIS_Y],
                f_SwingTrailOuterPath[0][AXIS_Z]
            );
            guTranslate(
                &f_SwingTrailMatrices[sharedIndex][1],
                f_SwingTrailInnerPath[0][AXIS_X],
                f_SwingTrailInnerPath[0][AXIS_Y],
                f_SwingTrailInnerPath[0][AXIS_Z]
            );
        }
    }
    else if (f_SwingTrailState == SWING_TRAIL_SHOWN) {
        f_SwingTrailState = SWING_TRAIL_FADING;
    }
}

void DrawSwingTrail(Gfx **displayListHead, CameraMatrices *matrices) {
    f32 outerPoint[3];
    f32 innerPoint[3];
    Mat4 projection;
    s32 step;
    s32 segment;
    Gfx *gfx = *displayListHead;
    s32 textureStep;

    void LoadSwingTrailSegments(s32 first, s32 last, s32 increment) {
        last += increment;
        for (step = first; step != last; step += increment) {
            s32 matrixIndex = (f_SwingTrailHead + segment) % SWING_TRAIL_MATRIX_COUNT;
            s32 alpha;
            f32 nearness;
            f32 fade;

            gSPMatrix(
                gfx++,
                PhysicalAddress(&f_SwingTrailMatrices[matrixIndex][0]),
                G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
            );
            gSPVertex(gfx++, &f_SwingTrailVertices[0], 1, step * 2);
            gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
            gSPMatrix(
                gfx++,
                PhysicalAddress(&f_SwingTrailMatrices[matrixIndex][1]),
                G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
            );
            gSPVertex(gfx++, &f_SwingTrailVertices[1], 1, step * 2 + 1);
            gSPPopMatrix(gfx++, G_MTX_MODELVIEW);

            nearness = (SWING_TRAIL_SEGMENTS - segment) / 255.0f;
            fade = f_SwingTrailAlpha * nearness / SWING_TRAIL_SEGMENTS;
            alpha = fade * 191.0f;
            gSPModifyVertex(gfx++, step * 2, G_MWO_POINT_RGBA, 0xFFFFFF00);
            if (alpha > OPAQUE_ALPHA) {
                alpha = OPAQUE_ALPHA;
            }

            gSPModifyVertex(gfx++, step * 2 + 1, G_MWO_POINT_RGBA, 0xFFFFFF00 | alpha);
            gSPModifyVertex(
                gfx++,
                step * 2,
                G_MWO_POINT_ST,
                TexcoordWord(textureStep * segment - 1, 0x400)
            );
            gSPModifyVertex(
                gfx++,
                step * 2 + 1,
                G_MWO_POINT_ST,
                TexcoordWord(textureStep * segment - 1, 0)
            );
            segment++;
        }

        for (step = 0; step != SWING_TRAIL_SEGMENTS; step++) {
            gSP2Triangles(
                gfx++,
                step * 2,
                step * 2 + 2,
                step * 2 + 1,
                0,
                step * 2 + 2,
                step * 2 + 3,
                step * 2 + 1,
                0
            );
        }
    }

    if (f_SwingTrailState == SWING_TRAIL_NONE) {
        return;
    }

    f_SwingTrailAlpha =
        (f_SwingTrailState == SWING_TRAIL_FADING) ? f_SwingTrailAlpha - 16 : f_SwingTrailAlpha - 4;
    if (f_SwingTrailAlpha < 0) {
        f_SwingTrailState = SWING_TRAIL_NONE;
        return;
    }

    step = 0;
    while (TRUE) {
        f_SwingTrailPathIndex++;
        if (f_SwingTrailPathIndex > f_SwingTrailPathLength - 1) {
            f_SwingTrailPathIndex = f_SwingTrailPathLength - 1;
            GetCharacterPointPosition(0, SWING_TRAIL_OUTER_POINT, outerPoint);
            GetCharacterPointPosition(0, SWING_TRAIL_INNER_POINT, innerPoint);
            guTranslate(
                &f_SwingTrailMatrices[f_SwingTrailHead][0],
                outerPoint[AXIS_X],
                outerPoint[AXIS_Y],
                outerPoint[AXIS_Z]
            );
            guTranslate(
                &f_SwingTrailMatrices[f_SwingTrailHead][1],
                innerPoint[AXIS_X],
                innerPoint[AXIS_Y],
                innerPoint[AXIS_Z]
            );
        }
        else {
            guTranslate(
                &f_SwingTrailMatrices[f_SwingTrailHead][0],
                f_SwingTrailOuterPath[f_SwingTrailPathIndex][AXIS_X],
                f_SwingTrailOuterPath[f_SwingTrailPathIndex][AXIS_Y],
                f_SwingTrailOuterPath[f_SwingTrailPathIndex][AXIS_Z]
            );
            guTranslate(
                &f_SwingTrailMatrices[f_SwingTrailHead][1],
                f_SwingTrailInnerPath[f_SwingTrailPathIndex][AXIS_X],
                f_SwingTrailInnerPath[f_SwingTrailPathIndex][AXIS_Y],
                f_SwingTrailInnerPath[f_SwingTrailPathIndex][AXIS_Z]
            );
        }

        f_SwingTrailLength++;
        if (f_SwingTrailLength > SWING_TRAIL_SEGMENTS - 1) {
            f_SwingTrailLength = SWING_TRAIL_SEGMENTS - 1;
        }

        textureStep = SWING_TRAIL_TEXTURE_LENGTH / f_SwingTrailLength;
        if (step == 1) {
            break;
        }

        f_SwingTrailHead--;
        if (f_SwingTrailHead < 0) {
            f_SwingTrailHead = 1;
            f_SwingTrailHead = SWING_TRAIL_MATRIX_COUNT - f_SwingTrailHead;
        }

        step++;
    }

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
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(f_SwingTrailImage)));
    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(LargeImageTexels(f_SwingTrailImage)),
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        SWING_TRAIL_IMAGE_WIDTH,
        SWING_TRAIL_IMAGE_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    if (f_SwingTrailKind == SWING_TRAIL_KIND_POWER_SHOT) {
        gDPSetCombineLERP(
            gfx++,
            0,
            0,
            0,
            TEXEL0,
            TEXEL0,
            0,
            SHADE,
            0,
            0,
            0,
            0,
            TEXEL0,
            TEXEL0,
            0,
            SHADE,
            0
        );
    }
    else {
        gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    }

    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetEnvColor(gfx++, 0xFF, 0x7F, 0x00, OPAQUE_ALPHA);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0x00, f_SwingTrailAlpha);

    guMtxL2F(projection, &matrices->projection);
    projection[3][2] -= 2.0f;
    guMtxF2L(projection, &f_SwingTrailProjections[g_DrawBufferIndex]);
    segment = 0;
    gSPMatrix(
        gfx++,
        &f_SwingTrailProjections[g_DrawBufferIndex],
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_SwingTrailOrigin),
        G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
    );
    LoadSwingTrailSegments(0, SWING_TRAIL_SEGMENTS, 1);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, &matrices->projection, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    f_SwingTrailHead--;
    if (f_SwingTrailHead < 0) {
        f_SwingTrailHead = SWING_TRAIL_MATRIX_COUNT - 1;
    }

    *displayListHead = gfx;
}
