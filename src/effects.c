#include "common.h"
#include "asset.h"
#include "ball_simulation.h"
#include "camera.h"
#include "character_slot.h"
#include "club.h"
#include "controller.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "functions.h"
#include "heap.h"
#include "libc.h"
#include "math_util.h"
#include "save.h"
#include "sound.h"
#include "surface.h"
#include "terrain.h"
#include "variables.h"
#include <lib/libnusys/nusys.h>

/* The particle effects drawn in the scene, such as the sparkles of a passed ring */
typedef struct Effect {
    /* 0x00 */ Vec3 position; /* In world units */
    /* 0x0C */ Vec3 velocity; /* In world units a frame */
    /* 0x18 */ u8 unk18[0xC];
    /* 0x24 */ f32 unk24; /* An angle about x and z */
    /* 0x28 */ f32 alpha;
    /* 0x2C */ f32 alphaStep;
    /* 0x30 */ f32 scale;
    /* 0x34 */ f32 scaleStep;
    /* 0x38 */ s16 unk38;
    /* 0x3A */ s8 kind;
    /* 0x3B */ s8 unk3B;
    /* 0x3C */ s8 matrixIndex; /* Of the effects' transform matrices */
} Effect; /* size = 0x40 */

#define EFFECT_COUNT 40

#define RAINDROP_COUNT 48
#define RAIN_SPLASH_COUNT 32

typedef struct Raindrop {
    /* 0x00 */ Vec3 head;
    /* 0x0C */ Vec3 tail;
    /* 0x18 */ f32 unk18;
    /* 0x1C */ f32 unk1C;
    /* 0x20 */ s16 unk20[5];
    /* 0x2A */ s16 unk2A;
    /* 0x2C */ s8 unk2C;
    /* 0x2D */ u8 unk2D;
} Raindrop; /* size = 0x30 */

typedef struct RainSplash {
    /* 0x00 */ Vec3 position;
    /* 0x0C */ f32 unkC;
    /* 0x10 */ u8 unk10[6];
    /* 0x16 */ s8 unk16;
    /* 0x17 */ s8 unk17;
    /* 0x18 */ s8 unk18;
} RainSplash; /* size = 0x1C */

/*
 * Set up for each shot's debris. Keeping the club kind in a struct is there to match, as it keeps
 * its loads from moving above the effects' stores.
 */
typedef struct ShotDebris {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ f32 unk8;
    /* 0x0C */ f32 unkC;
    /* 0x10 */ s32 clubKind;
    /* 0x14 */ s32 unk14;
} ShotDebris; /* size = 0x18 */

#define EFFECT_KIND_NONE (-1)
#define EFFECT_KIND_UNK0 0
#define EFFECT_KIND_UNK1 1
#define EFFECT_KIND_UNK2 2
#define EFFECT_KIND_UNK3 3
#define EFFECT_KIND_UNK4 4
#define EFFECT_KIND_DROP 5 /* Drawn with sDropImage */
#define EFFECT_KIND_DIVOT 6 /* Drawn with sDivotImage */
#define EFFECT_KIND_DIVOT_MARK 7 /* Drawn with sDivotImage's last frame */
#define EFFECT_KIND_SPARKLE 8
#define EFFECT_KIND_RIPPLE 0xC
#define EFFECT_KIND_UNKE 0xE
#define EFFECT_KIND_SPREADING_CIRCLE 0x13 /* Drawn with sArcImage */
#define EFFECT_KIND_RISING_CLUMP 0x10 /* Drawn with sClumpImage */
#define EFFECT_KIND_TEAR 0x14

/* The debris that the ball's lie gives, chosen by its surface kind */
#define DEBRIS_KIND_NONE 0
#define DEBRIS_KIND_TEE_UP 1
#define DEBRIS_KIND_GRASS 2 /* Fairway, tee ground and semi-rough */
#define DEBRIS_KIND_DUST 3 /* Bunker and bare ground */
#define DEBRIS_KIND_ROUGH 4
#define DEBRIS_KIND_BURIED 6 /* Bunker lies with the ball sunk in the sand */

#define WEATHER_NONE (-1)
#define WEATHER_RAIN 2

/* Each holds an image asset: a header, then texels */
extern u8 *sRippleImage;
extern u8 *sArcImage;
extern u8 *sDropImage;
extern u8 *sDivotImage;
extern u8 *sPlumeImage;
extern u8 *sCloudImage;
extern u8 *sClumpImage;
extern u8 *sDebrisImage;
extern u8 *sSpeckleImage;
extern u8 *sSplashImage;
extern u8 *sStarImage;
extern u8 *sDropletImage;
extern Effect sEffects[EFFECT_COUNT];
extern Mtx sEffectMatrices[EFFECT_COUNT];
extern Mtx D_801B9418[];
extern Mtx sDivotMarkMatrix;
extern ShotDebris sShotDebris;
extern s32 sSpawnGroundHeight;
extern s8 D_801B60B9;
extern u8 *sRainSplashImage;
extern Raindrop *sRaindrops;
extern RainSplash *sRainSplashes;
/* The x and z that the rain falls around, RAIN_CENTER_DISTANCE ahead of the camera */
extern s32 sRainCenter[2];
/* How far the wind blows the rain sideways for each unit it falls */
extern f32 sRainDriftX;
extern f32 sRainDriftZ;
extern Vec3 sRainVelocity; /* Per frame */
extern s32 D_80104AC0;

s32 sDebrisKind = 0;
s32 sNextMatrixIndex = 0;
Vtx sUprightQuadVertices[] = {
    { { { -1, -1, 0 }, 0, { 0, 0 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { -1, 1, 0 }, 0, { 0, 0x400 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { 1, 1, 0 }, 0, { 0x400, 0x400 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { 1, -1, 0 }, 0, { 0x400, 0 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
};
Vtx sSmallFlatQuadVertices[] = {
    { { { -3, 0, -3 }, 0, { 0, 0 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { -3, 0, 3 }, 0, { 0x7FF, 0 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { 3, 0, 3 }, 0, { 0x7FF, 0x7FF }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { 3, 0, -3 }, 0, { 0, 0x7FF }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
};
Vtx sRippleVertices[] = {
    { { { -7, 0, -7 }, 0, { 0, 0 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { -7, 0, 7 }, 0, { 0xFFF, 0 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { 7, 0, 7 }, 0, { 0xFFF, 0xFFF }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { 7, 0, -7 }, 0, { 0, 0xFFF }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
};
Vtx sSpreadingCircleVertices[] = {
    { { { -7, 0, -7 }, 0, { 0, 0 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { -7, 0, 7 }, 0, { 0x1FFF, 0 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { 7, 0, 7 }, 0, { 0x1FFF, 0x1FFF }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { 7, 0, -7 }, 0, { 0, 0x1FFF }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
};
s32 sAreEffectImagesLoaded = FALSE;
f32 sUnreferencedValues[] = { 1, 2, 3, 10, 11, 12, 13, 14, 20, 21, 22, 25, 30, 1 };
u8 sPingPongFrameIndices[] = { 0, 1, 2, 3, 2, 1, 0, 1, 2, 3, 2, 1 };
s32 sRaindropCount = RAINDROP_COUNT;
s32 sRainSplashCount = RAIN_SPLASH_COUNT;
Vtx D_800C5408[] = {
    { { { 0, 0, -100 }, 0, { 0, 0 }, { 0xAF, 0xAF, 0xBF, 0x1F } } },
    { { { 0, -1, -100 }, 0, { 0, 0 }, { 0xAF, 0xAF, 0xBF, 0x1F } } },
    { { { 48, -1, -100 }, 0, { 0, 0 }, { 0x90, 0xA0, 0xA0, 0x5F } } },
    { { { 48, 0, -100 }, 0, { 0, 0 }, { 0x90, 0xA0, 0xA0, 0x5F } } },
};
s32 sWeather = WEATHER_NONE; /* A WEATHER_* value */
f32 D_800C544C = 0.0f;
f32 D_800C5450 = 0.0f;
f32 D_800C5454 = 0.0f;
s32 sShownBadgeCount = 0;
s32 sIsBadgeCountShown = FALSE;
s32 sBadgeCountFrame = 0;
void ResetRaindrop(Raindrop *drop);
void StartRain(void);

#define RIPPLE_IMAGE_ASSET 0x6B0
#define DROP_IMAGE_ASSET 0x6B2
#define GRASS_IMAGE_ASSET 0x6B6 /* Blades of grass */
#define CLOUD_IMAGE_ASSET 0x6B5
#define CLUMP_IMAGE_ASSET 0x6B8
#define DIVOT_IMAGE_ASSET 0x6C0 /* A flying divot, then the mark it leaves */
#define PLUME_IMAGE_ASSET 0x6C1
#define SPLASH_IMAGE_ASSET 0x6C2
#define STAR_IMAGE_ASSET 0x69E
#define SPECKLE_IMAGE_ASSET 0x71D
#define ARC_IMAGE_ASSET 0x8EC
#define DROPLET_IMAGE_ASSET 0x93B

/* Frees the images that LoadEffectImages loaded */
void FreeEffectImages(void) {
    if (sAreEffectImagesLoaded) {
        FreeMemory(&sRippleImage);
        FreeMemory(&sArcImage);
        FreeMemory(&sDropImage);
        FreeMemory(&sDivotImage);
        FreeMemory(&sPlumeImage);
        FreeMemory(&sCloudImage);
        FreeMemory(&sClumpImage);
        FreeMemory(&sDebrisImage);
        FreeMemory(&sSpeckleImage);
        FreeMemory(&sSplashImage);
        FreeMemory(&sStarImage);
        FreeMemory(&sDropletImage);
        sAreEffectImagesLoaded = FALSE;
    }
}

/* Loads the effects' images; the debris image starts as a copy of the cloud */
void LoadEffectImages(void) {
    EncodedAsset asset;

    FreeEffectImages();
    if (!sAreEffectImagesLoaded) {
        sAreEffectImagesLoaded = TRUE;
        sDebrisImage = AllocMemory(OpenEncodedAsset(CLOUD_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sDebrisImage, &asset);
        sCloudImage = AllocMemory(OpenEncodedAsset(CLOUD_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sCloudImage, &asset);
        sClumpImage = AllocMemory(OpenEncodedAsset(CLUMP_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sClumpImage, &asset);
        sRippleImage = AllocMemory(OpenEncodedAsset(RIPPLE_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sRippleImage, &asset);
        sArcImage = AllocMemory(OpenEncodedAsset(ARC_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sArcImage, &asset);
        sDropImage = AllocMemory(OpenEncodedAsset(DROP_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sDropImage, &asset);
        sDivotImage = AllocMemory(OpenEncodedAsset(DIVOT_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sDivotImage, &asset);
        sPlumeImage = AllocMemory(OpenEncodedAsset(PLUME_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sPlumeImage, &asset);
        sSpeckleImage = AllocMemory(OpenEncodedAsset(SPECKLE_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sSpeckleImage, &asset);
        sSplashImage = AllocMemory(OpenEncodedAsset(SPLASH_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sSplashImage, &asset);
        sStarImage = AllocMemory(OpenEncodedAsset(STAR_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sStarImage, &asset);
        sDropletImage = AllocMemory(OpenEncodedAsset(DROPLET_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sDropletImage, &asset);
    }
}

/*
 * Reloads five of the effect images, clears the effects, and loads the debris image that the
 * ball's lie calls for.
 */
void PrepareShotEffects(BallSimulation *simulation) {
    EncodedAsset asset;
    s32 index;
    Effect *effect;

    OpenEncodedAsset(CLOUD_IMAGE_ASSET, &asset);
    ReadEncodedAsset(sCloudImage, &asset);
    OpenEncodedAsset(CLUMP_IMAGE_ASSET, &asset);
    ReadEncodedAsset(sClumpImage, &asset);
    OpenEncodedAsset(RIPPLE_IMAGE_ASSET, &asset);
    ReadEncodedAsset(sRippleImage, &asset);
    OpenEncodedAsset(DROP_IMAGE_ASSET, &asset);
    ReadEncodedAsset(sDropImage, &asset);
    OpenEncodedAsset(DIVOT_IMAGE_ASSET, &asset);
    ReadEncodedAsset(sDivotImage, &asset);
    /* Clearing the effects here instead of calling ClearEffects is there to match */
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &sEffects[index];
        effect->kind = EFFECT_KIND_NONE;
    }
    sSpawnGroundHeight =
        (f32)GetGroundHeight(g_CurrentShot.ballPosition[AXIS_X], g_CurrentShot.ballPosition[AXIS_Z]);
    if (simulation->surface == SURFACE_TEE_UP) {
        sDebrisKind = DEBRIS_KIND_TEE_UP;
        OpenEncodedAsset(GRASS_IMAGE_ASSET, &asset);
        ReadEncodedAsset(sDebrisImage, &asset);
    } else {
        switch (GetSurfaceInfo(g_CurrentShot.surface)->kind) {
            case SURFACE_KIND_FAIRWAY:
            case SURFACE_KIND_FAIRWAY_FAIR_LIE:
            case SURFACE_KIND_FAIRWAY_POOR_LIE:
            case SURFACE_KIND_TEE_GROUND:
            case SURFACE_KIND_SEMI_ROUGH:
                sDebrisKind = DEBRIS_KIND_GRASS;
                OpenEncodedAsset(GRASS_IMAGE_ASSET, &asset);
                ReadEncodedAsset(sDebrisImage, &asset);
                break;
            case SURFACE_KIND_ROUGH:
            case SURFACE_KIND_ROUGH_POOR_LIE:
            case 5:
            case SURFACE_KIND_DEEP_ROUGH:
                sDebrisKind = DEBRIS_KIND_ROUGH;
                OpenEncodedAsset(CLUMP_IMAGE_ASSET, &asset);
                ReadEncodedAsset(sDebrisImage, &asset);
                break;
            case SURFACE_KIND_BUNKER:
            case SURFACE_KIND_BARE_GROUND:
                sDebrisKind = DEBRIS_KIND_DUST;
                OpenEncodedAsset(CLOUD_IMAGE_ASSET, &asset);
                ReadEncodedAsset(sDebrisImage, &asset);
                break;
            case SURFACE_KIND_BUNKER_FAIR_LIE:
            case SURFACE_KIND_BUNKER_POOR_LIE:
            case SURFACE_KIND_BUNKER_UNK9:
                sDebrisKind = DEBRIS_KIND_BURIED;
                OpenEncodedAsset(CLOUD_IMAGE_ASSET, &asset);
                ReadEncodedAsset(sDebrisImage, &asset);
                break;
            default:
                sDebrisKind = DEBRIS_KIND_NONE;
                break;
        }
    }
    func_80216B10_ovl_00();
}

/* Frees every effect */
void ClearEffects(void) {
    s32 index;
    Effect *effect;

    /* Testing index with != and naming the effect are there to match */
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &sEffects[index];
        effect->kind = EFFECT_KIND_NONE;
    }
}

/* World units above the point that a spreading circle starts */
#define SPREADING_CIRCLE_RISE 1.5f
/* Effects spawned around a point are offset by up to half this on each axis, in course units */
#define POINT_SPREAD 3072
/* Sparkles start falling at this many world units a frame */
#define SPARKLE_FALL_SPEED (2.62f * COURSE_TO_WORLD_SCALE)

/* Starts a circle spreading from a character's point; Bowser's bogey reaction spawns them */
void SpawnSpreadingCircle(u32 slotIndex, s32 pointId) {
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 matrix;
    Vec3 point;
    s32 index;
    Effect *effect;

    if (GetCharacterPointPosition(slotIndex, pointId, point) == 0) {
        /* The (f32) conversion is there to match */
        sSpawnGroundHeight = (f32)GetGroundHeight(point[AXIS_X] * WORLD_TO_COURSE_SCALE,
                                                  point[AXIS_Z] * WORLD_TO_COURSE_SCALE);
        for (index = 0; index != EFFECT_COUNT; index++) {
            effect = &sEffects[index];
            if (effect->kind == EFFECT_KIND_NONE) {
                effect->position[AXIS_X] = point[AXIS_X];
                effect->position[AXIS_Y] = point[AXIS_Y] - SPREADING_CIRCLE_RISE;
                effect->position[AXIS_Z] = point[AXIS_Z];
                effect->kind = EFFECT_KIND_SPREADING_CIRCLE;
                effect->unk3B = 0;
                effect->velocity[AXIS_X] = 0;
                effect->velocity[AXIS_Y] = 0;
                effect->velocity[AXIS_Z] = 0;
                effect->alpha = 127.0f;
                effect->alphaStep = -8.0f;
                effect->scale = 0.2f;
                effect->scaleStep = 0.8f;
                effect->matrixIndex = sNextMatrixIndex;
                translation[AXIS_X] = effect->position[AXIS_X];
                translation[AXIS_Y] = effect->position[AXIS_Y];
                translation[AXIS_Z] = effect->position[AXIS_Z];
                rotation[AXIS_X] = 0.0f;
                rotation[AXIS_Y] = 0;
                rotation[AXIS_Z] = 0;
                scale[AXIS_X] = effect->scale;
                scale[AXIS_Y] = effect->scale;
                /* Updating sNextMatrixIndex here is there to match */
                sNextMatrixIndex = (sNextMatrixIndex + 1) % EFFECT_COUNT;
                scale[AXIS_Z] = effect->scale;
                BuildTransformMatrix(matrix, rotation, translation, scale);
                ConvertMatrixToFixed(matrix, &sEffectMatrices[effect->matrixIndex]);
                return;
            }
        }
    }
}

/* Tears are offset by a fifth as much as other effects around a point */
#define TEAR_OFFSET_SCALE (COURSE_TO_WORLD_SCALE / 5)
/* Tears fly out at between 0.4 and 1.4 times this many course units a frame */
#define TEAR_SPEED_RANGE 1536
/* Tears from this point get unk3B set */
#define TEAR_UNK3B_POINT 3

/*
 * Throws a tear from a character's point toward heading, and up; Baby
 * Mario's bogey reaction throws them from points 3 and 4
 */
void SpawnTear(u32 slotIndex, s32 pointId, f32 heading) {
    Vec3 translation; /* Unused; it is there to match */
    Vec3 rotation;    /* Unused; it is there to match */
    Vec3 scale;       /* Unused; it is there to match */
    Mat4 matrix;      /* Unused; it is there to match */
    Vec3 point;
    s32 index;
    Effect *effect;

    if (GetCharacterPointPosition(slotIndex, pointId, point) == 0) {
        /* The (f32) conversion is there to match */
        sSpawnGroundHeight = (f32)GetGroundHeight(point[AXIS_X] * WORLD_TO_COURSE_SCALE,
                                                  point[AXIS_Z] * WORLD_TO_COURSE_SCALE);
        for (index = 0; index != EFFECT_COUNT; index++) {
            effect = &sEffects[index];
            if (effect->kind == EFFECT_KIND_NONE) {
                effect->position[AXIS_X] =
                    point[AXIS_X] + (guRandom() % POINT_SPREAD - POINT_SPREAD / 2) * TEAR_OFFSET_SCALE;
                effect->position[AXIS_Y] =
                    point[AXIS_Y] + (guRandom() % POINT_SPREAD - POINT_SPREAD / 2) * TEAR_OFFSET_SCALE;
                effect->position[AXIS_Z] =
                    point[AXIS_Z] + (guRandom() % POINT_SPREAD - POINT_SPREAD / 2) * TEAR_OFFSET_SCALE;
                /* Overwritten at once; the original has it */
                effect->velocity[AXIS_X] = 0;
                effect->velocity[AXIS_Y] = SPARKLE_FALL_SPEED;
                effect->velocity[AXIS_X] =
                    cosf(heading) * (guRandom() % TEAR_SPEED_RANGE + TEAR_SPEED_RANGE * 0.4) / 1024.0;
                effect->velocity[AXIS_Y] =
                    (-TEAR_SPEED_RANGE * 0.7 - guRandom() % TEAR_SPEED_RANGE * 0.3f) / 1024.0;
                effect->velocity[AXIS_Z] =
                    sinf(heading) * (guRandom() % TEAR_SPEED_RANGE + TEAR_SPEED_RANGE * 0.4) / 1024.0;
                effect->position[AXIS_X] += effect->velocity[AXIS_X];
                effect->position[AXIS_Y] += effect->velocity[AXIS_Y];
                effect->position[AXIS_Z] += effect->velocity[AXIS_Z];
                effect->unk38 = 0;
                effect->alpha = 159.0f;
                effect->alphaStep = -1.0f;
                effect->scale = 1.3f;
                effect->scaleStep = -0.04f;
                effect->kind = EFFECT_KIND_TEAR;
                if (pointId == TEAR_UNK3B_POINT) {
                    effect->unk3B = 1;
                } else {
                    effect->unk3B = 0;
                }
                return;
            }
        }
    }
}

/* Starts a sparkle near a character's point; eagle reactions spawn them */
void SpawnSparkleNearPoint(u32 slotIndex, s32 pointId) {
    Vec3 translation; /* Unused; it is there to match */
    Vec3 rotation;    /* Unused; it is there to match */
    Vec3 scale;       /* Unused; it is there to match */
    Mat4 matrix;      /* Unused; it is there to match */
    Vec3 point;
    s32 index;
    Effect *effect;

    if (GetCharacterPointPosition(slotIndex, pointId, point) == 0) {
        /* The (f32) conversion is there to match */
        sSpawnGroundHeight = (f32)GetGroundHeight(point[AXIS_X] * WORLD_TO_COURSE_SCALE,
                                                  point[AXIS_Z] * WORLD_TO_COURSE_SCALE);
        for (index = 0; index != EFFECT_COUNT; index++) {
            effect = &sEffects[index];
            if (effect->kind == EFFECT_KIND_NONE) {
                effect->position[AXIS_X] =
                    point[AXIS_X] + (guRandom() % POINT_SPREAD - POINT_SPREAD / 2) * COURSE_TO_WORLD_SCALE;
                effect->position[AXIS_Y] =
                    point[AXIS_Y] + (guRandom() % POINT_SPREAD - POINT_SPREAD / 2) * COURSE_TO_WORLD_SCALE;
                effect->position[AXIS_Z] =
                    point[AXIS_Z] + (guRandom() % POINT_SPREAD - POINT_SPREAD / 2) * COURSE_TO_WORLD_SCALE;
                effect->velocity[AXIS_X] = 0;
                effect->velocity[AXIS_Y] = SPARKLE_FALL_SPEED;
                effect->velocity[AXIS_Z] = 0;
                effect->unk38 = 0;
                effect->alpha = 255.0f;
                effect->alphaStep = -4.0f;
                effect->scale = 6.0f;
                effect->scaleStep = -0.2f;
                effect->kind = EFFECT_KIND_SPARKLE;
                effect->unk3B = 0;
                return;
            }
        }
    }
}

/* Starts a ripple spreading at position */
void SpawnRipple(CoursePoint position) {
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 matrix;
    s32 index;
    Effect *effect;

    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &sEffects[index];
        if (effect->kind == EFFECT_KIND_NONE) {
            effect->position[AXIS_X] = position[AXIS_X] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Y] = position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Z] = position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            effect->kind = EFFECT_KIND_RIPPLE;
            effect->unk3B = 0;
            effect->matrixIndex = sNextMatrixIndex;
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
            /* Updating sNextMatrixIndex here is there to match */
            sNextMatrixIndex = (sNextMatrixIndex + 1) % EFFECT_COUNT;
            scale[AXIS_Z] = effect->scale;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(matrix, &sEffectMatrices[effect->matrixIndex]);
            return;
        }
    }
}

#define DROP_HEADING_COUNT 32
/* Horizontal speeds, in course units a frame */
#define DROP_SLOWEST_SPEED 200
#define DROP_SPEED_STEP 20
/* World units a frame; the faster a drop flies out, the faster it rises */
#define DROP_SLOWEST_RISE_SPEED (2700 * COURSE_TO_WORLD_SCALE)

/*
 * Launches a drop up and out from a point, in one of DROP_HEADING_COUNT headings across a
 * quarter turn.
 */
void LaunchDrop(CoursePoint point) {
    Vec3 translation;
    Vec3 scale;
    Vec3 rotation;
    Mat4 matrix;
    s32 index;
    Effect *effect;
    s32 random;
    /* Of the horizontal speed above the slowest */
    s32 speedOffset;
    f32 speed;
    f32 heading;

    sSpawnGroundHeight = (f32)GetGroundHeight(point[AXIS_X], point[AXIS_Z]);
    for (index = 0; index < EFFECT_COUNT; index++) {
        effect = &sEffects[index];
        if (effect->kind == EFFECT_KIND_NONE) {
            effect->position[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            random = guRandom() % DROP_HEADING_COUNT;
            speedOffset = random * DROP_SPEED_STEP;
            speed = speedOffset + DROP_SLOWEST_SPEED;
            heading = (f32)(random * (M_PI / 2 / DROP_HEADING_COUNT)) - M_PI / 4;
            effect->velocity[AXIS_X] = cosf(heading) * speed * COURSE_TO_WORLD_SCALE;
            effect->velocity[AXIS_Z] = sinf(heading) * speed * COURSE_TO_WORLD_SCALE;
            effect->velocity[AXIS_Y] = -DROP_SLOWEST_RISE_SPEED - speedOffset * COURSE_TO_WORLD_SCALE;
            effect->unk24 = 0;
            effect->kind = EFFECT_KIND_DROP;
            effect->alphaStep = 0;
            effect->scaleStep = 0;
            effect->alpha = 255.0f;
            effect->scale = 0.5f;
            translation[AXIS_X] = effect->position[AXIS_X];
            translation[AXIS_Y] = effect->position[AXIS_Y];
            translation[AXIS_Z] = effect->position[AXIS_Z];
            rotation[AXIS_X] = effect->unk24;
            rotation[AXIS_Y] = 0;
            rotation[AXIS_Z] = effect->unk24;
            scale[AXIS_X] = effect->scale;
            scale[AXIS_Y] = effect->scale;
            scale[AXIS_Z] = effect->scale;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(matrix, &D_801B9418[g_DrawBufferIndex]);
            return;
        }
    }
}

/* Starts a sparkle at position; passed rings spawn them */
void SpawnSparkle(CoursePoint position) {
    s32 index;
    Effect *effect;

    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &sEffects[index];
        if (effect->kind == EFFECT_KIND_NONE) {
            effect->position[AXIS_X] = position[AXIS_X] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Y] = position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Z] = position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            effect->velocity[AXIS_X] = 0;
            effect->velocity[AXIS_Y] = SPARKLE_FALL_SPEED;
            effect->velocity[AXIS_Z] = 0;
            effect->unk38 = 0;
            effect->alpha = 255.0f;
            effect->alphaStep = -4.0f;
            effect->scale = 24.0f;
            effect->scaleStep = -0.2f;
            effect->kind = EFFECT_KIND_SPARKLE;
            effect->unk3B = 0;
            return;
        }
    }
}

/* Unreferenced */
void SpawnUnkEEffect(u32 slotIndex, s32 pointId) {
    Vec3 translation; /* Unused; it is there to match */
    Vec3 rotation;    /* Unused; it is there to match */
    Vec3 scale;       /* Unused; it is there to match */
    Mat4 matrix;      /* Unused; it is there to match */
    Vec3 point;
    s32 index;
    s16 riseScale; /* Always 1; the original has it */
    Effect *effect;

    if (GetCharacterPointPosition(slotIndex, pointId, point) == 0) {
        riseScale = 1;
        for (index = 0; index != EFFECT_COUNT; index++) {
            effect = &sEffects[index];
            if (effect->kind == EFFECT_KIND_NONE) {
                effect->position[AXIS_X] = point[AXIS_X];
                effect->position[AXIS_Y] = point[AXIS_Y];
                effect->position[AXIS_Z] = point[AXIS_Z];
                effect->velocity[AXIS_X] = 0;
                effect->velocity[AXIS_Y] = riseScale * (-1.5 * 0.1);
                effect->velocity[AXIS_Z] = 0;
                effect->unk38 = 0;
                effect->alpha = 230.0f;
                effect->alphaStep = -2.0f;
                effect->scale = 2.0f;
                effect->scaleStep = 0.02f;
                effect->kind = EFFECT_KIND_UNKE;
                effect->unk3B = 0;
                return;
            }
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/effects", func_80079ADC);

#define RISING_CLUMP_COUNT 3
/* In course units; course y points down */
#define RISING_CLUMP_HEIGHT 15360.0f

/*
 * Starts RISING_CLUMP_COUNT clumps rising from above the ball, each faster than the last, that
 * grow as they fade. The callers pass a point, but the ball's position is used instead.
 */
void SpawnRisingClumps(CoursePoint unusedPoint) {
    s32 index;
    s32 spawnCount;
    Effect *effect;

    spawnCount = 0;
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &sEffects[index];
        if (effect->kind == EFFECT_KIND_NONE) {
            effect->position[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Y] =
                (g_CurrentShot.ballPosition[AXIS_Y] - RISING_CLUMP_HEIGHT) * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            effect->velocity[AXIS_X] = 0;
            effect->velocity[AXIS_Y] = (spawnCount * 0.3f + 0.1f + guRandom() % 10 * 0.01f) * -1.5f;
            effect->velocity[AXIS_Z] = 0;
            effect->alpha = 255.0f;
            effect->alphaStep = -1.0f;
            effect->scale = guRandom() % 30 * 0.1f + 1.0f;
            effect->scaleStep = guRandom() % 10 * 0.001f + 0.04;
            effect->unk3B = 0;
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
#define DEBRIS_SPEED_STEP 0x600
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

/*
 * A random number from 0 to below limit, or 0 when limit is 0. It is a macro, as an inline
 * function compiles differently from the original.
 */
#define RandomBelow(limit) ((limit) == 0 ? 0 : guRandom() % (limit))

/*
 * Throws up the debris of a shot struck from point: a drop for a teed-up ball, a divot and its
 * mark for an iron or wedge from grass, and the low and high debris that the debris kind gives.
 * The amount of debris grows with -sin(pitch). The nested spawners share the count and index.
 */
void SpawnShotDebris(CoursePoint point, f32 pitch, f32 yaw, s32 launchSpeed) {
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
    f32 heading;

    /* Spawns up to debrisCount slow-rising, slow-fading pieces of debris */
    s32 SpawnLowDebris(s32 slotCount, s32 debrisCount, s32 kind) {
        Effect *effect;
        s32 speed;

        spawnedCount = 0;
        for (index = 0; index < EFFECT_COUNT; index++) {
            effect = &sEffects[index];
            if (spawnedCount == debrisCount) {
                break;
            }
            spawnedCount++;
            slotCount++;
            if (effect->kind == EFFECT_KIND_NONE) {
                effect->position[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
                effect->position[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                effect->position[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                /* Reading launchSpeed here is there to match */
                speed = launchSpeed;
                if (kind == EFFECT_KIND_UNK2) {
                    speed = (RandomBelow(spawnedCount * 2) + DEBRIS_SLOWEST_STEP_COUNT) * DEBRIS_SPEED_STEP;
                } else {
                    speed = (RandomBelow(spawnedCount * 3) + DEBRIS_SLOWEST_STEP_COUNT) * DEBRIS_SPEED_STEP;
                }
                effect->velocity[AXIS_X] = cosYaw * speed * DEBRIS_OUTWARD_SCALE;
                effect->velocity[AXIS_Y] = sinPitch * speed * (0.1f * COURSE_TO_WORLD_SCALE);
                effect->velocity[AXIS_Z] = sinYaw * speed * DEBRIS_OUTWARD_SCALE;
                effect->unk38 = groundColor;
                effect->alpha = guRandom() % 32 + 95;
                effect->alphaStep = -1.0f;
                if (kind == EFFECT_KIND_UNK1) {
                    effect->scale = (guRandom() % 16 + 10) * 0.2f;
                    effect->scaleStep = debrisAmount * 0.01f + 0.6f;
                } else {
                    effect->scale = 3.2f;
                    effect->scaleStep = debrisAmount * 0.05f + 0.2;
                }
                effect->kind = kind;
                effect->unk3B = 0;
            }
        }
        return slotCount;
    }

    /* Spawns up to debrisCount faster-rising, faster-fading pieces of debris */
    s32 SpawnHighDebris(s32 slotCount, s32 debrisCount, s32 kind) {
        Effect *effect;
        s32 speed;

        spawnedCount = 0;
        for (index = 0; index < EFFECT_COUNT; index++) {
            effect = &sEffects[index];
            /* The original has it */
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
                speed = (RandomBelow(spawnedCount * 3) + DEBRIS_SLOWEST_STEP_COUNT) * DEBRIS_SPEED_STEP;
                effect->velocity[AXIS_X] = cosYaw * speed * DEBRIS_OUTWARD_SCALE;
                effect->velocity[AXIS_Y] = sinPitch * speed * (0.15f * COURSE_TO_WORLD_SCALE);
                effect->velocity[AXIS_Z] = sinYaw * speed * DEBRIS_OUTWARD_SCALE;
                effect->unk38 = groundColor;
                if (kind == EFFECT_KIND_UNK0) {
                    effect->alpha = guRandom() % 32 + 191;
                } else {
                    effect->alpha = guRandom() % 32 + 159;
                }
                effect->alphaStep = -2.0f;
                effect->scale = guRandom() % 32 * 0.2f;
                effect->scaleStep = debrisAmount * 0.01f + 0.4f;
                effect->kind = kind;
                effect->unk3B = 0;
            }
        }
        return slotCount;
    }

    nextIndex = 0;
    dropCount = 0;
    groundColor = BlendSurfaceMapColor(g_CurrentShot.ballPosition[AXIS_X], g_CurrentShot.ballPosition[AXIS_Z],
                                       g_CurrentShot.surface);
    sinPitch = sinf(pitch);
    cosYaw = cosf(yaw);
    divotCount = 0;
    highDebrisCount = 0;
    sinYaw = sinf(yaw);
    sShotDebris.unk0 = 0;
    sShotDebris.unk8 = -2.0f;
    sShotDebris.unkC = 0.5f;
    lowDebrisCount = 0;
    divotMarkCount = 0;
    if (launchSpeed > LAUNCH_SPEED_MOST) {
        launchSpeed = LAUNCH_SPEED_MOST;
    }
    sShotDebris.clubKind = GetSelectedClub()->kind;
    if (sShotDebris.clubKind == CLUB_KIND_WOOD) {
        debrisAmount = -sinPitch * DEBRIS_AMOUNT_MOST;
    } else if (sShotDebris.clubKind == CLUB_KIND_IRON) {
        debrisAmount = -sinPitch * DEBRIS_AMOUNT_MOST;
    } else if (sShotDebris.clubKind == CLUB_KIND_WEDGE) {
        debrisAmount = -sinPitch * DEBRIS_AMOUNT_MOST;
    } else {
        debrisAmount = 0;
    }
    if (debrisAmount < DEBRIS_AMOUNT_LEAST) {
        debrisAmount = DEBRIS_AMOUNT_LEAST;
    }
    if (debrisAmount > DEBRIS_AMOUNT_MOST) {
        debrisAmount = DEBRIS_AMOUNT_MOST;
    }
    /* Clearing the effects here instead of calling ClearEffects is there to match */
    for (index = 0; index != EFFECT_COUNT; index++) {
        clearedEffect = &sEffects[index];
        clearedEffect->kind = EFFECT_KIND_NONE;
    }
    if (sDebrisKind == DEBRIS_KIND_TEE_UP) {
        dropCount = 1;
        if (sShotDebris.clubKind == CLUB_KIND_IRON) {
            lowDebrisCount = debrisAmount;
        } else if (sShotDebris.clubKind == CLUB_KIND_WEDGE) {
            lowDebrisCount = debrisAmount;
        }
    } else if (sDebrisKind == DEBRIS_KIND_GRASS) {
        if (sShotDebris.clubKind == CLUB_KIND_WOOD) {
            lowDebrisCount = debrisAmount;
        }
        if (sShotDebris.clubKind == CLUB_KIND_IRON || sShotDebris.clubKind == CLUB_KIND_WEDGE) {
            divotCount = 1;
            lowDebrisCount = 2.0f * debrisAmount;
            divotMarkCount = 1;
        }
    } else if (sDebrisKind == DEBRIS_KIND_ROUGH) {
        lowDebrisCount = 2.0f * debrisAmount;
        highDebrisCount = 0;
    } else if (sDebrisKind == DEBRIS_KIND_DUST) {
        lowDebrisCount = 2.0f * debrisAmount;
        highDebrisCount = lowDebrisCount;
    } else if (sDebrisKind == DEBRIS_KIND_BURIED) {
        debrisAmount += BURIED_DEBRIS_EXTRA;
        if (debrisAmount > DEBRIS_AMOUNT_MOST) {
            debrisAmount = DEBRIS_AMOUNT_MOST;
        }
        lowDebrisCount = 2.0f * debrisAmount;
        highDebrisCount = lowDebrisCount;
    }
    spawnedCount = 0;
    for (index = nextIndex; index < EFFECT_COUNT; index++) {
        effect = &sEffects[index];
        if (spawnedCount == dropCount) {
            break;
        }
        nextIndex++;
        if (effect->kind == EFFECT_KIND_NONE) {
            effect->position[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            effect->position[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            headingIndex = g_CurrentShot.unk0 & (DROP_HEADING_COUNT - 1);
            speed = headingIndex * DROP_SPEED_STEP + DROP_SLOWEST_SPEED;
            heading = (f32)((u16)g_CurrentShot.unk0 * BINARY_ANGLE_TO_RADIANS)
                    + (f32)(headingIndex * (M_PI / 2 / DROP_HEADING_COUNT)) - M_PI / 4;
            effect->velocity[AXIS_X] = cosf(heading) * speed * COURSE_TO_WORLD_SCALE;
            effect->velocity[AXIS_Z] = sinf(heading) * speed * COURSE_TO_WORLD_SCALE;
            if (sShotDebris.clubKind == CLUB_KIND_PUTTER) {
                effect->velocity[AXIS_Y] =
                    -PUTTED_DROP_SLOWEST_RISE_SPEED - headingIndex * PUTTED_DROP_RISE_STEP * COURSE_TO_WORLD_SCALE;
            } else {
                effect->velocity[AXIS_Y] =
                    -DROP_SLOWEST_RISE_SPEED - headingIndex * DROP_SPEED_STEP * COURSE_TO_WORLD_SCALE;
            }
            effect->unk24 = 0.0f;
            effect->kind = EFFECT_KIND_DROP;
            effect->alphaStep = 0;
            effect->scaleStep = 0;
            effect->alpha = 255.0f;
            effect->scale = SHOT_DROP_SCALE;
            translation[AXIS_X] = effect->position[AXIS_X];
            translation[AXIS_Y] = effect->position[AXIS_Y];
            translation[AXIS_Z] = effect->position[AXIS_Z];
            rotation[AXIS_X] = effect->unk24;
            rotation[AXIS_Y] = 0.0f;
            rotation[AXIS_Z] = effect->unk24;
            scale[AXIS_X] = effect->scale;
            scale[AXIS_Y] = effect->scale;
            spawnedCount++;
            scale[AXIS_Z] = effect->scale;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(matrix, &D_801B9418[g_DrawBufferIndex]);
        }
    }
    /* The divot flies out on the mirror image of the drop's heading */
    spawnedCount = 0;
    for (index = nextIndex; index < EFFECT_COUNT; index++) {
        divotEffect = &sEffects[index];
        if (spawnedCount == divotCount) {
            break;
        }
        nextIndex++;
        if (divotEffect->kind == EFFECT_KIND_NONE) {
            divotEffect->position[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
            divotEffect->position[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            divotEffect->position[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            headingIndex = g_CurrentShot.unk0 & (DROP_HEADING_COUNT - 1);
            speed = headingIndex * DROP_SPEED_STEP + DROP_SLOWEST_SPEED;
            heading = (f32)((u16)g_CurrentShot.unk0 * BINARY_ANGLE_TO_RADIANS)
                    - (f32)(headingIndex * (M_PI / 2 / DROP_HEADING_COUNT)) + M_PI / 4;
            divotEffect->velocity[AXIS_X] = cosf(heading) * speed * COURSE_TO_WORLD_SCALE;
            divotEffect->velocity[AXIS_Z] = sinf(heading) * speed * COURSE_TO_WORLD_SCALE;
            divotEffect->kind = EFFECT_KIND_DIVOT;
            divotEffect->unk24 = 0;
            divotEffect->alphaStep = 0;
            divotEffect->alpha = 255.0f;
            divotEffect->velocity[AXIS_Y] =
                -DROP_SLOWEST_RISE_SPEED - headingIndex * DROP_SPEED_STEP * COURSE_TO_WORLD_SCALE;
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
    /* The mark stays at point, turned by yaw */
    spawnedCount = 0;
    for (index = nextIndex; index < EFFECT_COUNT; index++) {
        divotMarkEffect = &sEffects[index];
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
            divotMarkEffect->alpha = 255.0f;
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
            ConvertMatrixToFixed(matrix, &sDivotMarkMatrix);
        }
    }
    switch (sDebrisKind) {
        case DEBRIS_KIND_TEE_UP:
        case DEBRIS_KIND_GRASS:
            SpawnLowDebris(nextIndex, lowDebrisCount, EFFECT_KIND_UNK2);
            break;
        case DEBRIS_KIND_ROUGH:
            nextIndex += SpawnLowDebris(nextIndex, lowDebrisCount, EFFECT_KIND_UNK4);
            SpawnHighDebris(nextIndex, highDebrisCount, EFFECT_KIND_UNK3);
            break;
        case DEBRIS_KIND_DUST:
        case DEBRIS_KIND_BURIED:
            nextIndex += SpawnLowDebris(nextIndex, lowDebrisCount, EFFECT_KIND_UNK1);
            SpawnHighDebris(nextIndex, highDebrisCount, EFFECT_KIND_UNK0);
            break;
    }
}

INCLUDE_ASM("asm/nonmatchings/effects", func_8007B054);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007B994);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007C5D8);
INCLUDE_RODATA("asm/nonmatchings/effects", D_800D1A40);

/* Shadows are drawn only between these depths from ProjectPoint */
#define SHADOW_NEAREST_DEPTH (-3.0f)
#define SHADOW_FARTHEST_DEPTH (-60.0f)
#define SHADOW_ALPHA 63

/* Draws a dot of shadow on the spawn ground height, under an effect */
void DrawEffectShadow(Gfx **displayListHead, Effect *effect) {
    Gfx *gfx = *displayListHead;
    ScreenPoint shadow;
    f32 depth;

    depth = ProjectPoint(effect->position[AXIS_X], sSpawnGroundHeight * COURSE_TO_WORLD_SCALE,
                         effect->position[AXIS_Z], &shadow);
    /* Nesting the tests is there to match */
    if (depth < SHADOW_NEAREST_DEPTH) {
        if (depth > SHADOW_FARTHEST_DEPTH) {
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
            gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, SHADOW_ALPHA);
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(gfx++, (shadow.x - 1) << 2, (shadow.y - 1) << 2, (shadow.x + 1) << 2,
                                    (shadow.y + 1) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
            gDPPipeSync(gfx++);
        }
    }
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/effects", func_8007D19C);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007D4E8);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007DB08);

/* Ripples and spreading circles are drawn in this light gray */
#define CIRCLE_GRAY 230

/* Draws a ripple at its matrix, faded by its alpha */
void DrawRipple(Gfx **displayListHead, Effect *effect) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, CIRCLE_GRAY, CIRCLE_GRAY, CIRCLE_GRAY, (u32)effect->alpha);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sEffectMatrices[effect->matrixIndex]),
              G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPVertex(gfx++, sRippleVertices, 4, 0);
    gDPPipeSync(gfx++);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* Draws a spreading circle at its matrix, faded by its alpha */
void DrawSpreadingCircle(Gfx **displayListHead, Effect *effect) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, CIRCLE_GRAY, CIRCLE_GRAY, CIRCLE_GRAY, (u32)effect->alpha);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sEffectMatrices[effect->matrixIndex]),
              G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPVertex(gfx++, sSpreadingCircleVertices, 4, 0);
    gDPPipeSync(gfx++);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

#define RUMBLE_FREQUENCY 0x100
#define RUMBLE_FRAME_COUNT 10

/* Rumbles the player's controller briefly if it has a Rumble Pak, unless D_801B60B9 is set */
void StartPlayerRumble(UnkB8 *player) {
    if (player->controllerIndex < MAXCONTROLLERS && !D_801B60B9) {
        switch (nuContRmbCheck(g_PlayerControllerPorts[player->controllerIndex])) {
            case 0:
                nuContRmbStart(g_PlayerControllerPorts[player->controllerIndex], RUMBLE_FREQUENCY, RUMBLE_FRAME_COUNT);
                break;
            /* The empty cases are there to match */
            case PFS_ERR_NOPACK:
                break;
            case PFS_ERR_CONTRFAIL:
                break;
        }
    }
}

/* The same as StartPlayerRumble; the ball simulation calls it when the ball passes a ring */
void StartRingPassRumble(UnkB8 *player) {
    if (player->controllerIndex < MAXCONTROLLERS && !D_801B60B9) {
        switch (nuContRmbCheck(g_PlayerControllerPorts[player->controllerIndex])) {
            case 0:
                nuContRmbStart(g_PlayerControllerPorts[player->controllerIndex], RUMBLE_FREQUENCY, RUMBLE_FRAME_COUNT);
                break;
            /* The empty cases are there to match */
            case PFS_ERR_NOPACK:
                break;
            case PFS_ERR_CONTRFAIL:
                break;
        }
    }
}

#define RAIN_SPLASH_IMAGE_ASSET 0x6BC
#define RAIN_SOUND_CHANNEL 9

/* Sets the weather, and for rain loads its image and sets up its buffers */
void StartWeather(s32 weather) {
    EncodedAsset asset;

    if (weather == WEATHER_RAIN) {
        sRainSplashImage = AllocMemory(OpenEncodedAsset(RAIN_SPLASH_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sRainSplashImage, &asset);
        sRaindrops = AllocMemory(RAINDROP_COUNT * sizeof(Raindrop));
        sRainSplashes = AllocMemory(RAIN_SPLASH_COUNT * sizeof(RainSplash));
        StartRain();
    }
    sWeather = weather;
}

/* Frees what StartWeather loaded and stops the rain's sound */
void EndWeather(void) {
    if (sWeather == WEATHER_RAIN) {
        FreeMemory(&sRainSplashImage);
        FreeMemory(&sRaindrops);
        FreeMemory(&sRainSplashes);
    }
    StopSound(RAIN_SOUND_CHANNEL);
    sWeather = WEATHER_NONE;
}

#define RAIN_CENTER_DISTANCE 150.0f
/* New raindrops and splashes are placed in steps of this many course units around the center */
#define RAIN_SPREAD_STEP 0x600
#define RAIN_SPLASH_SPREAD_STEP_COUNT 500
#define RAIN_FALL_SPEED 60.0f
#define WIND_SPEED_UNIT 0x10000
/* How far the rain drifts sideways per unit of fall, for each whole unit of wind speed */
#define RAIN_DRIFT_PER_WIND_SPEED 0.1f

/* Centers the rain RAIN_CENTER_DISTANCE ahead of the camera, in the direction it looks */
void PlaceRainCenter(void) {
    s32 viewX;
    s32 viewZ;
    f32 scale;

    viewX = g_Cameras[0].view.lookAt[AXIS_X] - g_Cameras[0].view.eye[AXIS_X];
    viewZ = g_Cameras[0].view.lookAt[AXIS_Z] - g_Cameras[0].view.eye[AXIS_Z];
    /* Holding the view's length in scale is there to match */
    scale = sqrtf(viewX * viewX + viewZ * viewZ);
    sRainCenter[0] = viewX;
    sRainCenter[1] = viewZ;
    if ((s32)scale != 0.0f) {
        scale = RAIN_CENTER_DISTANCE / scale;
    } else {
        osSyncPrintf("Rain Pos Error");
        scale = 0.0f;
    }
    sRainCenter[0] = sRainCenter[0] * scale + g_Cameras[0].view.eye[AXIS_X];
    sRainCenter[1] = sRainCenter[1] * scale + g_Cameras[0].view.eye[AXIS_Z];
}

#define RAIN_SPREAD_STEP_COUNT 200
/* Course units above the drop's lowest start, which is RAIN_TOP_BELOW_EYE below the camera */
#define RAINDROP_LOWEST_RISE 0x1A400
#define RAIN_TOP_BELOW_EYE 15.0f
#define RAINDROP_LENGTH 90.0f

/*
 * Places a raindrop at a random spot above the area around the rain's center, moved back along
 * the rain's path by as far as it drifts while falling to just below the camera.
 * It does not match: from the head's z drift on, the instructions are scheduled differently.
 */
#ifdef NON_MATCHING
void ResetRaindrop(Raindrop *drop) {
    f32 fallFrameCount;

    drop->head[AXIS_X] =
        (rand() % RAIN_SPREAD_STEP_COUNT - RAIN_SPREAD_STEP_COUNT / 2) * RAIN_SPREAD_STEP * COURSE_TO_WORLD_SCALE
        + sRainCenter[0];
    /* Course y points down */
    drop->head[AXIS_Y] =
        (-RAINDROP_LOWEST_RISE - rand() % RAIN_SPREAD_STEP_COUNT * RAIN_SPREAD_STEP) * COURSE_TO_WORLD_SCALE;
    drop->head[AXIS_Z] =
        (rand() % RAIN_SPREAD_STEP_COUNT - RAIN_SPREAD_STEP_COUNT / 2) * RAIN_SPREAD_STEP * COURSE_TO_WORLD_SCALE
        + sRainCenter[1];
    fallFrameCount = drop->head[AXIS_Y] / sRainVelocity[AXIS_Y];
    drop->head[AXIS_Y] += g_Cameras[0].view.eye[AXIS_Y] + RAIN_TOP_BELOW_EYE;
    drop->head[AXIS_X] += sRainVelocity[AXIS_X] * fallFrameCount;
    drop->head[AXIS_Z] += sRainVelocity[AXIS_Z] * fallFrameCount;
    drop->unk18 = 0;
    drop->unk2A = 0;
    drop->unk2C = -1;
    drop->unk2D = 0;
    drop->unk1C = 1.0f;
    drop->tail[AXIS_Y] = drop->head[AXIS_Y] + RAINDROP_LENGTH;
    drop->tail[AXIS_X] = drop->head[AXIS_X] + sRainDriftX * RAINDROP_LENGTH;
    drop->tail[AXIS_Z] = drop->head[AXIS_Z] + sRainDriftZ * RAINDROP_LENGTH;
}
#else
INCLUDE_ASM("asm/nonmatchings/effects", ResetRaindrop);
#endif

/* Moves a splash to a random spot on the ground around the rain's center */
void ResetRainSplash(RainSplash *splash) {
    splash->position[AXIS_X] =
        (rand() % RAIN_SPLASH_SPREAD_STEP_COUNT - RAIN_SPLASH_SPREAD_STEP_COUNT / 2) * RAIN_SPREAD_STEP
            * COURSE_TO_WORLD_SCALE
        + sRainCenter[0];
    splash->position[AXIS_Z] =
        (rand() % RAIN_SPLASH_SPREAD_STEP_COUNT - RAIN_SPLASH_SPREAD_STEP_COUNT / 2) * RAIN_SPREAD_STEP
            * COURSE_TO_WORLD_SCALE
        + sRainCenter[1];
    splash->position[AXIS_Y] = GetGroundHeight(splash->position[AXIS_X] * WORLD_TO_COURSE_SCALE,
                                               splash->position[AXIS_Z] * WORLD_TO_COURSE_SCALE)
                             * COURSE_TO_WORLD_SCALE;
    splash->unk18 = 0;
}

/* Sets the rain's path from the wind, and places every raindrop and splash */
void StartRain(void) {
    f32 direction;
    f32 speed;
    s32 index;

    D_80104AC0 = 0;
    direction = g_HoleConditions.windDirection * WIND_DIRECTION_TO_RADIANS;
    speed = g_HoleConditions.windSpeed / WIND_SPEED_UNIT * RAIN_DRIFT_PER_WIND_SPEED;
    sRainDriftX = cosf(direction) * speed;
    sRainDriftZ = sinf(direction) * speed;
    sRainVelocity[AXIS_Y] = RAIN_FALL_SPEED;
    sRainVelocity[AXIS_X] = sRainDriftX * RAIN_FALL_SPEED;
    sRainVelocity[AXIS_Z] = sRainDriftZ * RAIN_FALL_SPEED;
    PlaceRainCenter();
    for (index = 0; index != RAINDROP_COUNT; index++) {
        ResetRaindrop(&sRaindrops[index]);
        sRaindrops[index].unk2A = 0;
    }
    for (index = 0; index != RAIN_SPLASH_COUNT; index++) {
        ResetRainSplash(&sRainSplashes[index]);
        sRainSplashes[index].unkC = 1.0f;
        sRainSplashes[index].unk16 = index % 9 - 1;
        sRainSplashes[index].unk17 = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/effects", func_8007E980);
INCLUDE_RODATA("asm/nonmatchings/effects", D_800D1A88);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007EF0C);

#define SPINNING_BADGE_IMAGE_ASSET 0x663
#define COUNT_GLYPHS_IMAGE_ASSET 0x689

/*
 * Starts showing the birdie badge count, which func_8007FEAC draws. Its
 * spinning badge and digit images borrow the cloud and clump images' memory.
 */
void ShowBirdieBadgeCount(s32 badgeCount) {
    EncodedAsset asset;

    sShownBadgeCount = badgeCount;
    sIsBadgeCountShown = TRUE;
    sBadgeCountFrame = 0;
    OpenEncodedAsset(SPINNING_BADGE_IMAGE_ASSET, &asset);
    ReadEncodedAsset(sCloudImage, &asset);
    OpenEncodedAsset(COUNT_GLYPHS_IMAGE_ASSET, &asset);
    ReadEncodedAsset(sClumpImage, &asset);
}

INCLUDE_ASM("asm/nonmatchings/effects", func_8007FEAC);

