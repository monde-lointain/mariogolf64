#include "effects.h"

#include <nusys.h>
#include <stdlib.h>
#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "ball_simulation.h"
#include "camera.h"
#include "character_slot.h"
#include "club.h"
#include "compiler.h"
#include "controller.h"
#include "course_space.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "game.h"
#include "game_state.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "heap.h"
#include "hole_conditions.h"
#include "language.h"
#include "math_util.h"
#include "player.h"
#include "screen.h"
#include "screen_layers.h"
#include "shot_effects.h"
#include "sound.h"
#include "surface.h"
#include "terrain.h"
#include "text.h"
#include "turn.h"

#define POINT_SPREAD 3072
#define SPARKLE_FALL_SPEED (2.62f * COURSE_TO_WORLD_SCALE)
#define DROP_HEADING_COUNT 32
#define DROP_SLOWEST_SPEED 200
#define DROP_SPEED_STEP 20
#define DROP_SLOWEST_RISE_SPEED (2700 * COURSE_TO_WORLD_SCALE)
#define DEBRIS_SPEED_STEP 0x600
#define EFFECT_NEAREST_DEPTH (-3.0f)
#define DIVOT_IMAGE_SIZE 32
#define DIVOT_FRAME_SIZE (DIVOT_IMAGE_SIZE * DIVOT_IMAGE_SIZE / 2)
#define CIRCLE_GRAY 230
#define RUMBLE_FRAME_COUNT 10
#define RAIN_SPREAD_STEP 0x600
#define RAIN_FALL_SPEED 60.0f
#define RAIN_DRIFT_PER_WIND_SPEED 0.1f

#define RAINDROP_COUNT 48
#define RAINDROP_FALLING (-1)

#define RAINDROP_OFF_SCREEN (1 << 0)
#define RAINDROP_HEAD_GROUNDED (1 << 4)
#define RAIN_SPLASH_COUNT 32

struct Raindrop {
    Vec3 head;
    Vec3 tail;
    f32 groundHeight;
    f32 tailScale;
    s16 headScreenX;
    s16 headScreenY;
    s16 tailScreenX;
    s16 tailScreenY;
    s16 tailDepth;
    u16 flags;
    s8 landedFrameCount;
    u8 splashColumn;
};

struct RainSplash {
    Vec3 position;
    f32 ignoredStartValue;
    u8 unusedAfterStartValue[6];
    s8 frameCount;
    s8 ignoredFrameValue;
    u8 column;
};

enum DebrisKind {
    DEBRIS_KIND_NONE,
    DEBRIS_KIND_TEE_UP,
    DEBRIS_KIND_GRASS,
    DEBRIS_KIND_DUST,
    DEBRIS_KIND_ROUGH,
    DEBRIS_KIND_BURIED = 6
};

typedef s32 DebrisKind;

static Mtx f_RainModelViewMatrix;

static ImageAsset *f_RippleImage;
static ImageAsset *f_ArcImage;
static ImageAsset *f_DropImage;
static ImageAsset *f_DivotImage;
static ImageAsset *f_PlumeImage;
static ImageAsset *f_CloudImage;
static ImageAsset *f_ClumpImage;
static ImageAsset *f_DebrisImage;
static ImageAsset *f_SpeckleImage;
static ImageAsset *f_SplashImage;
static ImageAsset *f_StarImage;
static ImageAsset *f_DropletImage;

void DrawEffectShadow(Gfx **displayListHead, Effect *effect);
void DrawDrop(Gfx **displayListHead, CameraMatrices *matrices, Effect *unusedEffect);
void DrawDivot(Gfx **displayListHead, Effect *effect);
void DrawDivotMark(Gfx **displayListHead);
void DrawRipple(Gfx **displayListHead, Effect *effect);
void DrawSpreadingCircle(Gfx **displayListHead, Effect *effect);

f32 g_RainSoundVolume;

FILE_STATIC Effect f_Effects[EFFECT_COUNT];
FILE_STATIC Mtx f_EffectMatrices[EFFECT_COUNT];
FILE_STATIC Mtx f_DropMatrices[DRAW_BUFFER_COUNT];
FILE_STATIC Mtx f_DivotMarkMatrix;
FILE_STATIC ShotDebris f_ShotDebris;
FILE_STATIC s32 f_SpawnGroundHeight;
FILE_STATIC ImageAsset *f_RainSplashImage;
FILE_STATIC Raindrop *f_Raindrops;
FILE_STATIC RainSplash *f_RainSplashes;
FILE_STATIC s32 f_RainCenter[2];
FILE_STATIC f32 f_RainDriftX;
FILE_STATIC f32 f_RainDriftZ;
FILE_STATIC Vec3 f_RainVelocity;
FILE_STATIC s32 f_GroundCheckDropIndex;

static DebrisKind f_DebrisKind = DEBRIS_KIND_NONE;
static s32 f_NextMatrixIndex = 0;
static Vtx f_UprightQuadVertices[QUAD_VERTEX_COUNT] = {
    {{{-1, -1, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-1, 1, 0}, 0, {0, 0x400}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{1, 1, 0}, 0, {0x400, 0x400}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{1, -1, 0}, 0, {0x400, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static Vtx f_SmallFlatQuadVertices[QUAD_VERTEX_COUNT] = {
    {{{-3, 0, -3}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-3, 0, 3}, 0, {0x7FF, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{3, 0, 3}, 0, {0x7FF, 0x7FF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{3, 0, -3}, 0, {0, 0x7FF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static Vtx f_RippleVertices[QUAD_VERTEX_COUNT] = {
    {{{-7, 0, -7}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-7, 0, 7}, 0, {0xFFF, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{7, 0, 7}, 0, {0xFFF, 0xFFF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{7, 0, -7}, 0, {0, 0xFFF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static Vtx f_SpreadingCircleVertices[QUAD_VERTEX_COUNT] = {
    {{{-7, 0, -7}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-7, 0, 7}, 0, {0x1FFF, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{7, 0, 7}, 0, {0x1FFF, 0x1FFF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{7, 0, -7}, 0, {0, 0x1FFF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static Boolean f_AreEffectImagesLoaded = FALSE;
static f32 f_UnreferencedValues[] UNUSED = {1, 2, 3, 10, 11, 12, 13, 14, 20, 21, 22, 25, 30, 1};
static u8 f_PingPongFrameIndices[] = {0, 1, 2, 3, 2, 1, 0, 1, 2, 3, 2, 1};
static s32 f_RaindropCount = RAINDROP_COUNT;
static s32 f_RainSplashCount = RAIN_SPLASH_COUNT;
static Vtx f_RainStreakVertices[] = {
    {{{0, 0, -100}, 0, {0, 0}, {0xAF, 0xAF, 0xBF, 0x1F}}},
    {{{0, -1, -100}, 0, {0, 0}, {0xAF, 0xAF, 0xBF, 0x1F}}},
    {{{48, -1, -100}, 0, {0, 0}, {0x90, 0xA0, 0xA0, 0x5F}}},
    {{{48, 0, -100}, 0, {0, 0}, {0x90, 0xA0, 0xA0, 0x5F}}},
};
static Weather f_Weather = WEATHER_NONE;
static Vec3 f_LastRainEye = {0.0f, 0.0f, 0.0f};
static s32 f_ShownBadgeCount = 0;
static Boolean f_IsBadgeCountShown = FALSE;
static s32 f_BadgeCountFrame = 0;
void StartRain(void);

#define ASSET_RIPPLE_IMAGE 0x6B0
#define ASSET_DROP_IMAGE 0x6B2
#define ASSET_GRASS_IMAGE 0x6B6
#define ASSET_CLOUD_IMAGE 0x6B5
#define ASSET_CLUMP_IMAGE 0x6B8
#define ASSET_DIVOT_IMAGE 0x6C0
#define ASSET_PLUME_IMAGE 0x6C1
#define ASSET_SPLASH_IMAGE 0x6C2
#define ASSET_STAR_IMAGE 0x69E
#define ASSET_SPECKLE_IMAGE 0x71D
#define ASSET_ARC_IMAGE 0x8EC
#define ASSET_DROPLET_IMAGE 0x93B

void FreeEffectImages(void) {
    if (f_AreEffectImagesLoaded) {
        FreeMemory(&f_RippleImage);
        FreeMemory(&f_ArcImage);
        FreeMemory(&f_DropImage);
        FreeMemory(&f_DivotImage);
        FreeMemory(&f_PlumeImage);
        FreeMemory(&f_CloudImage);
        FreeMemory(&f_ClumpImage);
        FreeMemory(&f_DebrisImage);
        FreeMemory(&f_SpeckleImage);
        FreeMemory(&f_SplashImage);
        FreeMemory(&f_StarImage);
        FreeMemory(&f_DropletImage);
        f_AreEffectImagesLoaded = FALSE;
    }
}

void LoadEffectImages(void) {
    EncodedAsset asset;

    FreeEffectImages();
    if (!f_AreEffectImagesLoaded) {
        f_AreEffectImagesLoaded = TRUE;
        f_DebrisImage = AllocMemory(OpenEncodedAsset(ASSET_CLOUD_IMAGE, &asset));
        ReadEncodedAsset(f_DebrisImage, &asset);
        f_CloudImage = AllocMemory(OpenEncodedAsset(ASSET_CLOUD_IMAGE, &asset));
        ReadEncodedAsset(f_CloudImage, &asset);
        f_ClumpImage = AllocMemory(OpenEncodedAsset(ASSET_CLUMP_IMAGE, &asset));
        ReadEncodedAsset(f_ClumpImage, &asset);
        f_RippleImage = AllocMemory(OpenEncodedAsset(ASSET_RIPPLE_IMAGE, &asset));
        ReadEncodedAsset(f_RippleImage, &asset);
        f_ArcImage = AllocMemory(OpenEncodedAsset(ASSET_ARC_IMAGE, &asset));
        ReadEncodedAsset(f_ArcImage, &asset);
        f_DropImage = AllocMemory(OpenEncodedAsset(ASSET_DROP_IMAGE, &asset));
        ReadEncodedAsset(f_DropImage, &asset);
        f_DivotImage = AllocMemory(OpenEncodedAsset(ASSET_DIVOT_IMAGE, &asset));
        ReadEncodedAsset(f_DivotImage, &asset);
        f_PlumeImage = AllocMemory(OpenEncodedAsset(ASSET_PLUME_IMAGE, &asset));
        ReadEncodedAsset(f_PlumeImage, &asset);
        f_SpeckleImage = AllocMemory(OpenEncodedAsset(ASSET_SPECKLE_IMAGE, &asset));
        ReadEncodedAsset(f_SpeckleImage, &asset);
        f_SplashImage = AllocMemory(OpenEncodedAsset(ASSET_SPLASH_IMAGE, &asset));
        ReadEncodedAsset(f_SplashImage, &asset);
        f_StarImage = AllocMemory(OpenEncodedAsset(ASSET_STAR_IMAGE, &asset));
        ReadEncodedAsset(f_StarImage, &asset);
        f_DropletImage = AllocMemory(OpenEncodedAsset(ASSET_DROPLET_IMAGE, &asset));
        ReadEncodedAsset(f_DropletImage, &asset);
    }
}

void PrepareShotEffects(BallSimulation *simulation) {
    EncodedAsset asset;
    s32 index;
    Effect *effect;

    OpenEncodedAsset(ASSET_CLOUD_IMAGE, &asset);
    ReadEncodedAsset(f_CloudImage, &asset);
    OpenEncodedAsset(ASSET_CLUMP_IMAGE, &asset);
    ReadEncodedAsset(f_ClumpImage, &asset);
    OpenEncodedAsset(ASSET_RIPPLE_IMAGE, &asset);
    ReadEncodedAsset(f_RippleImage, &asset);
    OpenEncodedAsset(ASSET_DROP_IMAGE, &asset);
    ReadEncodedAsset(f_DropImage, &asset);
    OpenEncodedAsset(ASSET_DIVOT_IMAGE, &asset);
    ReadEncodedAsset(f_DivotImage, &asset);

    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        effect->kind = EFFECT_KIND_NONE;
    }

    f_SpawnGroundHeight = (f32)
        GetGroundHeight(g_CurrentShot.ballPosition[AXIS_X], g_CurrentShot.ballPosition[AXIS_Z]);
    if (simulation->surface == SURFACE_TEE_UP) {
        f_DebrisKind = DEBRIS_KIND_TEE_UP;
        OpenEncodedAsset(ASSET_GRASS_IMAGE, &asset);
        ReadEncodedAsset(f_DebrisImage, &asset);
    }
    else {
        switch (GetSurfaceTraits(g_CurrentShot.surface)->kind) {
            case SURFACE_KIND_FAIRWAY:
            case SURFACE_KIND_FAIRWAY_FAIR_LIE:
            case SURFACE_KIND_FAIRWAY_POOR_LIE:
            case SURFACE_KIND_TEE_GROUND:
            case SURFACE_KIND_SEMI_ROUGH:
                f_DebrisKind = DEBRIS_KIND_GRASS;
                OpenEncodedAsset(ASSET_GRASS_IMAGE, &asset);
                ReadEncodedAsset(f_DebrisImage, &asset);
                break;
            case SURFACE_KIND_ROUGH:
            case SURFACE_KIND_ROUGH_POOR_LIE:
            case SURFACE_KIND_ROUGH_UNUSED_LIE:
            case SURFACE_KIND_DEEP_ROUGH:
                f_DebrisKind = DEBRIS_KIND_ROUGH;
                OpenEncodedAsset(ASSET_CLUMP_IMAGE, &asset);
                ReadEncodedAsset(f_DebrisImage, &asset);
                break;
            case SURFACE_KIND_BUNKER:
            case SURFACE_KIND_BARE_GROUND:
                f_DebrisKind = DEBRIS_KIND_DUST;
                OpenEncodedAsset(ASSET_CLOUD_IMAGE, &asset);
                ReadEncodedAsset(f_DebrisImage, &asset);
                break;
            case SURFACE_KIND_BUNKER_FAIR_LIE:
            case SURFACE_KIND_BUNKER_POOR_LIE:
            case SURFACE_KIND_BUNKER_UNUSED_LIE:
                f_DebrisKind = DEBRIS_KIND_BURIED;
                OpenEncodedAsset(ASSET_CLOUD_IMAGE, &asset);
                ReadEncodedAsset(f_DebrisImage, &asset);
                break;
            default:
                f_DebrisKind = DEBRIS_KIND_NONE;
                break;
        }
    }

    ResetShotEffects();
}

void ClearEffects(void) {
    s32 index;
    Effect *effect;

    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        effect->kind = EFFECT_KIND_NONE;
    }
}

#define SPREADING_CIRCLE_RISE 1.5f

void SpawnSpreadingCircle(u32 slotIndex, s32 pointId) {
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 matrix;
    Vec3 point;
    s32 index;
    Effect *effect;

    if (GetCharacterPointPosition(slotIndex, pointId, point) == POINT_LOOKUP_FOUND) {
        f_SpawnGroundHeight = (f32)GetGroundHeight(
            point[AXIS_X] * WORLD_TO_COURSE_SCALE,
            point[AXIS_Z] * WORLD_TO_COURSE_SCALE
        );
        for (index = 0; index != EFFECT_COUNT; index++) {
            effect = &f_Effects[index];
            if (effect->kind == EFFECT_KIND_NONE) {
                effect->position[AXIS_X] = point[AXIS_X];
                effect->position[AXIS_Y] = point[AXIS_Y] - SPREADING_CIRCLE_RISE;
                effect->position[AXIS_Z] = point[AXIS_Z];
                effect->kind = EFFECT_KIND_SPREADING_CIRCLE;
                effect->frame = 0;
                effect->velocity[AXIS_X] = 0;
                effect->velocity[AXIS_Y] = 0;
                effect->velocity[AXIS_Z] = 0;
                effect->alpha = 127.0f;
                effect->alphaStep = -8.0f;
                effect->scale = 0.2f;
                effect->scaleStep = 0.8f;
                effect->matrixIndex = f_NextMatrixIndex;
                f_NextMatrixIndex = (f_NextMatrixIndex + 1) % EFFECT_COUNT;
                translation[AXIS_X] = effect->position[AXIS_X];
                translation[AXIS_Y] = effect->position[AXIS_Y];
                translation[AXIS_Z] = effect->position[AXIS_Z];
                rotation[AXIS_X] = 0.0f;
                rotation[AXIS_Y] = 0;
                rotation[AXIS_Z] = 0;
                scale[AXIS_X] = effect->scale;
                scale[AXIS_Y] = effect->scale;

                scale[AXIS_Z] = effect->scale;
                BuildTransformMatrix(matrix, rotation, translation, scale);
                ConvertMatrixToFixed(matrix, &f_EffectMatrices[effect->matrixIndex]);
                return;
            }
        }
    }
}

#define TEAR_OFFSET_SCALE (COURSE_TO_WORLD_SCALE / 5)
#define TEAR_SPEED_RANGE 1536
#define TEAR_MIRRORED_POINT 3

void SpawnTear(u32 slotIndex, s32 pointId, Radians heading) {
    Vec3 translation UNUSED;
    Vec3 rotation UNUSED;
    Vec3 scale UNUSED;
    Mat4 matrix UNUSED;
    Vec3 point;
    s32 index;
    Effect *effect;

    if (GetCharacterPointPosition(slotIndex, pointId, point) == POINT_LOOKUP_FOUND) {
        f_SpawnGroundHeight = (f32)GetGroundHeight(
            point[AXIS_X] * WORLD_TO_COURSE_SCALE,
            point[AXIS_Z] * WORLD_TO_COURSE_SCALE
        );
        for (index = 0; index != EFFECT_COUNT; index++) {
            effect = &f_Effects[index];
            if (effect->kind == EFFECT_KIND_NONE) {
                effect->position[AXIS_X] = point[AXIS_X] +
                    (((guRandom() % POINT_SPREAD) - (POINT_SPREAD / 2)) * TEAR_OFFSET_SCALE);
                effect->position[AXIS_Y] = point[AXIS_Y] +
                    (((guRandom() % POINT_SPREAD) - (POINT_SPREAD / 2)) * TEAR_OFFSET_SCALE);
                effect->position[AXIS_Z] = point[AXIS_Z] +
                    (((guRandom() % POINT_SPREAD) - (POINT_SPREAD / 2)) * TEAR_OFFSET_SCALE);
                effect->velocity[AXIS_X] = 0;
                effect->velocity[AXIS_Y] = SPARKLE_FALL_SPEED;
                effect->velocity[AXIS_X] = cosf(heading) *
                    ((guRandom() % TEAR_SPEED_RANGE) + (TEAR_SPEED_RANGE * 0.4)) /
                    WORLD_TO_COURSE_SCALE;
                effect->velocity[AXIS_Y] =
                    ((-TEAR_SPEED_RANGE * 0.7) - (guRandom() % TEAR_SPEED_RANGE * 0.3f)) /
                    WORLD_TO_COURSE_SCALE;
                effect->velocity[AXIS_Z] = sinf(heading) *
                    ((guRandom() % TEAR_SPEED_RANGE) + (TEAR_SPEED_RANGE * 0.4)) /
                    WORLD_TO_COURSE_SCALE;
                effect->position[AXIS_X] += effect->velocity[AXIS_X];
                effect->position[AXIS_Y] += effect->velocity[AXIS_Y];
                effect->position[AXIS_Z] += effect->velocity[AXIS_Z];
                effect->color = 0;
                effect->alpha = 159.0f;
                effect->alphaStep = -1.0f;
                effect->scale = 1.3f;
                effect->scaleStep = -0.04f;
                effect->kind = EFFECT_KIND_TEAR;
                if (pointId == TEAR_MIRRORED_POINT) {
                    effect->frame = 1;
                }
                else {
                    effect->frame = 0;
                }

                return;
            }
        }
    }
}

void SpawnSparkleNearPoint(u32 slotIndex, s32 pointId) {
    Vec3 translation UNUSED;
    Vec3 rotation UNUSED;
    Vec3 scale UNUSED;
    Mat4 matrix UNUSED;
    Vec3 point;
    s32 index;
    Effect *effect;

    if (GetCharacterPointPosition(slotIndex, pointId, point) == POINT_LOOKUP_FOUND) {
        f_SpawnGroundHeight = (f32)GetGroundHeight(
            point[AXIS_X] * WORLD_TO_COURSE_SCALE,
            point[AXIS_Z] * WORLD_TO_COURSE_SCALE
        );
        for (index = 0; index != EFFECT_COUNT; index++) {
            effect = &f_Effects[index];
            if (effect->kind == EFFECT_KIND_NONE) {
                effect->position[AXIS_X] = point[AXIS_X] +
                    (((guRandom() % POINT_SPREAD) - (POINT_SPREAD / 2)) * COURSE_TO_WORLD_SCALE);
                effect->position[AXIS_Y] = point[AXIS_Y] +
                    (((guRandom() % POINT_SPREAD) - (POINT_SPREAD / 2)) * COURSE_TO_WORLD_SCALE);
                effect->position[AXIS_Z] = point[AXIS_Z] +
                    (((guRandom() % POINT_SPREAD) - (POINT_SPREAD / 2)) * COURSE_TO_WORLD_SCALE);
                effect->velocity[AXIS_X] = 0;
                effect->velocity[AXIS_Y] = SPARKLE_FALL_SPEED;
                effect->velocity[AXIS_Z] = 0;
                effect->color = 0;
                effect->alpha = OPAQUE_ALPHA;
                effect->alphaStep = -4.0f;
                effect->scale = 6.0f;
                effect->scaleStep = -0.2f;
                effect->kind = EFFECT_KIND_SPARKLE;
                effect->frame = 0;
                return;
            }
        }
    }
}

void SpawnRipple(CoursePoint point) {
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 matrix;
    s32 index;
    Effect *effect;

    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        if (effect->kind == EFFECT_KIND_NONE) {
            effect->position[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            effect->kind = EFFECT_KIND_RIPPLE;
            effect->frame = 0;
            effect->matrixIndex = f_NextMatrixIndex;
            f_NextMatrixIndex = (f_NextMatrixIndex + 1) % EFFECT_COUNT;
            effect->velocity[AXIS_X] = 0;
            effect->velocity[AXIS_Y] = 0;
            effect->velocity[AXIS_Z] = 0;
            effect->alpha = 95.0f;
            effect->alphaStep = -2.0f;
            effect->scale = 1.0f;
            effect->scaleStep = 0.1f;
            translation[AXIS_X] = effect->position[AXIS_X];
            translation[AXIS_Y] = effect->position[AXIS_Y];
            translation[AXIS_Z] = effect->position[AXIS_Z];
            rotation[AXIS_X] = 0.0f;
            rotation[AXIS_Y] = 0;
            rotation[AXIS_Z] = 0;
            scale[AXIS_X] = effect->scale;
            scale[AXIS_Y] = effect->scale;

            scale[AXIS_Z] = effect->scale;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(matrix, &f_EffectMatrices[effect->matrixIndex]);
            return;
        }
    }
}

void SpawnDrop(CoursePoint point) {
    Vec3 translation;
    Vec3 scale;
    Vec3 rotation;
    Mat4 matrix;
    s32 index;
    Effect *effect;
    s32 random;

    s32 speedOffset;
    f32 speed;
    Radians heading;

    f_SpawnGroundHeight = (f32)GetGroundHeight(point[AXIS_X], point[AXIS_Z]);
    for (index = 0; index < EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        if (effect->kind == EFFECT_KIND_NONE) {
            effect->position[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            random = guRandom() % DROP_HEADING_COUNT;
            speedOffset = random * DROP_SPEED_STEP;
            speed = speedOffset + DROP_SLOWEST_SPEED;
            heading = (f32)(random * (M_PI / 2 / DROP_HEADING_COUNT)) - (M_PI / 4);
            effect->velocity[AXIS_X] = cosf(heading) * speed * COURSE_TO_WORLD_SCALE;
            effect->velocity[AXIS_Z] = sinf(heading) * speed * COURSE_TO_WORLD_SCALE;
            effect->velocity[AXIS_Y] =
                -DROP_SLOWEST_RISE_SPEED - (speedOffset * COURSE_TO_WORLD_SCALE);
            effect->tilt = 0;
            effect->kind = EFFECT_KIND_DROP;
            effect->alphaStep = 0;
            effect->scaleStep = 0;
            effect->alpha = OPAQUE_ALPHA;
            effect->scale = 0.5f;
            translation[AXIS_X] = effect->position[AXIS_X];
            translation[AXIS_Y] = effect->position[AXIS_Y];
            translation[AXIS_Z] = effect->position[AXIS_Z];
            rotation[AXIS_X] = effect->tilt;
            rotation[AXIS_Y] = 0;
            rotation[AXIS_Z] = effect->tilt;
            scale[AXIS_X] = effect->scale;
            scale[AXIS_Y] = effect->scale;
            scale[AXIS_Z] = effect->scale;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(matrix, &f_DropMatrices[g_DrawBufferIndex]);
            return;
        }
    }
}

void SpawnSparkle(CoursePoint point) {
    s32 index;
    Effect *effect;

    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        if (effect->kind == EFFECT_KIND_NONE) {
            effect->position[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            effect->velocity[AXIS_X] = 0;
            effect->velocity[AXIS_Y] = SPARKLE_FALL_SPEED;
            effect->velocity[AXIS_Z] = 0;
            effect->color = 0;
            effect->alpha = OPAQUE_ALPHA;
            effect->alphaStep = -4.0f;
            effect->scale = 24.0f;
            effect->scaleStep = -0.2f;
            effect->kind = EFFECT_KIND_SPARKLE;
            effect->frame = 0;
            return;
        }
    }
}

void SpawnCharacterSmokePuff(u32 slotIndex, s32 pointId) {
    Vec3 translation UNUSED;
    Vec3 rotation UNUSED;
    Vec3 scale UNUSED;
    Mat4 matrix UNUSED;
    Vec3 point;
    s32 index;
    s16 riseScale;
    Effect *effect;

    if (GetCharacterPointPosition(slotIndex, pointId, point) == POINT_LOOKUP_FOUND) {
        riseScale = 1;
        for (index = 0; index != EFFECT_COUNT; index++) {
            effect = &f_Effects[index];
            if (effect->kind == EFFECT_KIND_NONE) {
                effect->position[AXIS_X] = point[AXIS_X];
                effect->position[AXIS_Y] = point[AXIS_Y];
                effect->position[AXIS_Z] = point[AXIS_Z];
                effect->velocity[AXIS_X] = 0;
                effect->velocity[AXIS_Y] = riseScale * (-1.5 * 0.1);
                effect->velocity[AXIS_Z] = 0;
                effect->color = 0;
                effect->alpha = 230.0f;
                effect->alphaStep = -2.0f;
                effect->scale = 2.0f;
                effect->scaleStep = 0.02f;
                effect->kind = EFFECT_KIND_SMOKE_PUFF;
                effect->frame = 0;
                return;
            }
        }
    }
}

#define IMPACT_PUFF_COUNT 3

void SpawnImpactEffects(BallSimulation *simulation) {
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 matrix;
    s32 index;
    s32 puffNumber;
    s32 color;
    Effect *effect;

    color = BlendSurfaceMapColor(
        simulation->position[AXIS_X],
        simulation->position[AXIS_Z],
        simulation->surface
    );
    puffNumber = 1;
    switch (GetSurfaceTraits(simulation->surface)->kind) {
        case SURFACE_KIND_FAIRWAY:
        case SURFACE_KIND_FAIRWAY_FAIR_LIE:
        case SURFACE_KIND_FAIRWAY_POOR_LIE:
        case SURFACE_KIND_SEMI_ROUGH:
            for (index = 0; index != EFFECT_COUNT; index++) {
                effect = &f_Effects[index];
                if (effect->kind == EFFECT_KIND_NONE) {
                    effect->position[AXIS_X] =
                        g_TurnBallSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE;
                    effect->position[AXIS_Y] =
                        g_TurnBallSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                    effect->position[AXIS_Z] =
                        g_TurnBallSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                    effect->velocity[AXIS_X] = 0;
                    effect->velocity[AXIS_Y] =
                        -puffNumber * DEBRIS_SPEED_STEP * (0.2f * COURSE_TO_WORLD_SCALE);
                    effect->velocity[AXIS_Z] = 0;
                    effect->color = color;
                    effect->alpha = 230.0f;
                    effect->alphaStep = -2.0f;
                    effect->scale = 1.0f;
                    effect->scaleStep = 0.04f;
                    effect->kind = EFFECT_KIND_HIGH_PUFF;
                    effect->frame = 0;
                    puffNumber++;
                    if (puffNumber == IMPACT_PUFF_COUNT + 1) {
                        break;
                    }
                }
            }

            break;
        case SURFACE_KIND_BUNKER:
        case SURFACE_KIND_BUNKER_FAIR_LIE:
        case SURFACE_KIND_BUNKER_POOR_LIE:
        case SURFACE_KIND_BUNKER_UNUSED_LIE:
        case SURFACE_KIND_BARE_GROUND:
            for (index = 0; index != EFFECT_COUNT; index++) {
                effect = &f_Effects[index];
                if (effect->kind == EFFECT_KIND_NONE) {
                    effect->position[AXIS_X] =
                        g_TurnBallSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE;
                    effect->position[AXIS_Y] =
                        g_TurnBallSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                    effect->position[AXIS_Z] =
                        g_TurnBallSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                    effect->velocity[AXIS_X] = 0;
                    effect->velocity[AXIS_Y] =
                        -puffNumber * DEBRIS_SPEED_STEP * (0.7f * COURSE_TO_WORLD_SCALE);
                    effect->velocity[AXIS_Z] = 0;
                    effect->color = color;
                    effect->alpha = 230.0f;
                    effect->alphaStep = -2.0f;
                    effect->scale = 2.0f;
                    effect->scaleStep = 0.1f;
                    effect->kind = EFFECT_KIND_HIGH_PUFF;
                    effect->frame = 0;
                    puffNumber++;
                    if (puffNumber == IMPACT_PUFF_COUNT + 1) {
                        break;
                    }
                }
            }

            break;
        case SURFACE_KIND_WATER_HAZARD:
            for (index = 0; index != EFFECT_COUNT; index++) {
                effect = &f_Effects[index];
                if (effect->kind == EFFECT_KIND_NONE) {
                    effect->position[AXIS_X] =
                        g_TurnBallSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE;
                    effect->position[AXIS_Y] =
                        g_TurnBallSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                    effect->position[AXIS_Z] =
                        g_TurnBallSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                    effect->kind = EFFECT_KIND_RIPPLE;
                    effect->frame = 0;
                    effect->matrixIndex = f_NextMatrixIndex;
                    f_NextMatrixIndex = (f_NextMatrixIndex + 1) % EFFECT_COUNT;
                    effect->velocity[AXIS_X] = 0;
                    effect->velocity[AXIS_Y] = 0;
                    effect->velocity[AXIS_Z] = 0;
                    effect->alpha = 95.0f;
                    effect->alphaStep = -2.0f;
                    effect->scale = 1.0f;
                    effect->scaleStep = 0.1f;
                    translation[AXIS_X] = effect->position[AXIS_X];
                    translation[AXIS_Y] = effect->position[AXIS_Y];
                    translation[AXIS_Z] = effect->position[AXIS_Z];
                    rotation[AXIS_X] = 0.0f;
                    rotation[AXIS_Y] = 0;
                    rotation[AXIS_Z] = 0;
                    scale[AXIS_X] = effect->scale;
                    scale[AXIS_Y] = effect->scale;
                    scale[AXIS_Z] = effect->scale;
                    BuildTransformMatrix(matrix, rotation, translation, scale);
                    ConvertMatrixToFixed(matrix, &f_EffectMatrices[effect->matrixIndex]);
                    return;
                }
            }

            break;
        case SURFACE_KIND_DEEP_ROUGH:
            break;
    }
}

#define RISING_CLUMP_COUNT 3
#define RISING_CLUMP_HEIGHT 15360.0f

void SpawnRisingClumps(CoursePoint unusedPoint UNUSED) {
    s32 index;
    s32 spawnCount;
    Effect *effect;

    spawnCount = 0;
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        if (effect->kind == EFFECT_KIND_NONE) {
            effect->position[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Y] =
                (g_CurrentShot.ballPosition[AXIS_Y] - RISING_CLUMP_HEIGHT) * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            effect->velocity[AXIS_X] = 0;
            effect->velocity[AXIS_Y] =
                ((spawnCount * 0.3f) + 0.1f + (guRandom() % 10 * 0.01f)) * -1.5f;
            effect->velocity[AXIS_Z] = 0;
            effect->alpha = OPAQUE_ALPHA;
            effect->alphaStep = -1.0f;
            effect->scale = (guRandom() % 30 * 0.1f) + 1.0f;
            effect->scaleStep = (guRandom() % 10 * 0.001f) + 0.04;
            effect->frame = 0;
            effect->kind = EFFECT_KIND_RISING_CLUMP;
            spawnCount++;
            if (spawnCount == RISING_CLUMP_COUNT) {
                break;
            }
        }
    }
}

#define LAUNCH_SPEED_MOST 0xB400
#define DEBRIS_AMOUNT_LEAST 2.0f
#define DEBRIS_AMOUNT_MOST 10.0f
#define BURIED_DEBRIS_EXTRA 4.0f
#define DEBRIS_SLOWEST_STEP_COUNT 5
#define DEBRIS_OUTWARD_SCALE (0.03f * COURSE_TO_WORLD_SCALE)
#define PUTTED_DROP_SLOWEST_RISE_SPEED (1500 * COURSE_TO_WORLD_SCALE)
#define PUTTED_DROP_RISE_STEP 10
#define SHOT_DROP_SCALE 0.8f
#define DIVOT_SCALE_PER_AMOUNT 0.15f
#define DIVOT_SMALLEST_SCALE 1.0f
#define DIVOT_LARGEST_SCALE 1.5f
#define DIVOT_MARK_SCALE_PER_AMOUNT 0.07f
#define DIVOT_MARK_SMALLEST_SCALE 0.5f
#define DIVOT_MARK_LARGEST_SCALE 1.0f

#define RandomBelow(limit) ((limit) == 0 ? 0 : guRandom() % (limit))

void SpawnShotDebris(CoursePoint point, Radians pitch, Radians yaw, s32 launchSpeed) {
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 matrix;
    s32 spawnedCount;
    s32 index;
    f32 cosYaw;
    f32 sinPitch;
    f32 sinYaw;
    s32 groundColor;
    f32 debrisAmount;
    s32 nextIndex;
    s32 dropCount;
    s32 divotCount;
    s32 divotMarkCount;
    s32 lowDebrisCount;
    s32 highDebrisCount;
    Effect *effect;
    Effect *clearedEffect;
    Effect *divotEffect;
    Effect *divotMarkEffect;
    s32 headingIndex;
    f32 speed;
    Radians heading;

    s32 SpawnLowDebris(s32 slotCount, s32 debrisCount, EffectKind kind) {
        Effect *effect;
        s32 speed;

        spawnedCount = 0;
        for (index = 0; index < EFFECT_COUNT; index++) {
            effect = &f_Effects[index];
            if (spawnedCount == debrisCount) {
                break;
            }

            spawnedCount++;
            slotCount++;
            if (effect->kind == EFFECT_KIND_NONE) {
                effect->position[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
                effect->position[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                effect->position[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;

                speed = launchSpeed;
                if (kind == EFFECT_KIND_LOW_GRASS) {
                    speed = (RandomBelow(spawnedCount * 2) + DEBRIS_SLOWEST_STEP_COUNT) *
                        DEBRIS_SPEED_STEP;
                }
                else {
                    speed = (RandomBelow(spawnedCount * 3) + DEBRIS_SLOWEST_STEP_COUNT) *
                        DEBRIS_SPEED_STEP;
                }

                effect->velocity[AXIS_X] = cosYaw * speed * DEBRIS_OUTWARD_SCALE;
                effect->velocity[AXIS_Y] = sinPitch * speed * (0.1f * COURSE_TO_WORLD_SCALE);
                effect->velocity[AXIS_Z] = sinYaw * speed * DEBRIS_OUTWARD_SCALE;
                effect->color = groundColor;
                effect->alpha = guRandom() % 32 + 95;
                effect->alphaStep = -1.0f;
                if (kind == EFFECT_KIND_LOW_PUFF) {
                    effect->scale = (guRandom() % 16 + 10) * 0.2f;
                    effect->scaleStep = debrisAmount * 0.01f + 0.6f;
                }
                else {
                    effect->scale = 3.2f;
                    effect->scaleStep = debrisAmount * 0.05f + 0.2;
                }

                effect->kind = kind;
                effect->frame = 0;
            }
        }

        return slotCount;
    }

    s32 SpawnHighDebris(s32 slotCount, s32 debrisCount, EffectKind kind) {
        Effect *effect;
        s32 speed;

        spawnedCount = 0;
        for (index = 0; index < EFFECT_COUNT; index++) {
            effect = &f_Effects[index];

            guRandom();
            if (spawnedCount == debrisCount) {
                break;
            }

            spawnedCount++;
            slotCount++;
            if (effect->kind == EFFECT_KIND_NONE) {
                effect->position[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
                effect->position[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                effect->position[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                speed =
                    (RandomBelow(spawnedCount * 3) + DEBRIS_SLOWEST_STEP_COUNT) * DEBRIS_SPEED_STEP;
                effect->velocity[AXIS_X] = cosYaw * speed * DEBRIS_OUTWARD_SCALE;
                effect->velocity[AXIS_Y] = sinPitch * speed * (0.15f * COURSE_TO_WORLD_SCALE);
                effect->velocity[AXIS_Z] = sinYaw * speed * DEBRIS_OUTWARD_SCALE;
                effect->color = groundColor;
                if (kind == EFFECT_KIND_HIGH_PUFF) {
                    effect->alpha = guRandom() % 32 + 191;
                }
                else {
                    effect->alpha = guRandom() % 32 + 159;
                }

                effect->alphaStep = -2.0f;
                effect->scale = guRandom() % 32 * 0.2f;
                effect->scaleStep = debrisAmount * 0.01f + 0.4f;
                effect->kind = kind;
                effect->frame = 0;
            }
        }

        return slotCount;
    }

    nextIndex = 0;
    dropCount = 0;
    groundColor = BlendSurfaceMapColor(
        g_CurrentShot.ballPosition[AXIS_X],
        g_CurrentShot.ballPosition[AXIS_Z],
        g_CurrentShot.surface
    );
    sinPitch = sinf(pitch);
    cosYaw = cosf(yaw);
    divotCount = 0;
    highDebrisCount = 0;
    sinYaw = sinf(yaw);
    f_ShotDebris.ignoredSpawnCount = 0;
    f_ShotDebris.ignoredNegativeSetting = -2.0f;
    f_ShotDebris.ignoredPositiveSetting = 0.5f;
    lowDebrisCount = 0;
    divotMarkCount = 0;
    if (launchSpeed > LAUNCH_SPEED_MOST) {
        launchSpeed = LAUNCH_SPEED_MOST;
    }

    f_ShotDebris.clubCategory = GetSelectedClub()->category;
    if (f_ShotDebris.clubCategory == CLUB_CATEGORY_WOOD) {
        debrisAmount = -sinPitch * DEBRIS_AMOUNT_MOST;
    }
    else if (f_ShotDebris.clubCategory == CLUB_CATEGORY_IRON) {
        debrisAmount = -sinPitch * DEBRIS_AMOUNT_MOST;
    }
    else if (f_ShotDebris.clubCategory == CLUB_CATEGORY_WEDGE) {
        debrisAmount = -sinPitch * DEBRIS_AMOUNT_MOST;
    }
    else {
        debrisAmount = 0;
    }

    if (debrisAmount < DEBRIS_AMOUNT_LEAST) {
        debrisAmount = DEBRIS_AMOUNT_LEAST;
    }

    if (debrisAmount > DEBRIS_AMOUNT_MOST) {
        debrisAmount = DEBRIS_AMOUNT_MOST;
    }

    for (index = 0; index != EFFECT_COUNT; index++) {
        clearedEffect = &f_Effects[index];
        clearedEffect->kind = EFFECT_KIND_NONE;
    }

    if (f_DebrisKind == DEBRIS_KIND_TEE_UP) {
        dropCount = 1;
        if (f_ShotDebris.clubCategory == CLUB_CATEGORY_IRON) {
            lowDebrisCount = debrisAmount;
        }
        else if (f_ShotDebris.clubCategory == CLUB_CATEGORY_WEDGE) {
            lowDebrisCount = debrisAmount;
        }
    }
    else if (f_DebrisKind == DEBRIS_KIND_GRASS) {
        if (f_ShotDebris.clubCategory == CLUB_CATEGORY_WOOD) {
            lowDebrisCount = debrisAmount;
        }

        if (f_ShotDebris.clubCategory == CLUB_CATEGORY_IRON ||
            f_ShotDebris.clubCategory == CLUB_CATEGORY_WEDGE) {
            divotCount = 1;
            lowDebrisCount = 2.0f * debrisAmount;
            divotMarkCount = 1;
        }
    }
    else if (f_DebrisKind == DEBRIS_KIND_ROUGH) {
        lowDebrisCount = 2.0f * debrisAmount;
        highDebrisCount = 0;
    }
    else if (f_DebrisKind == DEBRIS_KIND_DUST) {
        lowDebrisCount = 2.0f * debrisAmount;
        highDebrisCount = lowDebrisCount;
    }
    else if (f_DebrisKind == DEBRIS_KIND_BURIED) {
        debrisAmount += BURIED_DEBRIS_EXTRA;
        if (debrisAmount > DEBRIS_AMOUNT_MOST) {
            debrisAmount = DEBRIS_AMOUNT_MOST;
        }

        lowDebrisCount = 2.0f * debrisAmount;
        highDebrisCount = lowDebrisCount;
    }

    spawnedCount = 0;
    for (index = nextIndex; index < EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        if (spawnedCount == dropCount) {
            break;
        }

        nextIndex++;
        if (effect->kind == EFFECT_KIND_NONE) {
            effect->position[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            headingIndex = g_CurrentShot.heading & (DROP_HEADING_COUNT - 1);
            speed = (headingIndex * DROP_SPEED_STEP) + DROP_SLOWEST_SPEED;
            heading = (f32)((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) +
                (f32)(headingIndex * (M_PI / 2 / DROP_HEADING_COUNT)) - (M_PI / 4);
            effect->velocity[AXIS_X] = cosf(heading) * speed * COURSE_TO_WORLD_SCALE;
            effect->velocity[AXIS_Z] = sinf(heading) * speed * COURSE_TO_WORLD_SCALE;
            if (f_ShotDebris.clubCategory == CLUB_CATEGORY_PUTTER) {
                effect->velocity[AXIS_Y] = -PUTTED_DROP_SLOWEST_RISE_SPEED -
                    (headingIndex * PUTTED_DROP_RISE_STEP * COURSE_TO_WORLD_SCALE);
            }
            else {
                effect->velocity[AXIS_Y] = -DROP_SLOWEST_RISE_SPEED -
                    (headingIndex * DROP_SPEED_STEP * COURSE_TO_WORLD_SCALE);
            }

            effect->tilt = 0.0f;
            effect->kind = EFFECT_KIND_DROP;
            effect->alphaStep = 0;
            effect->scaleStep = 0;
            effect->alpha = OPAQUE_ALPHA;
            effect->scale = SHOT_DROP_SCALE;
            translation[AXIS_X] = effect->position[AXIS_X];
            translation[AXIS_Y] = effect->position[AXIS_Y];
            translation[AXIS_Z] = effect->position[AXIS_Z];
            rotation[AXIS_X] = effect->tilt;
            rotation[AXIS_Y] = 0.0f;
            rotation[AXIS_Z] = effect->tilt;
            scale[AXIS_X] = effect->scale;
            scale[AXIS_Y] = effect->scale;
            spawnedCount++;
            scale[AXIS_Z] = effect->scale;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(matrix, &f_DropMatrices[g_DrawBufferIndex]);
        }
    }

    spawnedCount = 0;
    for (index = nextIndex; index < EFFECT_COUNT; index++) {
        divotEffect = &f_Effects[index];
        if (spawnedCount == divotCount) {
            break;
        }

        nextIndex++;
        if (divotEffect->kind == EFFECT_KIND_NONE) {
            divotEffect->position[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
            divotEffect->position[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            divotEffect->position[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            headingIndex = g_CurrentShot.heading & (DROP_HEADING_COUNT - 1);
            speed = (headingIndex * DROP_SPEED_STEP) + DROP_SLOWEST_SPEED;
            heading = (f32)((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) -
                (f32)(headingIndex * (M_PI / 2 / DROP_HEADING_COUNT)) + (M_PI / 4);
            divotEffect->velocity[AXIS_X] = cosf(heading) * speed * COURSE_TO_WORLD_SCALE;
            divotEffect->velocity[AXIS_Z] = sinf(heading) * speed * COURSE_TO_WORLD_SCALE;
            divotEffect->kind = EFFECT_KIND_DIVOT;
            divotEffect->tilt = 0;
            divotEffect->alphaStep = 0;
            divotEffect->alpha = OPAQUE_ALPHA;
            divotEffect->velocity[AXIS_Y] =
                -DROP_SLOWEST_RISE_SPEED - (headingIndex * DROP_SPEED_STEP * COURSE_TO_WORLD_SCALE);
            divotEffect->scale = debrisAmount * DIVOT_SCALE_PER_AMOUNT;
            if (divotEffect->scale < DIVOT_SMALLEST_SCALE) {
                divotEffect->scale = DIVOT_SMALLEST_SCALE;
            }

            if (divotEffect->scale > DIVOT_LARGEST_SCALE) {
                divotEffect->scale = DIVOT_LARGEST_SCALE;
            }

            divotEffect->scaleStep = 0;
            spawnedCount++;
        }
    }

    spawnedCount = 0;
    for (index = nextIndex; index < EFFECT_COUNT; index++) {
        divotMarkEffect = &f_Effects[index];
        if (spawnedCount == divotMarkCount) {
            break;
        }

        nextIndex++;
        if (divotMarkEffect->kind == EFFECT_KIND_NONE) {
            divotMarkEffect->position[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
            divotMarkEffect->position[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            divotMarkEffect->position[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            divotMarkEffect->velocity[AXIS_X] = 0;
            divotMarkEffect->velocity[AXIS_Y] = 0;
            divotMarkEffect->velocity[AXIS_Z] = 0;
            divotMarkEffect->scale = debrisAmount * DIVOT_MARK_SCALE_PER_AMOUNT;
            if (divotMarkEffect->scale < DIVOT_MARK_SMALLEST_SCALE) {
                divotMarkEffect->scale = DIVOT_MARK_SMALLEST_SCALE;
            }

            if (divotMarkEffect->scale > DIVOT_MARK_LARGEST_SCALE) {
                divotMarkEffect->scale = DIVOT_MARK_LARGEST_SCALE;
            }

            divotMarkEffect->alphaStep = 0;
            divotMarkEffect->scaleStep = 0;
            divotMarkEffect->kind = EFFECT_KIND_DIVOT_MARK;
            divotMarkEffect->alpha = OPAQUE_ALPHA;
            translation[AXIS_X] = divotMarkEffect->position[AXIS_X];
            translation[AXIS_Y] = divotMarkEffect->position[AXIS_Y];
            translation[AXIS_Z] = divotMarkEffect->position[AXIS_Z];
            rotation[AXIS_X] = 0.0f;
            rotation[AXIS_Y] = -yaw + M_PI;
            rotation[AXIS_Z] = 0.0f;
            scale[AXIS_X] = divotMarkEffect->scale;
            scale[AXIS_Y] = divotMarkEffect->scale;
            spawnedCount++;
            scale[AXIS_Z] = divotMarkEffect->scale;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(matrix, &f_DivotMarkMatrix);
        }
    }

    switch (f_DebrisKind) {
        case DEBRIS_KIND_TEE_UP:
        case DEBRIS_KIND_GRASS:
            SpawnLowDebris(nextIndex, lowDebrisCount, EFFECT_KIND_LOW_GRASS);
            break;
        case DEBRIS_KIND_ROUGH:
            nextIndex += SpawnLowDebris(nextIndex, lowDebrisCount, EFFECT_KIND_LOW_ROUGH);
            SpawnHighDebris(nextIndex, highDebrisCount, EFFECT_KIND_HIGH_ROUGH);
            break;
        case DEBRIS_KIND_DUST:
        case DEBRIS_KIND_BURIED:
            nextIndex += SpawnLowDebris(nextIndex, lowDebrisCount, EFFECT_KIND_LOW_PUFF);
            SpawnHighDebris(nextIndex, highDebrisCount, EFFECT_KIND_HIGH_PUFF);
            break;
    }
}

#define EFFECT_PUSH_PER_WIND_SPEED 0.005f
#define EFFECT_FALL_ACCELERATION (26.2f * COURSE_TO_WORLD_SCALE)
#define RISING_CLUMP_FALL_ACCELERATION (2 * EFFECT_FALL_ACCELERATION)
#define OTHER_EFFECT_FALL_ACCELERATION (8 * EFFECT_FALL_ACCELERATION)
#define TEAR_FALL_ACCELERATION 0.23027343f
#define RISING_CLUMP_VELOCITY_KEPT 0.95f
#define LIGHT_EFFECT_VELOCITY_KEPT 0.97f
#define HIGH_DEBRIS_VELOCITY_KEPT 0.8f
#define OTHER_EFFECT_VELOCITY_KEPT 0.95f
#define SMOKE_WIND_SHARE 0.1f
#define PAST_GROUND_FADE_SCALE 4.0f
#define TILT_FALL_SPEED 100.0f
#define TILT_PER_SPEED (M_PI * 0.001f)
#define HIGH_DEBRIS_FAST_RISE_SPEED (-1.5f)
#define HIGH_DEBRIS_SLOW_RISE_SPEED (-0.15f)
#define OTHER_EFFECT_FAST_RISE_SPEED (-1.5f)
#define OTHER_EFFECT_FAST_FALL_SPEED 1.5f
#define TEAR_FRAME_STEP 2
#define TEAR_LAST_FRAME 40
#define SPARKLE_LAST_FRAME 20
#define CYCLING_FRAME_COUNT 3
#define GROUNDED_EFFECT_DEPTH 15.36f

#define IsPastSpawnGround(effect) \
    ((f32)f_SpawnGroundHeight < (effect)->position[AXIS_Y] * WORLD_TO_COURSE_SCALE)
#define IsFadedPastGround(kind) \
    ((kind) != EFFECT_KIND_RIPPLE && \
        (kind) != EFFECT_KIND_SPREADING_CIRCLE && \
        (kind) != EFFECT_KIND_SPARKLE && \
        (kind) != EFFECT_KIND_TEAR)
#define IsRemovedWhenFaded(kind) \
    ((kind) != EFFECT_KIND_DIVOT_MARK && \
        (kind) != EFFECT_KIND_DROP && \
        (kind) != EFFECT_KIND_DIVOT && \
        (kind) != EFFECT_KIND_TEAR)

void UpdateEffects(void) {
    Mat4 matrix;
    Vec3 scale;
    Vec3 translation;
    Vec3 rotation;
    f32 direction;
    f32 speed;
    f32 windPushX;
    f32 windPushZ;
    s32 index;
    Effect *effect;

    direction = g_HoleSetup.conditions.windDirection * WIND_DIRECTION_TO_RADIANS;
    speed = g_HoleSetup.conditions.windSpeed / WIND_SPEED_UNIT * EFFECT_PUSH_PER_WIND_SPEED;
    windPushX = cosf(direction) * speed;
    windPushZ = sinf(direction) * speed;
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        if (effect->kind == EFFECT_KIND_NONE) {
            continue;
        }

        if (IsPastSpawnGround(effect) && IsFadedPastGround(effect->kind)) {
            if (effect->kind == EFFECT_KIND_RISING_CLUMP) {
                effect->alpha = 0.0f;
            }
            else {
                effect->alpha += effect->alphaStep * PAST_GROUND_FADE_SCALE;
            }
        }
        else if (effect->alpha > 0.0f || effect->scale > 0.0f) {
            effect->position[AXIS_X] += effect->velocity[AXIS_X];
            effect->position[AXIS_Y] += effect->velocity[AXIS_Y];
            effect->position[AXIS_Z] += effect->velocity[AXIS_Z];

            if (effect->kind == EFFECT_KIND_RISING_CLUMP) {
                effect->velocity[AXIS_X] += windPushX;
                effect->velocity[AXIS_Y] += RISING_CLUMP_FALL_ACCELERATION;
                effect->velocity[AXIS_Z] += windPushZ;
                effect->velocity[AXIS_X] *= RISING_CLUMP_VELOCITY_KEPT;
                effect->velocity[AXIS_Y] *= RISING_CLUMP_VELOCITY_KEPT;
                effect->velocity[AXIS_Z] *= RISING_CLUMP_VELOCITY_KEPT;
            }
            else if (effect->kind == EFFECT_KIND_TEAR) {
                effect->velocity[AXIS_Y] += TEAR_FALL_ACCELERATION;
                effect->velocity[AXIS_X] *= LIGHT_EFFECT_VELOCITY_KEPT;
                effect->velocity[AXIS_Z] *= LIGHT_EFFECT_VELOCITY_KEPT;
                effect->velocity[AXIS_Y] *= LIGHT_EFFECT_VELOCITY_KEPT;
            }
            else if (effect->kind == EFFECT_KIND_SPARKLE) {
                effect->velocity[AXIS_Y] += EFFECT_FALL_ACCELERATION;
                effect->velocity[AXIS_X] *= LIGHT_EFFECT_VELOCITY_KEPT;
                effect->velocity[AXIS_Z] *= LIGHT_EFFECT_VELOCITY_KEPT;
                effect->velocity[AXIS_Y] *= LIGHT_EFFECT_VELOCITY_KEPT;
            }
            else if (effect->kind == EFFECT_KIND_SMOKE_PUFF) {
                effect->velocity[AXIS_X] += windPushX * SMOKE_WIND_SHARE;
                effect->velocity[AXIS_Y] += 0.0f;
                effect->velocity[AXIS_Z] += windPushZ * SMOKE_WIND_SHARE;
                effect->velocity[AXIS_X] *= LIGHT_EFFECT_VELOCITY_KEPT;
                effect->velocity[AXIS_Y] *= LIGHT_EFFECT_VELOCITY_KEPT;
                effect->velocity[AXIS_Z] *= LIGHT_EFFECT_VELOCITY_KEPT;
            }
            else if (effect->kind == EFFECT_KIND_HIGH_PUFF ||
                effect->kind == EFFECT_KIND_HIGH_ROUGH) {
                effect->velocity[AXIS_X] += windPushX;
                effect->velocity[AXIS_Y] += EFFECT_FALL_ACCELERATION;
                effect->velocity[AXIS_Z] += windPushZ;
                effect->velocity[AXIS_X] *= HIGH_DEBRIS_VELOCITY_KEPT;
                effect->velocity[AXIS_Y] *= HIGH_DEBRIS_VELOCITY_KEPT;
                effect->velocity[AXIS_Z] *= HIGH_DEBRIS_VELOCITY_KEPT;
            }
            else if (effect->kind == EFFECT_KIND_RIPPLE ||
                effect->kind == EFFECT_KIND_SPREADING_CIRCLE) {
            }
            else {
                effect->velocity[AXIS_Y] += OTHER_EFFECT_FALL_ACCELERATION;
                effect->velocity[AXIS_X] *= OTHER_EFFECT_VELOCITY_KEPT;
                effect->velocity[AXIS_Z] *= OTHER_EFFECT_VELOCITY_KEPT;
                effect->velocity[AXIS_Y] *= OTHER_EFFECT_VELOCITY_KEPT;
            }

            effect->alpha += effect->alphaStep;
            effect->scale += effect->scaleStep;
            if (effect->velocity[AXIS_Y] < TILT_FALL_SPEED) {
                effect->tilt -= (effect->velocity[AXIS_Y] - TILT_FALL_SPEED) * TILT_PER_SPEED;
            }

            if (effect->tilt > FULL_TURN_RADIANS) {
                effect->tilt -= FULL_TURN_RADIANS;
            }

            if (effect->tilt < 0.0f) {
                effect->tilt += FULL_TURN_RADIANS;
            }

            if (effect->kind == EFFECT_KIND_HIGH_PUFF || effect->kind == EFFECT_KIND_HIGH_ROUGH) {
                if (effect->velocity[AXIS_Y] < HIGH_DEBRIS_FAST_RISE_SPEED) {
                    effect->frame = 0;
                }
                else if (effect->velocity[AXIS_Y] < HIGH_DEBRIS_SLOW_RISE_SPEED) {
                    effect->frame = 1;
                }
                else {
                    effect->frame = 2;
                }
            }
            else if (effect->kind == EFFECT_KIND_TEAR) {
                effect->frame += TEAR_FRAME_STEP;
                if (effect->frame > TEAR_LAST_FRAME) {
                    effect->kind = EFFECT_KIND_NONE;
                }
            }
            else if (effect->kind == EFFECT_KIND_SPARKLE) {
                if (effect->frame < SPARKLE_LAST_FRAME) {
                    effect->frame++;
                }
                else {
                    effect->kind = EFFECT_KIND_NONE;
                }
            }
            else if (effect->kind == EFFECT_KIND_RISING_CLUMP ||
                effect->kind == EFFECT_KIND_SMOKE_PUFF) {
                effect->frame = (effect->frame + 1) % CYCLING_FRAME_COUNT;
            }
            else {
                if (effect->velocity[AXIS_Y] < OTHER_EFFECT_FAST_RISE_SPEED) {
                    effect->frame = 0;
                }
                else if (effect->velocity[AXIS_Y] > OTHER_EFFECT_FAST_FALL_SPEED) {
                    effect->frame = 2;
                }
                else {
                    effect->frame = 1;
                }

                effect->frame = 0;
            }

            if (IsPastSpawnGround(effect) &&
                (effect->kind == EFFECT_KIND_DROP || effect->kind == EFFECT_KIND_DIVOT)) {
                effect->position[AXIS_Y] =
                    (f_SpawnGroundHeight + GROUNDED_EFFECT_DEPTH) * COURSE_TO_WORLD_SCALE;
            }
        }

        if (effect->kind == EFFECT_KIND_RIPPLE) {
            effect->matrixIndex = f_NextMatrixIndex;
            f_NextMatrixIndex = (f_NextMatrixIndex + 1) % EFFECT_COUNT;
            translation[AXIS_X] = effect->position[AXIS_X];
            translation[AXIS_Y] = effect->position[AXIS_Y];
            translation[AXIS_Z] = effect->position[AXIS_Z];
            rotation[AXIS_X] = 0.0f;
            rotation[AXIS_Y] = 0;
            rotation[AXIS_Z] = 0.0f;
            scale[AXIS_X] = effect->scale;
            scale[AXIS_Y] = effect->scale;
            scale[AXIS_Z] = effect->scale;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(matrix, &f_EffectMatrices[effect->matrixIndex]);
        }

        if (effect->kind == EFFECT_KIND_SPREADING_CIRCLE) {
            effect->matrixIndex = f_NextMatrixIndex;
            f_NextMatrixIndex = (f_NextMatrixIndex + 1) % EFFECT_COUNT;
            translation[AXIS_X] = effect->position[AXIS_X];
            translation[AXIS_Y] = effect->position[AXIS_Y];
            translation[AXIS_Z] = effect->position[AXIS_Z];
            rotation[AXIS_X] = 0.0f;
            rotation[AXIS_Y] = 0;
            rotation[AXIS_Z] = 0.0f;
            scale[AXIS_X] = effect->scale;
            scale[AXIS_Y] = effect->scale;
            scale[AXIS_Z] = effect->scale;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(matrix, &f_EffectMatrices[effect->matrixIndex]);
        }

        if ((effect->alpha <= 0.0f || effect->scale < 1.0f) && IsRemovedWhenFaded(effect->kind)) {
            effect->kind = EFFECT_KIND_NONE;
            effect->alpha = 0.0f;
        }

        if (effect->kind == EFFECT_KIND_DROP) {
            translation[AXIS_X] = effect->position[AXIS_X];
            translation[AXIS_Y] = effect->position[AXIS_Y];
            translation[AXIS_Z] = effect->position[AXIS_Z];
            rotation[AXIS_X] = effect->tilt;
            rotation[AXIS_Y] = 0;
            rotation[AXIS_Z] = effect->tilt;
            scale[AXIS_X] = effect->scale;
            scale[AXIS_Y] = effect->scale;
            scale[AXIS_Z] = effect->scale;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(matrix, &f_DropMatrices[g_DrawBufferIndex]);
        }
    }
}

#define RIPPLE_IMAGE_SIZE 64
#define ARC_IMAGE_SIZE 64
#define DIVOT_MARK_ALPHA_THRESHOLD 0x1E
#define BILLBOARD_ALPHA_THRESHOLD 0x1F
#define SPARKLE_ALPHA_THRESHOLD 0x3F
#define RISING_CLUMP_TINT GPACK_RGBA5551(32, 248, 32, 0)
#define SMOKE_TINT GPACK_RGBA5551(0, 0, 0, 0)
#define SPARKLE_TINT GPACK_RGBA5551(255, 255, 0, 0)

void DrawBillboardEffect(Gfx **displayListHead, Effect *effect, u16 color);

void DrawEffects(CameraMatrices *matrices, Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 index;
    Effect *effect;
    s32 color;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, DIVOT_MARK_ALPHA_THRESHOLD);
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        if (effect->kind == EFFECT_KIND_DIVOT_MARK) {
            DrawDivotMark(&gfx);
        }
    }

    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
    gDPSetCombineLERP(
        gfx++,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_RippleImage)),
        G_IM_FMT_I,
        RIPPLE_IMAGE_SIZE,
        RIPPLE_IMAGE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP,
        TextureMask(RIPPLE_IMAGE_SIZE),
        TextureMask(RIPPLE_IMAGE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        if (effect->kind == EFFECT_KIND_RIPPLE) {
            DrawRipple(&gfx, effect);
        }
    }

    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_ArcImage)),
        G_IM_FMT_I,
        ARC_IMAGE_SIZE,
        ARC_IMAGE_SIZE,
        0,
        G_TX_MIRROR | G_TX_WRAP,
        G_TX_MIRROR | G_TX_WRAP,
        TextureMask(ARC_IMAGE_SIZE),
        TextureMask(ARC_IMAGE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        if (effect->kind == EFFECT_KIND_SPREADING_CIRCLE) {
            DrawSpreadingCircle(&gfx, effect);
        }
    }

    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        if (effect->kind == EFFECT_KIND_DROP) {
            DrawEffectShadow(&gfx, effect);
            DrawDrop(&gfx, matrices, effect);
        }
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    gDPPipeSync(gfx++);
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        if (effect->kind == EFFECT_KIND_DIVOT) {
            DrawEffectShadow(&gfx, effect);
            DrawDivot(&gfx, effect);
        }
    }

    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_ZB_CLD_SURF, G_RM_ZB_CLD_SURF2);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_DITHER);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, BILLBOARD_ALPHA_THRESHOLD);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_NOISE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        if (effect->kind == EFFECT_KIND_HIGH_PUFF ||
            effect->kind == EFFECT_KIND_LOW_PUFF ||
            effect->kind == EFFECT_KIND_LOW_GRASS) {
            DrawBillboardEffect(&gfx, effect, effect->color);
        }
    }

    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, BILLBOARD_ALPHA_THRESHOLD);
    gDPSetCombineLERP(
        gfx++,
        TEXEL0,
        0,
        PRIMITIVE,
        0,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        TEXEL0,
        0,
        PRIMITIVE,
        0,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        if (effect->kind == EFFECT_KIND_HIGH_ROUGH || effect->kind == EFFECT_KIND_LOW_ROUGH) {
            DrawBillboardEffect(&gfx, effect, effect->color);
        }
    }

    color = RISING_CLUMP_TINT;
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, BILLBOARD_ALPHA_THRESHOLD);
    gDPSetCombineLERP(
        gfx++,
        TEXEL0,
        0,
        PRIMITIVE,
        0,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        TEXEL0,
        0,
        PRIMITIVE,
        0,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        if (effect->kind == EFFECT_KIND_RISING_CLUMP) {
            DrawBillboardEffect(&gfx, effect, color);
        }
    }

    color = SMOKE_TINT;
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, BILLBOARD_ALPHA_THRESHOLD);
    gDPSetCombineLERP(
        gfx++,
        TEXEL0,
        0,
        PRIMITIVE,
        0,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        TEXEL0,
        0,
        PRIMITIVE,
        0,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        if (effect->kind == EFFECT_KIND_SMOKE_PUFF) {
            DrawBillboardEffect(&gfx, effect, color);
        }
    }

    gDPSetCombineLERP(
        gfx++,
        ENVIRONMENT,
        PRIMITIVE,
        TEXEL0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        ENVIRONMENT,
        PRIMITIVE,
        TEXEL0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    color = SPARKLE_TINT;
    gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, SPARKLE_ALPHA_THRESHOLD);
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        if (effect->kind == EFFECT_KIND_SPARKLE) {
            DrawBillboardEffect(&gfx, effect, color);
        }
    }

    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineLERP(
        gfx++,
        0,
        0,
        0,
        TEXEL0,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        TEXEL0,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_DropletImage)));
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &f_Effects[index];
        if (effect->kind == EFFECT_KIND_TEAR) {
            DrawBillboardEffect(&gfx, effect, color);
        }
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

#define BILLBOARD_IMAGE_SIZE 64
#define BILLBOARD_FRAME_SIZE (BILLBOARD_IMAGE_SIZE * BILLBOARD_IMAGE_SIZE)
#define STAR_FRAME_SIZE (BILLBOARD_IMAGE_SIZE * BILLBOARD_IMAGE_SIZE / 2)
#define STAR_LAST_FRAME 2
#define SPARKLE_HIDDEN_FRAME_COUNT 2
#define SPARKLE_STEP_COUNT 7
#define SPECKLE_IMAGE_SIZE 32
#define DROPLET_IMAGE_SIZE 16
#define DROPLET_FRAME_SIZE (DROPLET_IMAGE_SIZE * DROPLET_IMAGE_SIZE / 2)
#define TEAR_SECOND_FRAME_START 6
#define TEAR_THIRD_FRAME_START 20
#define TEAR_MIRROR_S_SHIFT 11
#define PUFF_LIGHTENING 0x40

void DrawBillboardEffect(Gfx **displayListHead, Effect *effect, u16 color) {
    Gfx *gfx = *displayListHead;
    ScreenPoint center;
    s32 halfSize;
    s32 texelStep;
    s32 frameOffset;
    s32 sparkleStep;
    s32 red;
    s32 green;
    s32 blue;

    if (ProjectPoint(
            effect->position[AXIS_X],
            effect->position[AXIS_Y],
            effect->position[AXIS_Z],
            &center
        ) < EFFECT_NEAREST_DEPTH) {
        halfSize = center.scaleX * effect->scale;

        if (effect->alpha == 0.0f) {
            *displayListHead = gfx;
            return;
        }

        if (halfSize >= 2) {
            frameOffset = effect->frame * BILLBOARD_FRAME_SIZE;
            gDPPipeSync(gfx++);
            gDPSetPrimDepth(gfx++, center.depth, 0);
            gDPPipeSync(gfx++);

            texelStep = TexelRunStep((BILLBOARD_IMAGE_SIZE / 2), halfSize);
            if (effect->kind == EFFECT_KIND_RISING_CLUMP) {
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress(ImageBodyAt(f_ClumpImage, frameOffset)),
                    G_IM_FMT_I,
                    G_IM_SIZ_8b,
                    BILLBOARD_IMAGE_SIZE,
                    BILLBOARD_IMAGE_SIZE,
                    0,
                    G_TX_WRAP,
                    G_TX_WRAP,
                    TextureMask(BILLBOARD_IMAGE_SIZE),
                    TextureMask(BILLBOARD_IMAGE_SIZE),
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
            }
            else if (effect->kind == EFFECT_KIND_SPARKLE) {
                s32 sparkleFrames[SPARKLE_STEP_COUNT] = {2, 1, 0, 0, 1, 1, 2};

                sparkleStep = effect->frame - SPARKLE_HIDDEN_FRAME_COUNT;
                if (effect->frame < SPARKLE_HIDDEN_FRAME_COUNT) {
                    *displayListHead = gfx;
                    return;
                }

                if (sparkleStep > SPARKLE_STEP_COUNT - 1) {
                    sparkleStep = SPARKLE_STEP_COUNT - 1;
                }

                frameOffset = sparkleFrames[sparkleStep] * STAR_FRAME_SIZE;
                if (frameOffset > STAR_LAST_FRAME * STAR_FRAME_SIZE) {
                    frameOffset = STAR_LAST_FRAME * STAR_FRAME_SIZE;
                }

                gDPLoadTextureBlock_4b(
                    gfx++,
                    TextureAddress(ImageBodyAt(f_StarImage, frameOffset)),
                    G_IM_FMT_I,
                    BILLBOARD_IMAGE_SIZE,
                    BILLBOARD_IMAGE_SIZE,
                    0,
                    G_TX_WRAP,
                    G_TX_WRAP,
                    TextureMask(BILLBOARD_IMAGE_SIZE),
                    TextureMask(BILLBOARD_IMAGE_SIZE),
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
            }
            else if (effect->kind == EFFECT_KIND_TEAR) {
                if (effect->frame < TEAR_SECOND_FRAME_START) {
                    frameOffset = 0 * DROPLET_FRAME_SIZE;
                }
                else if (effect->frame < TEAR_THIRD_FRAME_START) {
                    frameOffset = 1 * DROPLET_FRAME_SIZE;
                }
                else {
                    frameOffset = 2 * DROPLET_FRAME_SIZE;
                }

                gDPLoadTextureBlock_4b(
                    gfx++,
                    TextureAddress(ImageTexelsAt(f_DropletImage, frameOffset)),
                    G_IM_FMT_CI,
                    DROPLET_IMAGE_SIZE,
                    DROPLET_IMAGE_SIZE,
                    0,
                    G_TX_MIRROR,
                    G_TX_WRAP,
                    TextureMask(DROPLET_IMAGE_SIZE),
                    G_TX_NOMASK,
                    2,
                    2
                );
            }
            else if (effect->kind == EFFECT_KIND_SMOKE_PUFF) {
                gDPLoadTextureBlock_4b(
                    gfx++,
                    TextureAddress(ImageBody(f_SpeckleImage)),
                    G_IM_FMT_I,
                    SPECKLE_IMAGE_SIZE,
                    SPECKLE_IMAGE_SIZE,
                    0,
                    G_TX_WRAP,
                    G_TX_WRAP,
                    6,
                    6,
                    1,
                    1
                );
            }
            else {
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress(ImageBodyAt(f_DebrisImage, frameOffset)),
                    G_IM_FMT_I,
                    G_IM_SIZ_8b,
                    BILLBOARD_IMAGE_SIZE,
                    BILLBOARD_IMAGE_SIZE,
                    0,
                    G_TX_WRAP,
                    G_TX_WRAP,
                    TextureMask(BILLBOARD_IMAGE_SIZE),
                    TextureMask(BILLBOARD_IMAGE_SIZE),
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
            }

            gDPPipeSync(gfx++);
            red = (color >> RGBA16_RED_SHIFT) << RGBA16_CHANNEL_WIDEN_SHIFT;
            green = ((color >> RGBA16_GREEN_SHIFT) & RGBA16_CHANNEL_MASK)
                << RGBA16_CHANNEL_WIDEN_SHIFT;
            blue = ((color >> RGBA16_BLUE_SHIFT) & RGBA16_CHANNEL_MASK)
                << RGBA16_CHANNEL_WIDEN_SHIFT;
            if (effect->kind == EFFECT_KIND_HIGH_PUFF || effect->kind == EFFECT_KIND_LOW_PUFF) {
                red += PUFF_LIGHTENING;
                green += PUFF_LIGHTENING;
                blue += PUFF_LIGHTENING;
                if (red > COLOR_CHANNEL_MAX) {
                    red = COLOR_CHANNEL_MAX;
                }

                if (green > COLOR_CHANNEL_MAX) {
                    green = COLOR_CHANNEL_MAX;
                }

                if (blue > COLOR_CHANNEL_MAX) {
                    blue = COLOR_CHANNEL_MAX;
                }
            }

            gDPSetPrimColor(gfx++, 0, 0, red, green, blue, effect->alpha);
            gDPPipeSync(gfx++);

            if (effect->kind == EFFECT_KIND_TEAR) {
                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels(center.x - halfSize),
                    QuarterPixels(center.y - halfSize),
                    QuarterPixels(center.x + halfSize),
                    QuarterPixels(center.y + halfSize),
                    G_TX_RENDERTILE,
                    (effect->frame & 1) << TEAR_MIRROR_S_SHIFT,
                    0,
                    texelStep,
                    texelStep
                );
            }
            else {
                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels(center.x - halfSize),
                    QuarterPixels(center.y - halfSize),
                    QuarterPixels(center.x + halfSize),
                    QuarterPixels(center.y + halfSize),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    texelStep,
                    texelStep
                );
            }

            gDPPipeSync(gfx++);
        }
    }

    *displayListHead = gfx;
}

#define SHADOW_FARTHEST_DEPTH (-60.0f)
#define SHADOW_ALPHA 63

void DrawEffectShadow(Gfx **displayListHead, Effect *effect) {
    Gfx *gfx = *displayListHead;
    ScreenPoint shadow;
    f32 depth;

    depth = ProjectPoint(
        effect->position[AXIS_X],
        f_SpawnGroundHeight * COURSE_TO_WORLD_SCALE,
        effect->position[AXIS_Z],
        &shadow
    );
    if (depth < EFFECT_NEAREST_DEPTH) {
        if (depth > SHADOW_FARTHEST_DEPTH) {
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
            gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, SHADOW_ALPHA);
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(shadow.x - 1),
                QuarterPixels(shadow.y - 1),
                QuarterPixels(shadow.x + 1),
                QuarterPixels(shadow.y + 1),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }
    }

    *displayListHead = gfx;
}

#define DROP_IMAGE_SIZE 16
#define DROP_GRAY 239

void DrawDrop(Gfx **displayListHead, CameraMatrices *matrices, Effect *unusedEffect UNUSED) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(gfx++, 0, 0, DROP_GRAY, DROP_GRAY, DROP_GRAY, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);

    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_DropImage)),
        G_IM_FMT_I,
        DROP_IMAGE_SIZE,
        DROP_IMAGE_SIZE,
        0,
        G_TX_CLAMP,
        G_TX_CLAMP,
        6,
        6,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_DropMatrices[g_DrawBufferIndex]),
        G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&matrices->billboard),
        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
    );
    gSPVertex(gfx++, f_UprightQuadVertices, QUAD_VERTEX_COUNT, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

#define DIVOT_SPIN_STEP_COUNT 12
#define DIVOT_DEPTH_BIAS 0.75f

void DrawDivot(Gfx **displayListHead, Effect *effect) {
    Gfx *gfx = *displayListHead;
    ScreenPoint center;
    s32 halfSize;
    s32 texelStep;
    s32 spinStep;
    Radians angle;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_DivotImage)));
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    if (ProjectPoint(
            effect->position[AXIS_X],
            effect->position[AXIS_Y],
            effect->position[AXIS_Z],
            &center
        ) < EFFECT_NEAREST_DEPTH) {
        halfSize = center.scaleX * effect->scale;
        if (halfSize > 0) {
            texelStep = TexelRunStep((DIVOT_IMAGE_SIZE / 2), halfSize);

            angle = effect->tilt + (M_PI / DIVOT_SPIN_STEP_COUNT);
            if (angle > FULL_TURN_RADIANS) {
                angle -= FULL_TURN_RADIANS;
            }

            if (angle < 0.0f) {
                angle += FULL_TURN_RADIANS;
            }

            center.depth -= center.depthStep * DIVOT_DEPTH_BIAS;
            gDPSetPrimDepth(gfx++, center.depth, 0);
            spinStep = angle * DIVOT_SPIN_STEP_COUNT / FULL_TURN_RADIANS;
            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(
                    f_DivotImage +
                    ((f_PingPongFrameIndices[spinStep] * DIVOT_FRAME_SIZE) + PALETTED_TEXELS_OFFSET)
                ),
                G_IM_FMT_CI,
                DIVOT_IMAGE_SIZE,
                DIVOT_IMAGE_SIZE,
                0,
                G_TX_CLAMP,
                G_TX_CLAMP,
                6,
                6,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(center.x - halfSize),
                QuarterPixels(center.y - halfSize),
                QuarterPixels(center.x + halfSize),
                QuarterPixels(center.y + halfSize),
                G_TX_RENDERTILE,
                0,
                0,
                texelStep,
                texelStep
            );
            gDPPipeSync(gfx++);
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

#define DIVOT_MARK_FRAME 4

void DrawDivotMark(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_DivotImage)));
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);

    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageTexelsAt(f_DivotImage, DIVOT_MARK_FRAME * DIVOT_FRAME_SIZE)),
        G_IM_FMT_CI,
        DIVOT_IMAGE_SIZE,
        DIVOT_IMAGE_SIZE,
        0,
        G_TX_CLAMP,
        G_TX_CLAMP,
        6,
        6,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gSPMatrix(gfx++, PhysicalAddress(&f_DivotMarkMatrix), G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPVertex(gfx++, f_SmallFlatQuadVertices, QUAD_VERTEX_COUNT, 0);
    gDPPipeSync(gfx++);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawRipple(Gfx **displayListHead, Effect *effect) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, CIRCLE_GRAY, CIRCLE_GRAY, CIRCLE_GRAY, (u32)effect->alpha);
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_EffectMatrices[effect->matrixIndex]),
        G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
    );
    gSPVertex(gfx++, f_RippleVertices, QUAD_VERTEX_COUNT, 0);
    gDPPipeSync(gfx++);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawSpreadingCircle(Gfx **displayListHead, Effect *effect) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, CIRCLE_GRAY, CIRCLE_GRAY, CIRCLE_GRAY, (u32)effect->alpha);
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_EffectMatrices[effect->matrixIndex]),
        G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
    );
    gSPVertex(gfx++, f_SpreadingCircleVertices, QUAD_VERTEX_COUNT, 0);
    gDPPipeSync(gfx++);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void StartPlayerRumble(Player *player) {
    if (player->controllerIndex < MAXCONTROLLERS && g_GameState.replayKind == REPLAY_KIND_NONE) {
        switch (nuContRmbCheck(g_PlayerControllerPorts[player->controllerIndex])) {
            case 0:
                nuContRmbStart(
                    g_PlayerControllerPorts[player->controllerIndex],
                    RUMBLE_FREQUENCY,
                    RUMBLE_FRAME_COUNT
                );
                break;
            case PFS_ERR_NOPACK:
            case PFS_ERR_CONTRFAIL:
                break;
        }
    }
}

void StartRingPassRumble(Player *player) {
    if (player->controllerIndex < MAXCONTROLLERS && g_GameState.replayKind == REPLAY_KIND_NONE) {
        switch (nuContRmbCheck(g_PlayerControllerPorts[player->controllerIndex])) {
            case 0:
                nuContRmbStart(
                    g_PlayerControllerPorts[player->controllerIndex],
                    RUMBLE_FREQUENCY,
                    RUMBLE_FRAME_COUNT
                );
                break;
            case PFS_ERR_NOPACK:
            case PFS_ERR_CONTRFAIL:
                break;
        }
    }
}

#define ASSET_RAIN_SPLASH_IMAGE 0x6BC

void StartWeather(Weather weather) {
    EncodedAsset asset;

    if (weather == WEATHER_RAIN) {
        f_RainSplashImage = AllocMemory(OpenEncodedAsset(ASSET_RAIN_SPLASH_IMAGE, &asset));
        ReadEncodedAsset(f_RainSplashImage, &asset);
        f_Raindrops = AllocMemory(RAINDROP_COUNT * sizeof(Raindrop));
        f_RainSplashes = AllocMemory(RAIN_SPLASH_COUNT * sizeof(RainSplash));
        StartRain();
    }

    f_Weather = weather;
}

void StopWeather(void) {
    if (f_Weather == WEATHER_RAIN) {
        FreeMemory(&f_RainSplashImage);
        FreeMemory(&f_Raindrops);
        FreeMemory(&f_RainSplashes);
    }

    StopSound(RAIN_SOUND_CHANNEL);
    f_Weather = WEATHER_NONE;
}

#define RAIN_CENTER_DISTANCE 150.0f
#define RAIN_SPLASH_SPREAD_STEP_COUNT 500

void PlaceRainCenter(void) {
    s32 viewX;
    s32 viewZ;
    f32 scale;

    viewX = g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] - g_Cameras[MAIN_CAMERA].view.eye[AXIS_X];
    viewZ = g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] - g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z];

    scale = sqrtf((viewX * viewX) + (viewZ * viewZ));
    f_RainCenter[0] = viewX;
    f_RainCenter[1] = viewZ;
    // Skips views shorter than one unit. Comparing the truncated length
    // with 0 as an int instead of a float breaks the match.
    if ((s32)scale != 0.0f) {
        scale = RAIN_CENTER_DISTANCE / scale;
    }
    else {
        osSyncPrintf("Rain Pos Error");
        scale = 0.0f;
    }

    f_RainCenter[0] = (f_RainCenter[0] * scale) + g_Cameras[MAIN_CAMERA].view.eye[AXIS_X];
    f_RainCenter[1] = (f_RainCenter[1] * scale) + g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z];
}

#define RAIN_SPREAD_STEP_COUNT 200
#define RAINDROP_LOWEST_RISE 0x1A400
#define RAIN_TOP_BELOW_EYE 15.0f
#define RAINDROP_LENGTH 90.0f

void ResetRaindrop(Raindrop *drop) {
    f32 fallFrameCount;

    drop->head[AXIS_X] = (((rand() % RAIN_SPREAD_STEP_COUNT) - (RAIN_SPREAD_STEP_COUNT / 2)) *
                             RAIN_SPREAD_STEP * COURSE_TO_WORLD_SCALE) +
        f_RainCenter[0];
    drop->head[AXIS_Y] =
        (-RAINDROP_LOWEST_RISE - (rand() % RAIN_SPREAD_STEP_COUNT * RAIN_SPREAD_STEP)) *
        COURSE_TO_WORLD_SCALE;
    drop->head[AXIS_Z] = (((rand() % RAIN_SPREAD_STEP_COUNT) - (RAIN_SPREAD_STEP_COUNT / 2)) *
                             RAIN_SPREAD_STEP * COURSE_TO_WORLD_SCALE) +
        f_RainCenter[1];
    fallFrameCount = drop->head[AXIS_Y] / f_RainVelocity[AXIS_Y];
    drop->head[AXIS_Y] += g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] + RAIN_TOP_BELOW_EYE;
    drop->head[AXIS_X] += f_RainVelocity[AXIS_X] * fallFrameCount;
    drop->head[AXIS_Z] += f_RainVelocity[AXIS_Z] * fallFrameCount;
    drop->tail[AXIS_X] = drop->head[AXIS_X] + (f_RainDriftX * RAINDROP_LENGTH);
    drop->tail[AXIS_Z] = drop->head[AXIS_Z] + (f_RainDriftZ * RAINDROP_LENGTH);
    drop->tail[AXIS_Y] = drop->head[AXIS_Y] + RAINDROP_LENGTH;
    drop->groundHeight = 0;
    drop->landedFrameCount = RAINDROP_FALLING;
    drop->flags = 0;
    drop->splashColumn = 0;
    drop->tailScale = 1.0f;
}

void ResetRainSplash(RainSplash *splash) {
    splash->position[AXIS_X] =
        (((rand() % RAIN_SPLASH_SPREAD_STEP_COUNT) - (RAIN_SPLASH_SPREAD_STEP_COUNT / 2)) *
            RAIN_SPREAD_STEP * COURSE_TO_WORLD_SCALE) +
        f_RainCenter[0];
    splash->position[AXIS_Z] =
        (((rand() % RAIN_SPLASH_SPREAD_STEP_COUNT) - (RAIN_SPLASH_SPREAD_STEP_COUNT / 2)) *
            RAIN_SPREAD_STEP * COURSE_TO_WORLD_SCALE) +
        f_RainCenter[1];
    splash->position[AXIS_Y] = GetGroundHeight(
                                   splash->position[AXIS_X] * WORLD_TO_COURSE_SCALE,
                                   splash->position[AXIS_Z] * WORLD_TO_COURSE_SCALE
                               ) *
        COURSE_TO_WORLD_SCALE;
    splash->column = 0;
}

void StartRain(void) {
    f32 direction;
    f32 speed;
    s32 index;

    f_GroundCheckDropIndex = 0;
    direction = g_HoleSetup.conditions.windDirection * WIND_DIRECTION_TO_RADIANS;
    speed = g_HoleSetup.conditions.windSpeed / WIND_SPEED_UNIT * RAIN_DRIFT_PER_WIND_SPEED;
    f_RainDriftX = cosf(direction) * speed;
    f_RainDriftZ = sinf(direction) * speed;
    f_RainVelocity[AXIS_Y] = RAIN_FALL_SPEED;
    f_RainVelocity[AXIS_X] = f_RainDriftX * RAIN_FALL_SPEED;
    f_RainVelocity[AXIS_Z] = f_RainDriftZ * RAIN_FALL_SPEED;
    PlaceRainCenter();
    for (index = 0; index != RAINDROP_COUNT; index++) {
        ResetRaindrop(&f_Raindrops[index]);
        f_Raindrops[index].flags = 0;
    }

    for (index = 0; index != RAIN_SPLASH_COUNT; index++) {
        ResetRainSplash(&f_RainSplashes[index]);
        f_RainSplashes[index].ignoredStartValue = 1.0f;
        f_RainSplashes[index].frameCount = (index % 9) - 1;
        f_RainSplashes[index].ignoredFrameValue = 0;
    }
}

#define RAIN_SOUND_STEADY_VOLUME 80.0f
#define RAIN_SOUND_FADE_STEP 0.5f
#define RAIN_CAMERA_JUMP_DISTANCE 15.0f
#define RAINDROP_LANDED_FRAME_COUNT 8
#define RAIN_SPLASH_FRAME_COUNT 8

void UpdateRain(void) {
    Vec3 cameraShift = {0.0f, 0.0f, 0.0f};
    f32 direction;
    f32 speed;
    f32 shiftX;
    f32 shiftZ;
    s32 index;
    Raindrop *drop;
    RainSplash *splash;

    if (g_HoleSetup.conditions.isRaining == TRUE && f_Weather == WEATHER_RAIN) {
        if (g_RainSoundVolume > RAIN_SOUND_STEADY_VOLUME) {
            g_RainSoundVolume -= RAIN_SOUND_FADE_STEP;
        }

        SetSoundVolume(RAIN_SOUND_CHANNEL, g_RainSoundVolume);
        direction = g_HoleSetup.conditions.windDirection * WIND_DIRECTION_TO_RADIANS;
        speed = g_HoleSetup.conditions.windSpeed / WIND_SPEED_UNIT * RAIN_DRIFT_PER_WIND_SPEED;
        f_RainDriftX = cosf(direction) * speed;
        f_RainDriftZ = sinf(direction) * speed;
        f_RainVelocity[AXIS_Y] = RAIN_FALL_SPEED;
        f_RainVelocity[AXIS_X] = f_RainDriftX * RAIN_FALL_SPEED;
        f_RainVelocity[AXIS_Z] = f_RainDriftZ * RAIN_FALL_SPEED;
        PlaceRainCenter();
        shiftX = g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] - f_LastRainEye[AXIS_X];
        shiftZ = g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] - f_LastRainEye[AXIS_Z];
        if (sqrtf((shiftX * shiftX) + (shiftZ * shiftZ)) > RAIN_CAMERA_JUMP_DISTANCE) {
            cameraShift[AXIS_X] += shiftX;
            cameraShift[AXIS_Z] += shiftZ;
        }
        else if (f_LastRainEye[AXIS_Y] < g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y]) {
            cameraShift[AXIS_Y] = g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] - f_LastRainEye[AXIS_Y];
        }

        f_LastRainEye[AXIS_X] = g_Cameras[MAIN_CAMERA].view.eye[AXIS_X];
        f_LastRainEye[AXIS_Y] = g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y];
        f_LastRainEye[AXIS_Z] = g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z];

        f_Raindrops[f_GroundCheckDropIndex].groundHeight =
            GetGroundHeight(
                f_Raindrops[f_GroundCheckDropIndex].head[AXIS_X] * WORLD_TO_COURSE_SCALE,
                f_Raindrops[f_GroundCheckDropIndex].head[AXIS_Z] * WORLD_TO_COURSE_SCALE
            ) *
            COURSE_TO_WORLD_SCALE;
        for (index = 0; index != f_RainSplashCount; index++) {
            splash = &f_RainSplashes[index];
            splash->frameCount++;
            if (splash->frameCount == RAIN_SPLASH_FRAME_COUNT) {
                ResetRainSplash(&f_RainSplashes[index]);
                splash->frameCount = 0;
            }
        }

        for (index = 0; index != f_RaindropCount; index++) {
            drop = &f_Raindrops[index];
            if (drop->head[AXIS_Y] < drop->groundHeight) {
                drop->head[AXIS_X] += f_RainVelocity[AXIS_X];
                drop->head[AXIS_Y] += f_RainVelocity[AXIS_Y];
                drop->head[AXIS_Z] += f_RainVelocity[AXIS_Z];
            }

            if (drop->head[AXIS_Y] > drop->groundHeight) {
                drop->flags |= RAINDROP_HEAD_GROUNDED;
            }

            if (drop->landedFrameCount == RAINDROP_FALLING) {
                if (drop->head[AXIS_Y] < drop->groundHeight) {
                    drop->head[AXIS_X] += cameraShift[AXIS_X];
                    drop->head[AXIS_Y] += cameraShift[AXIS_Y];
                    drop->head[AXIS_Z] += cameraShift[AXIS_Z];
                }

                drop->tail[AXIS_X] += f_RainVelocity[AXIS_X] + cameraShift[AXIS_X];
                drop->tail[AXIS_Y] += f_RainVelocity[AXIS_Y] + cameraShift[AXIS_Y];
                drop->tail[AXIS_Z] += f_RainVelocity[AXIS_Z] + cameraShift[AXIS_Z];
                if (drop->tail[AXIS_Y] > drop->groundHeight) {
                    drop->landedFrameCount = 0;
                    drop->tail[AXIS_X] = drop->head[AXIS_X] +
                        ((drop->groundHeight - drop->head[AXIS_Y]) * f_RainDriftX);
                    drop->tail[AXIS_Y] =
                        drop->head[AXIS_Y] + (drop->groundHeight - drop->head[AXIS_Y]);
                    drop->tail[AXIS_Z] = drop->head[AXIS_Z] +
                        ((drop->groundHeight - drop->head[AXIS_Y]) * f_RainDriftZ);
                }
            }
            else {
                drop->landedFrameCount++;
            }

            if (drop->landedFrameCount == RAINDROP_LANDED_FRAME_COUNT) {
                ResetRaindrop(drop);
            }
        }

        f_GroundCheckDropIndex = (f_GroundCheckDropIndex + 1) % f_RaindropCount;
    }
}

#define RAINDROP_NEAREST_DEPTH (-15.0f)
#define RAIN_SPLASH_NEAREST_DEPTH (-7.5f)
#define RAINDROP_WIDTH 5.0f
#define RAIN_ALPHA_THRESHOLD 1
#define RAIN_SPLASH_IMAGE_WIDTH 32
#define RAIN_SPLASH_IMAGE_HEIGHT 64
#define RAIN_SPLASH_FRAME_SIZE 16
#define RAIN_SPLASH_FRAME_SHIFT 9
#define RAIN_SPLASH_FRAME_DURATION 2
#define RAIN_SPLASH_GRAY 0xBF
#define RAIN_SPLASH_ALPHA 0x9F
#define RAIN_SPLASH_HALF_SIZE_PER_SCALE 8.0f
#define LANDED_DROP_SMALLEST_HALF_SIZE 4.0f
#define RAIN_SPLASH_SMALLEST_HALF_SIZE 1.0f
#define RAIN_SPLASH_LARGEST_HALF_SIZE 16

void DrawRain(Gfx **displayListHead, CameraMatrices *matrices) {
    ScreenPoint point;
    ScreenPoint splashPoint;
    Gfx *gfx = *displayListHead;
    s32 index;

    Raindrop *drop;
    Raindrop *streak;
    Raindrop *landedDrop;
    RainSplash *splash;
    f32 lengthX;
    f32 lengthY;
    f32 widthScale;
    s32 halfSize;
    s32 splashSize;
    s32 splashStep;
    s32 texelStep;

    if (g_HoleSetup.conditions.isRaining == TRUE &&
        f_Weather == WEATHER_RAIN

        && IsCloseUpCameraMode(g_Cameras[MAIN_CAMERA].mode) == FALSE) {
        UpdateRain();
        for (index = 0; index != f_RaindropCount; index++) {
            drop = &f_Raindrops[index];
            if (ProjectPoint(drop->tail[AXIS_X], drop->tail[AXIS_Y], drop->tail[AXIS_Z], &point) <
                RAINDROP_NEAREST_DEPTH) {
                drop->tailScreenX = point.x * QUARTER_PIXELS_PER_PIXEL;
                drop->tailScreenY = point.y * QUARTER_PIXELS_PER_PIXEL;

                drop->tailDepth = point.depth;
                drop->flags &= ~RAINDROP_OFF_SCREEN;
                drop->tailScale = point.scaleX;
                if (!(drop->flags & RAINDROP_HEAD_GROUNDED)) {
                    if (ProjectPointToScreen(
                            drop->head[AXIS_X],
                            drop->head[AXIS_Y],
                            drop->head[AXIS_Z],
                            &point
                        ) < RAINDROP_NEAREST_DEPTH) {
                        drop->headScreenX = point.x * QUARTER_PIXELS_PER_PIXEL;
                        drop->headScreenY = point.y * QUARTER_PIXELS_PER_PIXEL;
                    }
                    else {
                        drop->flags |= RAINDROP_OFF_SCREEN;
                    }
                }
            }
            else {
                drop->flags |= RAINDROP_OFF_SCREEN;
            }
        }

        guMtxIdent(&f_RainModelViewMatrix);
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gSPLoadGeometryMode(gfx++, 0);
        gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_PERSP);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
        gDPPipeSync(gfx++);
        gDPSetDepthSource(gfx++, G_ZS_PRIM);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_ZB_CLD_SURF, G_RM_ZB_CLD_SURF2);
        gDPPipeSync(gfx++);
        gSPMatrix(
            gfx++,
            PhysicalAddress(&g_ScreenOrthoMatrix),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_RainModelViewMatrix),
            G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW
        );
        gSPVertex(gfx++, f_RainStreakVertices, ARRAY_COUNT(f_RainStreakVertices), 0);
        for (index = 0; index != f_RaindropCount; index++) {
            streak = &f_Raindrops[index];

            if (streak->flags == 0) {
                lengthX = streak->headScreenX - streak->tailScreenX;
                lengthY = streak->headScreenY - streak->tailScreenY;
                if (lengthX == 0.0f && lengthY == 0.0f) {
                }
                else {
                    widthScale = RAINDROP_WIDTH / sqrtf((lengthX * lengthX) + (lengthY * lengthY));
                    gDPPipeSync(gfx++);
                    gDPSetPrimDepth(gfx++, streak->tailDepth, 0);
                    gDPPipeSync(gfx++);

                    lengthY *= widthScale;
                    lengthX *= widthScale;
                    gSPModifyVertex(
                        gfx++,
                        0,
                        G_MWO_POINT_XYSCREEN,
                        ScreenPointWord(streak->headScreenX + (s32)lengthY, streak->headScreenY)
                    );
                    gSPModifyVertex(
                        gfx++,
                        1,
                        G_MWO_POINT_XYSCREEN,
                        ScreenPointWord(streak->headScreenX, streak->headScreenY + (s32)lengthX)
                    );
                    gSPModifyVertex(
                        gfx++,
                        2,
                        G_MWO_POINT_XYSCREEN,
                        ScreenPointWord(streak->tailScreenX + (s32)lengthY, streak->tailScreenY)
                    );
                    gSPModifyVertex(
                        gfx++,
                        3,
                        G_MWO_POINT_XYSCREEN,
                        ScreenPointWord(streak->tailScreenX, streak->tailScreenY + (s32)lengthX)
                    );
                    gSP2Triangles(gfx++, 1, 0, 2, 0, 1, 2, 3, 0);
                }
            }
        }

        gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_NONE);
        gDPSetCombineLERP(
            gfx++,
            0,
            0,
            0,
            PRIMITIVE,
            TEXEL0,
            0,
            PRIMITIVE,
            0,
            0,
            0,
            0,
            PRIMITIVE,
            TEXEL0,
            0,
            PRIMITIVE,
            0
        );
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
        gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, RAIN_ALPHA_THRESHOLD);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(ImageBody(f_RainSplashImage)),
            G_IM_FMT_I,
            RAIN_SPLASH_IMAGE_WIDTH,
            RAIN_SPLASH_IMAGE_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            6,
            6,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPSetPrimColor(
            gfx++,
            0,
            0,
            RAIN_SPLASH_GRAY,
            RAIN_SPLASH_GRAY,
            RAIN_SPLASH_GRAY,
            RAIN_SPLASH_ALPHA
        );
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        for (index = 0; index != f_RaindropCount; index++) {
            landedDrop = &f_Raindrops[index];
            if (landedDrop->landedFrameCount != RAINDROP_FALLING &&
                !(landedDrop->flags & RAINDROP_OFF_SCREEN)) {
                gDPPipeSync(gfx++);
                gDPSetPrimDepth(gfx++, landedDrop->tailDepth, 0);
                gDPPipeSync(gfx++);
                halfSize = landedDrop->tailScale * RAIN_SPLASH_HALF_SIZE_PER_SCALE;
                if (halfSize >= LANDED_DROP_SMALLEST_HALF_SIZE) {
                    if (halfSize > (f32)RAIN_SPLASH_LARGEST_HALF_SIZE) {
                        halfSize = RAIN_SPLASH_LARGEST_HALF_SIZE;
                    }

                    texelStep = TexelRunStep((RAIN_SPLASH_FRAME_SIZE * 2), halfSize);
                    gSPScisTextureRectangle(
                        gfx++,
                        landedDrop->tailScreenX - halfSize,
                        landedDrop->tailScreenY - halfSize,
                        landedDrop->tailScreenX + halfSize,
                        landedDrop->tailScreenY + halfSize,
                        G_TX_RENDERTILE,
                        landedDrop->splashColumn << RAIN_SPLASH_FRAME_SHIFT,
                        (landedDrop->landedFrameCount / RAIN_SPLASH_FRAME_DURATION)
                            << RAIN_SPLASH_FRAME_SHIFT,
                        texelStep,
                        texelStep
                    );
                }
            }
        }

        for (index = 0; index != f_RainSplashCount; index++) {
            splash = &f_RainSplashes[index];
            if (ProjectPointToScreen(
                    splash->position[AXIS_X],
                    splash->position[AXIS_Y],
                    splash->position[AXIS_Z],
                    &splashPoint
                ) < RAIN_SPLASH_NEAREST_DEPTH) {
                splashSize = splashPoint.scaleX * RAIN_SPLASH_HALF_SIZE_PER_SCALE;
                if (splashSize >= RAIN_SPLASH_SMALLEST_HALF_SIZE) {
                    if (splashSize > (f32)RAIN_SPLASH_LARGEST_HALF_SIZE) {
                        splashSize = RAIN_SPLASH_LARGEST_HALF_SIZE;
                    }

                    splashStep = TexelRunStep((RAIN_SPLASH_FRAME_SIZE * 2), splashSize);
                    gSPScisTextureRectangle(
                        gfx++,
                        (splashPoint.x * QUARTER_PIXELS_PER_PIXEL) - splashSize,
                        (splashPoint.y * QUARTER_PIXELS_PER_PIXEL) - splashSize,
                        (splashPoint.x * QUARTER_PIXELS_PER_PIXEL) + splashSize,
                        (splashPoint.y * QUARTER_PIXELS_PER_PIXEL) + splashSize,
                        G_TX_RENDERTILE,
                        splash->column << RAIN_SPLASH_FRAME_SHIFT,
                        (splash->frameCount / RAIN_SPLASH_FRAME_DURATION)
                            << RAIN_SPLASH_FRAME_SHIFT,
                        splashStep,
                        splashStep
                    );
                }
            }
        }

        gDPPipeSync(gfx++);
        gSPMatrix(
            gfx++,
            PhysicalAddress(&matrices->projection),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
        );
        gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
        gDPPipeSync(gfx++);
        gDPSetDepthSource(gfx++, G_ZS_PIXEL);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gDPPipeSync(gfx++);
        *displayListHead = gfx;
    }
}

void ShowBirdieBadgeCount(s32 badgeCount) {
    EncodedAsset asset;

    f_ShownBadgeCount = badgeCount;
    f_IsBadgeCountShown = TRUE;
    f_BadgeCountFrame = 0;
    OpenEncodedAsset(ASSET_SPINNING_BADGE, &asset);
    ReadEncodedAsset(f_CloudImage, &asset);
    OpenEncodedAsset(ASSET_LARGE_DIGITS, &asset);
    ReadEncodedAsset(f_ClumpImage, &asset);
}

#define BADGE_LEFT 216
#define BADGE_RESTING_TOP 20
#define BADGE_COUNT_LEFT 258
#define BADGE_COUNT_TOP 24
#define BADGE_COUNT_SHOWN_FRAME_COUNT 60
#define BADGE_COUNT_RISE_FRAME 40
#define BADGE_COUNT_ADD_FRAME 15
#define BADGE_COUNT_SOUND 0x6B
#define BADGE_COUNT_SOUND_VOLUME 100
#define BADGE_SLIDE_PER_FRAME 4
#define BADGE_DROP_START_OFFSET (-24)
#define BADGE_RISEN_OFFSET (-100)
#define COUNT_GLYPHS_HEIGHT 256

void DrawBirdieBadgeCount(Gfx **displayListHead) {
    Gfx *gfx;
    s32 spinFrame;
    s32 offsetY;

    gfx = *displayListHead;
    if (f_IsBadgeCountShown) {
        gSPDisplayList(gfx++, g_DecalTextureSetup);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_POINT);
        spinFrame = f_BadgeCountFrame / 2 % SPINNING_BADGE_FRAME_COUNT;
        if (f_BadgeCountFrame < BADGE_COUNT_RISE_FRAME) {
            offsetY = (f_BadgeCountFrame * BADGE_SLIDE_PER_FRAME) + BADGE_DROP_START_OFFSET;
            if (offsetY > 0) {
                offsetY = 0;
            }
        }
        else {
            offsetY = (BADGE_COUNT_RISE_FRAME - f_BadgeCountFrame) * BADGE_SLIDE_PER_FRAME;
            if (offsetY < BADGE_RISEN_OFFSET) {
                offsetY = BADGE_RISEN_OFFSET;
            }
        }

        LoadSmallPalette(&gfx, 0, ImagePalette(f_CloudImage));

        gDPLoadTextureTile_4b(
            gfx++,
            TextureAddress(ImageTexelsAt(f_CloudImage, spinFrame * SPINNING_BADGE_FRAME_SIZE)),
            G_IM_FMT_CI,
            SPINNING_BADGE_SIZE,
            SPINNING_BADGE_SIZE,
            0,
            0,
            SPINNING_BADGE_SIZE - 1,
            SPINNING_BADGE_SIZE - 1,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(BADGE_LEFT),
            QuarterPixels(offsetY + BADGE_RESTING_TOP),
            QuarterPixels(BADGE_LEFT + SPINNING_BADGE_SIZE),
            QuarterPixels(offsetY + BADGE_RESTING_TOP + SPINNING_BADGE_SIZE),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        LoadSmallPalette(&gfx, 0, ImagePalette(f_ClumpImage));
        LoadNibbleTextureBlock(
            &gfx,
            ImageTexels(f_ClumpImage),
            G_IM_FMT_CI,
            16,
            COUNT_GLYPHS_HEIGHT,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        DrawDigitText(&gfx, BADGE_LEFT + SPINNING_BADGE_SIZE, offsetY + BADGE_COUNT_TOP, "*");
        sprintf(g_PrintBuffer, "%d", (int)f_ShownBadgeCount);
        DrawDigitText(&gfx, BADGE_COUNT_LEFT, offsetY + BADGE_COUNT_TOP, g_PrintBuffer);
        f_BadgeCountFrame++;
        if (f_BadgeCountFrame == BADGE_COUNT_ADD_FRAME) {
            f_ShownBadgeCount++;
            PlaySound(BADGE_COUNT_SOUND, g_NextSoundChannel, BADGE_COUNT_SOUND_VOLUME);
        }

        if (f_BadgeCountFrame == BADGE_COUNT_SHOWN_FRAME_COUNT) {
            f_BadgeCountFrame = 0;
            f_IsBadgeCountShown = FALSE;
        }

        *displayListHead = gfx;
    }
}
