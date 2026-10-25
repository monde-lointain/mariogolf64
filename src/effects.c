#include "common.h"
#include "asset.h"
#include "ball_simulation.h"
#include "camera.h"
#include "character_slot.h"
#include "club.h"
#include "controller.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "effects.h"
#include "functions.h"
#include "heap.h"
#include "libc.h"
#include "math_util.h"
#include "save.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "surface.h"
#include "terrain.h"
#include "text.h"
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
#define RAINDROP_FALLING (-1)

/* Raindrop flags bits */
#define RAINDROP_OFF_SCREEN (1 << 0)
#define RAINDROP_HEAD_GROUNDED (1 << 4) /* The head has reached the ground too */
#define RAIN_SPLASH_COUNT 32

typedef struct Raindrop {
    /* 0x00 */ Vec3 head;
    /* 0x0C */ Vec3 tail;
    /* 0x18 */ f32 groundHeight; /* Under the drop; UpdateRain refreshes one drop each frame */
    /* 0x1C */ f32 tailScale;   /* The tail's projected scale */
    /* 0x20 */ s16 headScreenX; /* In quarter pixels */
    /* 0x22 */ s16 headScreenY;
    /* 0x24 */ s16 tailScreenX;
    /* 0x26 */ s16 tailScreenY;
    /* 0x28 */ s16 tailDepth;
    /* 0x2A */ u16 flags; /* RAINDROP_* bits */
    /* 0x2C */ s8 landedFrameCount; /* RAINDROP_FALLING until the drop reaches the ground */
    /* 0x2D */ u8 splashColumn; /* The column of sRainSplashImage frames it lands with */
} Raindrop; /* size = 0x30 */

typedef struct RainSplash {
    /* 0x00 */ Vec3 position;
    /* 0x0C */ f32 unkC;
    /* 0x10 */ u8 unk10[6];
    /* 0x16 */ s8 frameCount;
    /* 0x17 */ s8 unk17;
    /* 0x18 */ u8 column; /* The column of sRainSplashImage frames it shows */
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
extern Mtx sDropMatrices[]; /* One per draw buffer */
extern Mtx sDivotMarkMatrix;
extern ShotDebris sShotDebris;
extern s32 sSpawnGroundHeight;
extern f32 g_RainSoundVolume;
extern Gfx D_800C3FC0[];
extern u8 *sRainSplashImage;
extern Mtx sRainModelViewMatrix;
extern Raindrop *sRaindrops;
extern RainSplash *sRainSplashes;
/* The x and z that the rain falls around, RAIN_CENTER_DISTANCE ahead of the camera */
extern s32 sRainCenter[2];
/* How far the wind blows the rain sideways for each unit it falls */
extern f32 sRainDriftX;
extern f32 sRainDriftZ;
extern Vec3 sRainVelocity; /* Per frame */
extern s32 sGroundCheckDropIndex; /* The raindrop whose ground height UpdateRain refreshes next */

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
Vtx sRainStreakVertices[] = {
    { { { 0, 0, -100 }, 0, { 0, 0 }, { 0xAF, 0xAF, 0xBF, 0x1F } } },
    { { { 0, -1, -100 }, 0, { 0, 0 }, { 0xAF, 0xAF, 0xBF, 0x1F } } },
    { { { 48, -1, -100 }, 0, { 0, 0 }, { 0x90, 0xA0, 0xA0, 0x5F } } },
    { { { 48, 0, -100 }, 0, { 0, 0 }, { 0x90, 0xA0, 0xA0, 0x5F } } },
};
s32 sWeather = WEATHER_NONE; /* A WEATHER_* value */
Vec3 sLastRainEye = { 0.0f, 0.0f, 0.0f }; /* The camera's eye on UpdateRain's last frame */
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
            ConvertMatrixToFixed(matrix, &sDropMatrices[g_DrawBufferIndex]);
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

#define DEBRIS_SPEED_STEP 0x600
#define IMPACT_PUFF_COUNT 3

/*
 * Spawns effects at g_BallPosition by the surface kind under the ball: rising puffs of the ground's
 * color on fairway lies and semi-rough, larger and faster ones on bunker lies and bare ground, or
 * a ripple on water.
 */
void SpawnImpactEffects(BallSimulation *simulation) {
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 matrix;
    s32 index;
    s32 puffNumber;
    s32 color;
    Effect *effect;

    color = BlendSurfaceMapColor(simulation->position[AXIS_X], simulation->position[AXIS_Z], simulation->surface);
    puffNumber = 1;
    switch (GetSurfaceInfo(simulation->surface)->kind) {
        case SURFACE_KIND_FAIRWAY:
        case SURFACE_KIND_FAIRWAY_FAIR_LIE:
        case SURFACE_KIND_FAIRWAY_POOR_LIE:
        case SURFACE_KIND_SEMI_ROUGH:
            for (index = 0; index != EFFECT_COUNT; index++) {
                effect = &sEffects[index];
                if (effect->kind == EFFECT_KIND_NONE) {
                    effect->position[AXIS_X] = g_BallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
                    effect->position[AXIS_Y] = g_BallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                    effect->position[AXIS_Z] = g_BallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                    effect->velocity[AXIS_X] = 0;
                    effect->velocity[AXIS_Y] = -puffNumber * DEBRIS_SPEED_STEP * (0.2f * COURSE_TO_WORLD_SCALE);
                    effect->velocity[AXIS_Z] = 0;
                    effect->unk38 = color;
                    effect->alpha = 230.0f;
                    effect->alphaStep = -2.0f;
                    effect->scale = 1.0f;
                    effect->scaleStep = 0.04f;
                    effect->kind = EFFECT_KIND_UNK0;
                    effect->unk3B = 0;
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
        case SURFACE_KIND_BUNKER_UNK9:
        case SURFACE_KIND_BARE_GROUND:
            for (index = 0; index != EFFECT_COUNT; index++) {
                effect = &sEffects[index];
                if (effect->kind == EFFECT_KIND_NONE) {
                    effect->position[AXIS_X] = g_BallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
                    effect->position[AXIS_Y] = g_BallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                    effect->position[AXIS_Z] = g_BallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                    effect->velocity[AXIS_X] = 0;
                    effect->velocity[AXIS_Y] = -puffNumber * DEBRIS_SPEED_STEP * (0.7f * COURSE_TO_WORLD_SCALE);
                    effect->velocity[AXIS_Z] = 0;
                    effect->unk38 = color;
                    effect->alpha = 230.0f;
                    effect->alphaStep = -2.0f;
                    effect->scale = 2.0f;
                    effect->scaleStep = 0.1f;
                    effect->kind = EFFECT_KIND_UNK0;
                    effect->unk3B = 0;
                    puffNumber++;
                    if (puffNumber == IMPACT_PUFF_COUNT + 1) {
                        break;
                    }
                }
            }
            break;
        case SURFACE_KIND_WATER_HAZARD:
            /* The original spawns the ripple here instead of calling SpawnRipple */
            for (index = 0; index != EFFECT_COUNT; index++) {
                effect = &sEffects[index];
                if (effect->kind == EFFECT_KIND_NONE) {
                    effect->position[AXIS_X] = g_BallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
                    effect->position[AXIS_Y] = g_BallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                    effect->position[AXIS_Z] = g_BallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
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
                    sNextMatrixIndex = (sNextMatrixIndex + 1) % EFFECT_COUNT;
                    scale[AXIS_Z] = effect->scale;
                    BuildTransformMatrix(matrix, rotation, translation, scale);
                    ConvertMatrixToFixed(matrix, &sEffectMatrices[effect->matrixIndex]);
                    return;
                }
            }
            break;
        /* The empty case is there to match */
        case SURFACE_KIND_DEEP_ROUGH:
            break;
    }
}

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
            ConvertMatrixToFixed(matrix, &sDropMatrices[g_DrawBufferIndex]);
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

#define WIND_SPEED_UNIT 0x10000
/* How far the wind pushes an effect a frame, in world units, for each whole unit of wind speed */
#define EFFECT_PUSH_PER_WIND_SPEED 0.005f
/* Positive y velocities fall, in world units a frame each frame */
#define EFFECT_FALL_ACCELERATION (26.2f * COURSE_TO_WORLD_SCALE)
#define RISING_CLUMP_FALL_ACCELERATION (2 * EFFECT_FALL_ACCELERATION)
#define OTHER_EFFECT_FALL_ACCELERATION (8 * EFFECT_FALL_ACCELERATION)
#define TEAR_FALL_ACCELERATION 0.23027343f
/* The share of its velocity that an effect keeps each frame */
#define RISING_CLUMP_VELOCITY_KEPT 0.95f
#define LIGHT_EFFECT_VELOCITY_KEPT 0.97f
#define HIGH_DEBRIS_VELOCITY_KEPT 0.8f
#define OTHER_EFFECT_VELOCITY_KEPT 0.95f
/* Kind 0xE effects feel this share of the wind */
#define UNKE_WIND_SHARE 0.1f
/* Effects past the ground height fade this many times as fast */
#define PAST_GROUND_FADE_SCALE 4.0f
/* Effects spin about x and z by SPIN_PER_SPEED for each unit their fall speed is below this */
#define SPIN_FALL_SPEED 100.0f
#define SPIN_PER_SPEED (M_PI * 0.001f)
/* Kinds 0 and 3 (high debris) and most other kinds pick frame 0, 1 or 2 by their fall speed */
#define HIGH_DEBRIS_FAST_RISE_SPEED (-1.5f)
#define HIGH_DEBRIS_SLOW_RISE_SPEED (-0.15f)
#define OTHER_EFFECT_FAST_RISE_SPEED (-1.5f)
#define OTHER_EFFECT_FAST_FALL_SPEED 1.5f
#define TEAR_FRAME_STEP 2
#define TEAR_LAST_FRAME 40
#define SPARKLE_LAST_FRAME 20
#define CYCLING_FRAME_COUNT 3
/* Drops and divots past the ground height are held this many course units past it */
#define GROUNDED_EFFECT_DEPTH 15.36f

/* It is a macro, as an inline function compiles differently from the original */
#define IsPastSpawnGround(effect) ((f32)sSpawnGroundHeight < (effect)->position[AXIS_Y] * WORLD_TO_COURSE_SCALE)

/*
 * Moves, fades, spins and animates every effect for a frame, and removes those that are done.
 * The wind pushes rising clumps, kind 0xE effects and kinds 0 and 3. Most effects past the spawn
 * ground height fade faster. It also rebuilds the transform matrices of ripples, spreading
 * circles and drops.
 */
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

    direction = g_HoleConditions.windDirection * WIND_DIRECTION_TO_RADIANS;
    speed = g_HoleConditions.windSpeed / WIND_SPEED_UNIT * EFFECT_PUSH_PER_WIND_SPEED;
    windPushX = cosf(direction) * speed;
    windPushZ = sinf(direction) * speed;
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &sEffects[index];
        if (effect->kind == EFFECT_KIND_NONE) {
            continue;
        }
        if (IsPastSpawnGround(effect)
            && effect->kind != EFFECT_KIND_RIPPLE && effect->kind != EFFECT_KIND_SPREADING_CIRCLE
            && effect->kind != EFFECT_KIND_SPARKLE && effect->kind != EFFECT_KIND_TEAR) {
            if (effect->kind == EFFECT_KIND_RISING_CLUMP) {
                effect->alpha = 0.0f;
            } else {
                effect->alpha += effect->alphaStep * PAST_GROUND_FADE_SCALE;
            }
        } else if (effect->alpha > 0.0f || effect->scale > 0.0f) {
            effect->position[AXIS_X] += effect->velocity[AXIS_X];
            effect->position[AXIS_Y] += effect->velocity[AXIS_Y];
            effect->position[AXIS_Z] += effect->velocity[AXIS_Z];
            /* Accelerating every axis before keeping a share of each is there to match */
            if (effect->kind == EFFECT_KIND_RISING_CLUMP) {
                effect->velocity[AXIS_X] += windPushX;
                effect->velocity[AXIS_Y] += RISING_CLUMP_FALL_ACCELERATION;
                effect->velocity[AXIS_Z] += windPushZ;
                effect->velocity[AXIS_X] *= RISING_CLUMP_VELOCITY_KEPT;
                effect->velocity[AXIS_Y] *= RISING_CLUMP_VELOCITY_KEPT;
                effect->velocity[AXIS_Z] *= RISING_CLUMP_VELOCITY_KEPT;
            } else if (effect->kind == EFFECT_KIND_TEAR) {
                effect->velocity[AXIS_Y] += TEAR_FALL_ACCELERATION;
                effect->velocity[AXIS_X] *= LIGHT_EFFECT_VELOCITY_KEPT;
                effect->velocity[AXIS_Z] *= LIGHT_EFFECT_VELOCITY_KEPT;
                effect->velocity[AXIS_Y] *= LIGHT_EFFECT_VELOCITY_KEPT;
            } else if (effect->kind == EFFECT_KIND_SPARKLE) {
                effect->velocity[AXIS_Y] += EFFECT_FALL_ACCELERATION;
                effect->velocity[AXIS_X] *= LIGHT_EFFECT_VELOCITY_KEPT;
                effect->velocity[AXIS_Z] *= LIGHT_EFFECT_VELOCITY_KEPT;
                effect->velocity[AXIS_Y] *= LIGHT_EFFECT_VELOCITY_KEPT;
            } else if (effect->kind == EFFECT_KIND_UNKE) {
                effect->velocity[AXIS_X] += windPushX * UNKE_WIND_SHARE;
                effect->velocity[AXIS_Y] += 0.0f; /* The original has it */
                effect->velocity[AXIS_Z] += windPushZ * UNKE_WIND_SHARE;
                effect->velocity[AXIS_X] *= LIGHT_EFFECT_VELOCITY_KEPT;
                effect->velocity[AXIS_Y] *= LIGHT_EFFECT_VELOCITY_KEPT;
                effect->velocity[AXIS_Z] *= LIGHT_EFFECT_VELOCITY_KEPT;
            } else if (effect->kind == EFFECT_KIND_UNK0 || effect->kind == EFFECT_KIND_UNK3) {
                effect->velocity[AXIS_X] += windPushX;
                effect->velocity[AXIS_Y] += EFFECT_FALL_ACCELERATION;
                effect->velocity[AXIS_Z] += windPushZ;
                effect->velocity[AXIS_X] *= HIGH_DEBRIS_VELOCITY_KEPT;
                effect->velocity[AXIS_Y] *= HIGH_DEBRIS_VELOCITY_KEPT;
                effect->velocity[AXIS_Z] *= HIGH_DEBRIS_VELOCITY_KEPT;
            } else if (effect->kind == EFFECT_KIND_RIPPLE || effect->kind == EFFECT_KIND_SPREADING_CIRCLE) {
                /* The empty branch is there to match */
            } else {
                effect->velocity[AXIS_Y] += OTHER_EFFECT_FALL_ACCELERATION;
                effect->velocity[AXIS_X] *= OTHER_EFFECT_VELOCITY_KEPT;
                effect->velocity[AXIS_Z] *= OTHER_EFFECT_VELOCITY_KEPT;
                effect->velocity[AXIS_Y] *= OTHER_EFFECT_VELOCITY_KEPT;
            }
            effect->alpha += effect->alphaStep;
            effect->scale += effect->scaleStep;
            if (effect->velocity[AXIS_Y] < SPIN_FALL_SPEED) {
                effect->unk24 -= (effect->velocity[AXIS_Y] - SPIN_FALL_SPEED) * SPIN_PER_SPEED;
            }
            if (effect->unk24 > 2 * M_PI) {
                effect->unk24 -= 2 * M_PI;
            }
            if (effect->unk24 < 0.0f) {
                effect->unk24 += 2 * M_PI;
            }
            if (effect->kind == EFFECT_KIND_UNK0 || effect->kind == EFFECT_KIND_UNK3) {
                if (effect->velocity[AXIS_Y] < HIGH_DEBRIS_FAST_RISE_SPEED) {
                    effect->unk3B = 0;
                } else if (effect->velocity[AXIS_Y] < HIGH_DEBRIS_SLOW_RISE_SPEED) {
                    effect->unk3B = 1;
                } else {
                    effect->unk3B = 2;
                }
            } else if (effect->kind == EFFECT_KIND_TEAR) {
                effect->unk3B += TEAR_FRAME_STEP;
                if (effect->unk3B > TEAR_LAST_FRAME) {
                    effect->kind = EFFECT_KIND_NONE;
                }
            } else if (effect->kind == EFFECT_KIND_SPARKLE) {
                if (effect->unk3B < SPARKLE_LAST_FRAME) {
                    effect->unk3B++;
                } else {
                    effect->kind = EFFECT_KIND_NONE;
                }
            } else if (effect->kind == EFFECT_KIND_RISING_CLUMP || effect->kind == EFFECT_KIND_UNKE) {
                effect->unk3B = (effect->unk3B + 1) % CYCLING_FRAME_COUNT;
            } else {
                if (effect->velocity[AXIS_Y] < OTHER_EFFECT_FAST_RISE_SPEED) {
                    effect->unk3B = 0;
                } else if (effect->velocity[AXIS_Y] > OTHER_EFFECT_FAST_FALL_SPEED) {
                    effect->unk3B = 2;
                } else {
                    effect->unk3B = 1;
                }
                effect->unk3B = 0; /* Overwrites the frame picked above; the original has it */
            }
            if (IsPastSpawnGround(effect)
                && (effect->kind == EFFECT_KIND_DROP || effect->kind == EFFECT_KIND_DIVOT)) {
                effect->position[AXIS_Y] = (sSpawnGroundHeight + GROUNDED_EFFECT_DEPTH) * COURSE_TO_WORLD_SCALE;
            }
        }
        if (effect->kind == EFFECT_KIND_RIPPLE) {
            effect->matrixIndex = sNextMatrixIndex;
            translation[AXIS_X] = effect->position[AXIS_X];
            translation[AXIS_Y] = effect->position[AXIS_Y];
            translation[AXIS_Z] = effect->position[AXIS_Z];
            rotation[AXIS_X] = 0.0f;
            rotation[AXIS_Y] = 0;
            rotation[AXIS_Z] = 0.0f;
            scale[AXIS_X] = effect->scale;
            scale[AXIS_Y] = effect->scale;
            scale[AXIS_Z] = effect->scale;
            sNextMatrixIndex = (sNextMatrixIndex + 1) % EFFECT_COUNT;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(matrix, &sEffectMatrices[effect->matrixIndex]);
        }
        if (effect->kind == EFFECT_KIND_SPREADING_CIRCLE) {
            effect->matrixIndex = sNextMatrixIndex;
            translation[AXIS_X] = effect->position[AXIS_X];
            translation[AXIS_Y] = effect->position[AXIS_Y];
            translation[AXIS_Z] = effect->position[AXIS_Z];
            rotation[AXIS_X] = 0.0f;
            rotation[AXIS_Y] = 0;
            rotation[AXIS_Z] = 0.0f;
            scale[AXIS_X] = effect->scale;
            scale[AXIS_Y] = effect->scale;
            scale[AXIS_Z] = effect->scale;
            sNextMatrixIndex = (sNextMatrixIndex + 1) % EFFECT_COUNT;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(matrix, &sEffectMatrices[effect->matrixIndex]);
        }
        /* Fading or shrinking away does not remove divot marks, drops, divots or tears */
        if ((effect->alpha <= 0.0f || effect->scale < 1.0f) && effect->kind != EFFECT_KIND_DIVOT_MARK
            && effect->kind != EFFECT_KIND_DROP && effect->kind != EFFECT_KIND_DIVOT
            && effect->kind != EFFECT_KIND_TEAR) {
            effect->kind = EFFECT_KIND_NONE;
            effect->alpha = 0.0f;
        }
        if (effect->kind == EFFECT_KIND_DROP) {
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
            ConvertMatrixToFixed(matrix, &sDropMatrices[g_DrawBufferIndex]);
        }
    }
}

#define RIPPLE_IMAGE_SIZE 64
#define ARC_IMAGE_SIZE 64
/* Effects are drawn only where their texels' alpha is above these */
#define DIVOT_MARK_ALPHA_THRESHOLD 0x1E
#define BILLBOARD_ALPHA_THRESHOLD 0x1F
#define SPARKLE_ALPHA_THRESHOLD 0x3F
#define RISING_CLUMP_TINT GPACK_RGBA5551(32, 248, 32, 0)
#define UNKE_TINT GPACK_RGBA5551(0, 0, 0, 0)
#define SPARKLE_TINT GPACK_RGBA5551(255, 255, 0, 0)

void DrawBillboardEffect(Gfx **displayListHead, Effect *effect, u16 color);
void DrawEffectShadow(Gfx **displayListHead, Effect *effect);
void DrawDrop(Gfx **displayListHead, CameraMatrices *matrices, Effect *unusedEffect);
void DrawDivot(Gfx **displayListHead, Effect *effect);
void DrawDivotMark(Gfx **displayListHead);
void DrawRipple(Gfx **displayListHead, Effect *effect);
void DrawSpreadingCircle(Gfx **displayListHead, Effect *effect);

/*
 * Draws every effect, a kind at a time: divot marks, ripples, spreading circles, drops and divots
 * with their shadows, then the billboards. It sets the render state between the loops.
 */
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
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, DIVOT_MARK_ALPHA_THRESHOLD);
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &sEffects[index];
        if (effect->kind == EFFECT_KIND_DIVOT_MARK) {
            DrawDivotMark(&gfx);
        }
    }

    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
    gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPLoadTextureBlock_4b(gfx++, TEXTURE_ADDRESS(sRippleImage + IMAGE_HEADER_SIZE), G_IM_FMT_I, RIPPLE_IMAGE_SIZE,
                           RIPPLE_IMAGE_SIZE, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, 6, 6,
                           G_TX_NOLOD, G_TX_NOLOD);
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &sEffects[index];
        if (effect->kind == EFFECT_KIND_RIPPLE) {
            DrawRipple(&gfx, effect);
        }
    }

    gDPLoadTextureBlock_4b(gfx++, TEXTURE_ADDRESS(sArcImage + IMAGE_HEADER_SIZE), G_IM_FMT_I, ARC_IMAGE_SIZE,
                           ARC_IMAGE_SIZE, 0, G_TX_MIRROR | G_TX_WRAP, G_TX_MIRROR | G_TX_WRAP, 6, 6, G_TX_NOLOD,
                           G_TX_NOLOD);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &sEffects[index];
        if (effect->kind == EFFECT_KIND_SPREADING_CIRCLE) {
            DrawSpreadingCircle(&gfx, effect);
        }
    }

    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &sEffects[index];
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
        effect = &sEffects[index];
        if (effect->kind == EFFECT_KIND_DIVOT) {
            DrawEffectShadow(&gfx, effect);
            DrawDivot(&gfx, effect);
        }
    }

    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
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
        effect = &sEffects[index];
        if (effect->kind == EFFECT_KIND_UNK0 || effect->kind == EFFECT_KIND_UNK1 || effect->kind == EFFECT_KIND_UNK2) {
            DrawBillboardEffect(&gfx, effect, effect->unk38);
        }
    }

    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, BILLBOARD_ALPHA_THRESHOLD);
    gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, 0, TEXEL0, 0, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, 0,
                      TEXEL0, 0);
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &sEffects[index];
        if (effect->kind == EFFECT_KIND_UNK3 || effect->kind == EFFECT_KIND_UNK4) {
            DrawBillboardEffect(&gfx, effect, effect->unk38);
        }
    }

    /* Setting color before each group's commands (after one for sparkles) is there to match */
    color = RISING_CLUMP_TINT;
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, BILLBOARD_ALPHA_THRESHOLD);
    gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, 0, TEXEL0, 0, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, 0,
                      TEXEL0, 0);
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &sEffects[index];
        if (effect->kind == EFFECT_KIND_RISING_CLUMP) {
            DrawBillboardEffect(&gfx, effect, color);
        }
    }

    color = UNKE_TINT;
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, BILLBOARD_ALPHA_THRESHOLD);
    gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, 0, TEXEL0, 0, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, 0,
                      TEXEL0, 0);
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &sEffects[index];
        if (effect->kind == EFFECT_KIND_UNKE) {
            DrawBillboardEffect(&gfx, effect, color);
        }
    }

    gDPSetCombineLERP(gfx++, ENVIRONMENT, PRIMITIVE, TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, ENVIRONMENT, PRIMITIVE,
                      TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
    color = SPARKLE_TINT;
    gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0xFF, 0xFF);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, SPARKLE_ALPHA_THRESHOLD);
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &sEffects[index];
        if (effect->kind == EFFECT_KIND_SPARKLE) {
            DrawBillboardEffect(&gfx, effect, color);
        }
    }

    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, TEXEL0, PRIMITIVE, 0, TEXEL0, 0);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, TEXTURE_ADDRESS(sDropletImage + IMAGE_HEADER_SIZE));
    /* Reusing the sparkles' color for the tears is there to match */
    for (index = 0; index != EFFECT_COUNT; index++) {
        effect = &sEffects[index];
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

/* Effects are drawn only where they project to a depth below this */
#define EFFECT_NEAREST_DEPTH (-3.0f)
/* The rectangles step across this many texels; the smaller images are shifted down to fit */
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
/*
 * 1 << this is BILLBOARD_IMAGE_SIZE texels in s10.5, which the droplet's tile, shifted down by 2
 * and mirrored every 16 texels, draws flipped. Shifting instead of multiplying is there to
 * match.
 */
#define TEAR_MIRROR_S_SHIFT 11
#define PUFF_LIGHTENING 0x40

/*
 * Draws an effect as a screen-aligned rectangle with its image's frame, tinted by color (RGBA
 * 5551) and faded by its alpha. Kinds 0 and 1 are lightened.
 */
void DrawBillboardEffect(Gfx **displayListHead, Effect *effect, u16 color) {
    Gfx *gfx = *displayListHead;
    ScreenPoint center;
    s32 halfSize;
    s32 textureStep;
    s32 frameOffset;
    s32 sparkleStep;
    s32 red;
    s32 green;
    s32 blue;

    if (ProjectPoint(effect->position[AXIS_X], effect->position[AXIS_Y], effect->position[AXIS_Z], &center)
        < EFFECT_NEAREST_DEPTH) {
        halfSize = center.scaleX * effect->scale;
        /* Returning early here is there to match */
        if (effect->alpha == 0.0f) {
            *displayListHead = gfx;
            return;
        }
        if (halfSize >= 2) {
            frameOffset = effect->unk3B * BILLBOARD_FRAME_SIZE;
            gDPPipeSync(gfx++);
            gDPSetPrimDepth(gfx++, center.depth, 0);
            gDPPipeSync(gfx++);
            /* Texels per pixel, in s5.10 fixed point */
            textureStep = ((BILLBOARD_IMAGE_SIZE / 2) << 10) / halfSize;
            if (effect->kind == EFFECT_KIND_RISING_CLUMP) {
                gDPLoadTextureBlock(gfx++, TEXTURE_ADDRESS(sClumpImage + (frameOffset + IMAGE_HEADER_SIZE)),
                                    G_IM_FMT_I, G_IM_SIZ_8b, BILLBOARD_IMAGE_SIZE, BILLBOARD_IMAGE_SIZE, 0, G_TX_WRAP,
                                    G_TX_WRAP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
            } else if (effect->kind == EFFECT_KIND_SPARKLE) {
                s32 sparkleFrames[SPARKLE_STEP_COUNT] = { 2, 1, 0, 0, 1, 1, 2 };

                sparkleStep = effect->unk3B - SPARKLE_HIDDEN_FRAME_COUNT;
                if (effect->unk3B < SPARKLE_HIDDEN_FRAME_COUNT) {
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
                gDPLoadTextureBlock_4b(gfx++, TEXTURE_ADDRESS(sStarImage + (frameOffset + IMAGE_HEADER_SIZE)),
                                       G_IM_FMT_I, BILLBOARD_IMAGE_SIZE, BILLBOARD_IMAGE_SIZE, 0, G_TX_WRAP,
                                       G_TX_WRAP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
            } else if (effect->kind == EFFECT_KIND_TEAR) {
                if (effect->unk3B < TEAR_SECOND_FRAME_START) {
                    frameOffset = 0 * DROPLET_FRAME_SIZE;
                } else if (effect->unk3B < TEAR_THIRD_FRAME_START) {
                    frameOffset = 1 * DROPLET_FRAME_SIZE;
                } else {
                    frameOffset = 2 * DROPLET_FRAME_SIZE;
                }
                gDPLoadTextureBlock_4b(gfx++, TEXTURE_ADDRESS(sDropletImage + (frameOffset + PALETTED_TEXELS_OFFSET)),
                                       G_IM_FMT_CI, DROPLET_IMAGE_SIZE, DROPLET_IMAGE_SIZE, 0, G_TX_MIRROR,
                                       G_TX_WRAP, 4, 0, 2, 2);
            } else if (effect->kind == EFFECT_KIND_UNKE) {
                gDPLoadTextureBlock_4b(gfx++, TEXTURE_ADDRESS(sSpeckleImage + IMAGE_HEADER_SIZE), G_IM_FMT_I,
                                       SPECKLE_IMAGE_SIZE, SPECKLE_IMAGE_SIZE, 0, G_TX_WRAP, G_TX_WRAP, 6, 6, 1, 1);
            } else {
                gDPLoadTextureBlock(gfx++, TEXTURE_ADDRESS(sDebrisImage + (frameOffset + IMAGE_HEADER_SIZE)),
                                    G_IM_FMT_I, G_IM_SIZ_8b, BILLBOARD_IMAGE_SIZE, BILLBOARD_IMAGE_SIZE, 0, G_TX_WRAP,
                                    G_TX_WRAP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
            }
            gDPPipeSync(gfx++);
            red = (color >> 11) << 3;
            green = ((color >> 6) & 0x1F) << 3;
            blue = ((color >> 1) & 0x1F) << 3;
            if (effect->kind == EFFECT_KIND_UNK0 || effect->kind == EFFECT_KIND_UNK1) {
                red += PUFF_LIGHTENING;
                green += PUFF_LIGHTENING;
                blue += PUFF_LIGHTENING;
                if (red > 0xFF) {
                    red = 0xFF;
                }
                if (green > 0xFF) {
                    green = 0xFF;
                }
                if (blue > 0xFF) {
                    blue = 0xFF;
                }
            }
            gDPSetPrimColor(gfx++, 0, 0, red, green, blue, effect->alpha);
            gDPPipeSync(gfx++);
            /* Odd frames of a tear draw the droplet mirrored */
            if (effect->kind == EFFECT_KIND_TEAR) {
                gSPScisTextureRectangle(gfx++, (center.x - halfSize) << 2, (center.y - halfSize) << 2,
                                        (center.x + halfSize) << 2, (center.y + halfSize) << 2, G_TX_RENDERTILE,
                                        (effect->unk3B & 1) << TEAR_MIRROR_S_SHIFT, 0, textureStep, textureStep);
            } else {
                gSPScisTextureRectangle(gfx++, (center.x - halfSize) << 2, (center.y - halfSize) << 2,
                                        (center.x + halfSize) << 2, (center.y + halfSize) << 2, G_TX_RENDERTILE, 0,
                                        0, textureStep, textureStep);
            }
            gDPPipeSync(gfx++);
        }
    }
    *displayListHead = gfx;
}

/* Shadows are drawn only between these depths from ProjectPoint */
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
    if (depth < EFFECT_NEAREST_DEPTH) {
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

#define DROP_IMAGE_SIZE 16
#define DROP_GRAY 239

/* Draws the drop that LaunchDrop placed, turned to face the camera */
void DrawDrop(Gfx **displayListHead, CameraMatrices *matrices, Effect *unusedEffect) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(gfx++, 0, 0, DROP_GRAY, DROP_GRAY, DROP_GRAY, 255);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    /* The masks of 6 are wider than the image; the original has them */
    gDPLoadTextureBlock_4b(gfx++, TEXTURE_ADDRESS(sDropImage + IMAGE_HEADER_SIZE), G_IM_FMT_I, DROP_IMAGE_SIZE,
                           DROP_IMAGE_SIZE, 0, G_TX_CLAMP, G_TX_CLAMP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sDropMatrices[g_DrawBufferIndex]), G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&matrices->billboard), G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPVertex(gfx++, sUprightQuadVertices, 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

#define DIVOT_IMAGE_SIZE 32
#define DIVOT_FRAME_SIZE (DIVOT_IMAGE_SIZE * DIVOT_IMAGE_SIZE / 2)
/* The spin is cut into this many steps, each drawn with the frame in sPingPongFrameIndices */
#define DIVOT_SPIN_STEP_COUNT 12
#define DIVOT_DEPTH_BIAS 0.75f

/* Draws a flying divot as a screen-aligned rectangle, with the frame for its spin angle */
void DrawDivot(Gfx **displayListHead, Effect *effect) {
    Gfx *gfx = *displayListHead;
    ScreenPoint center;
    s32 halfSize;
    s32 textureStep;
    s32 spinStep;
    f32 angle;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, TEXTURE_ADDRESS(sDivotImage + IMAGE_HEADER_SIZE));
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    if (ProjectPoint(effect->position[AXIS_X], effect->position[AXIS_Y], effect->position[AXIS_Z], &center)
        < EFFECT_NEAREST_DEPTH) {
        halfSize = center.scaleX * effect->scale;
        if (halfSize > 0) {
            /* Texels per pixel, in s5.10 fixed point */
            textureStep = ((DIVOT_IMAGE_SIZE / 2) << 10) / halfSize;
            /* Half a step on, so that each frame is centered on its step */
            angle = effect->unk24 + M_PI / DIVOT_SPIN_STEP_COUNT;
            if (angle > 2 * M_PI) {
                angle -= 2 * M_PI;
            }
            if (angle < 0.0f) {
                angle += 2 * M_PI;
            }
            center.depth -= center.depthStep * DIVOT_DEPTH_BIAS;
            gDPSetPrimDepth(gfx++, center.depth, 0);
            spinStep = angle * DIVOT_SPIN_STEP_COUNT / (2 * M_PI);
            gDPLoadTextureBlock_4b(gfx++,
                                   TEXTURE_ADDRESS(sDivotImage
                                                   + (sPingPongFrameIndices[spinStep] * DIVOT_FRAME_SIZE
                                                      + PALETTED_TEXELS_OFFSET)),
                                   G_IM_FMT_CI, DIVOT_IMAGE_SIZE, DIVOT_IMAGE_SIZE, 0, G_TX_CLAMP, G_TX_CLAMP, 6, 6,
                                   G_TX_NOLOD, G_TX_NOLOD);
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(gfx++, (center.x - halfSize) << 2, (center.y - halfSize) << 2,
                                    (center.x + halfSize) << 2, (center.y + halfSize) << 2, G_TX_RENDERTILE, 0, 0,
                                    textureStep, textureStep);
            gDPPipeSync(gfx++);
        }
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

#define DIVOT_MARK_FRAME 4

/* Draws the divot's mark where SpawnShotDebris placed it, flat on the ground */
void DrawDivotMark(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, TEXTURE_ADDRESS(sDivotImage + IMAGE_HEADER_SIZE));
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    /* Grouping the frame's offset is there to match */
    gDPLoadTextureBlock_4b(gfx++,
                           TEXTURE_ADDRESS(sDivotImage
                                           + (DIVOT_MARK_FRAME * DIVOT_FRAME_SIZE + PALETTED_TEXELS_OFFSET)),
                           G_IM_FMT_CI, DIVOT_IMAGE_SIZE, DIVOT_IMAGE_SIZE, 0, G_TX_CLAMP, G_TX_CLAMP, 6, 6, G_TX_NOLOD,
                           G_TX_NOLOD);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sDivotMarkMatrix), G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPVertex(gfx++, sSmallFlatQuadVertices, 4, 0);
    gDPPipeSync(gfx++);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

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

/* Rumbles the player's controller briefly if it has a Rumble Pak, unless g_GameState.unk31 is set */
void StartPlayerRumble(UnkB8 *player) {
    if (player->controllerIndex < MAXCONTROLLERS && !g_GameState.unk31) {
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
    if (player->controllerIndex < MAXCONTROLLERS && !g_GameState.unk31) {
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
    drop->groundHeight = 0;
    drop->flags = 0;
    drop->landedFrameCount = RAINDROP_FALLING;
    drop->splashColumn = 0;
    drop->tailScale = 1.0f;
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
    splash->column = 0;
}

/* Sets the rain's path from the wind, and places every raindrop and splash */
void StartRain(void) {
    f32 direction;
    f32 speed;
    s32 index;

    sGroundCheckDropIndex = 0;
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
        sRaindrops[index].flags = 0;
    }
    for (index = 0; index != RAIN_SPLASH_COUNT; index++) {
        ResetRainSplash(&sRainSplashes[index]);
        sRainSplashes[index].unkC = 1.0f;
        sRainSplashes[index].frameCount = index % 9 - 1;
        sRainSplashes[index].unk17 = 0;
    }
}

#define RAIN_SOUND_STEADY_VOLUME 80.0f
#define RAIN_SOUND_FADE_STEP 0.5f
/* A camera that moves farther than this across the ground in a frame takes the rain with it */
#define RAIN_CAMERA_JUMP_DISTANCE 15.0f
#define RAINDROP_LANDED_FRAME_COUNT 8
#define RAIN_SPLASH_FRAME_COUNT 8

/*
 * Moves the rain on while it rains: fades the rain sound down, follows the wind and the camera,
 * restarts each splash after RAIN_SPLASH_FRAME_COUNT frames, and drops each raindrop until it
 * reaches the ground, then restarts it RAINDROP_LANDED_FRAME_COUNT frames later.
 */
void UpdateRain(void) {
    Vec3 cameraShift = { 0.0f, 0.0f, 0.0f };
    f32 direction;
    f32 speed;
    f32 shiftX;
    f32 shiftZ;
    s32 index;
    Raindrop *drop;
    RainSplash *splash;

    if (g_HoleConditions.isRaining == TRUE && sWeather == WEATHER_RAIN) {
        if (g_RainSoundVolume > RAIN_SOUND_STEADY_VOLUME) {
            g_RainSoundVolume -= RAIN_SOUND_FADE_STEP;
        }
        SetSoundVolume(RAIN_SOUND_CHANNEL, g_RainSoundVolume);
        direction = g_HoleConditions.windDirection * WIND_DIRECTION_TO_RADIANS;
        speed = g_HoleConditions.windSpeed / WIND_SPEED_UNIT * RAIN_DRIFT_PER_WIND_SPEED;
        sRainDriftX = cosf(direction) * speed;
        sRainDriftZ = sinf(direction) * speed;
        sRainVelocity[AXIS_Y] = RAIN_FALL_SPEED;
        sRainVelocity[AXIS_X] = sRainDriftX * RAIN_FALL_SPEED;
        sRainVelocity[AXIS_Z] = sRainDriftZ * RAIN_FALL_SPEED;
        PlaceRainCenter();
        shiftX = g_Cameras[0].view.eye[AXIS_X] - sLastRainEye[AXIS_X];
        shiftZ = g_Cameras[0].view.eye[AXIS_Z] - sLastRainEye[AXIS_Z];
        if (sqrtf(shiftX * shiftX + shiftZ * shiftZ) > RAIN_CAMERA_JUMP_DISTANCE) {
            cameraShift[AXIS_X] += shiftX;
            cameraShift[AXIS_Z] += shiftZ;
        } else if (sLastRainEye[AXIS_Y] < g_Cameras[0].view.eye[AXIS_Y]) {
            cameraShift[AXIS_Y] = g_Cameras[0].view.eye[AXIS_Y] - sLastRainEye[AXIS_Y];
        }
        sLastRainEye[AXIS_X] = g_Cameras[0].view.eye[AXIS_X];
        sLastRainEye[AXIS_Y] = g_Cameras[0].view.eye[AXIS_Y];
        sLastRainEye[AXIS_Z] = g_Cameras[0].view.eye[AXIS_Z];
        /* Indexing the drop each time is there to match */
        sRaindrops[sGroundCheckDropIndex].groundHeight =
            GetGroundHeight(sRaindrops[sGroundCheckDropIndex].head[AXIS_X] * WORLD_TO_COURSE_SCALE,
                            sRaindrops[sGroundCheckDropIndex].head[AXIS_Z] * WORLD_TO_COURSE_SCALE)
            * COURSE_TO_WORLD_SCALE;
        for (index = 0; index != sRainSplashCount; index++) {
            splash = &sRainSplashes[index];
            splash->frameCount++;
            if (splash->frameCount == RAIN_SPLASH_FRAME_COUNT) {
                ResetRainSplash(&sRainSplashes[index]);
                splash->frameCount = 0;
            }
        }
        for (index = 0; index != sRaindropCount; index++) {
            drop = &sRaindrops[index];
            if (drop->head[AXIS_Y] < drop->groundHeight) {
                drop->head[AXIS_X] += sRainVelocity[AXIS_X];
                drop->head[AXIS_Y] += sRainVelocity[AXIS_Y];
                drop->head[AXIS_Z] += sRainVelocity[AXIS_Z];
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
                drop->tail[AXIS_X] += sRainVelocity[AXIS_X] + cameraShift[AXIS_X];
                drop->tail[AXIS_Y] += sRainVelocity[AXIS_Y] + cameraShift[AXIS_Y];
                drop->tail[AXIS_Z] += sRainVelocity[AXIS_Z] + cameraShift[AXIS_Z];
                if (drop->tail[AXIS_Y] > drop->groundHeight) {
                    drop->landedFrameCount = 0;
                    drop->tail[AXIS_X] = drop->head[AXIS_X] + (drop->groundHeight - drop->head[AXIS_Y]) * sRainDriftX;
                    drop->tail[AXIS_Y] = drop->head[AXIS_Y] + (drop->groundHeight - drop->head[AXIS_Y]);
                    drop->tail[AXIS_Z] = drop->head[AXIS_Z] + (drop->groundHeight - drop->head[AXIS_Y]) * sRainDriftZ;
                }
            } else {
                drop->landedFrameCount++;
            }
            if (drop->landedFrameCount == RAINDROP_LANDED_FRAME_COUNT) {
                ResetRaindrop(drop);
            }
        }
        sGroundCheckDropIndex = (sGroundCheckDropIndex + 1) % sRaindropCount;
    }
}

/* ProjectPoint depths that rain is drawn at, nearest first */
#define RAINDROP_NEAREST_DEPTH (-15.0f)
#define RAIN_SPLASH_NEAREST_DEPTH (-7.5f)
/* Screen coordinates in display lists are in quarter pixels */
#define QUARTER_PIXELS_PER_PIXEL 4
/* How far a streak's second edge is offset from its first, in quarter pixels */
#define RAINDROP_WIDTH 5.0f
#define RAIN_ALPHA_THRESHOLD 1
#define RAIN_SPLASH_IMAGE_WIDTH 32
#define RAIN_SPLASH_IMAGE_HEIGHT 64
/* The splash image holds columns of square frames */
#define RAIN_SPLASH_FRAME_SIZE 16
/* Shifts a frame index to its texture coordinate, in 10.5 fixed point */
#define RAIN_SPLASH_FRAME_SHIFT 9
/* Each splash frame shows for this many frames */
#define RAIN_SPLASH_FRAME_DURATION 2
#define RAIN_SPLASH_GRAY 0xBF
#define RAIN_SPLASH_ALPHA 0x9F
/* Splash half sizes, in quarter pixels for each unit of projected scale */
#define RAIN_SPLASH_HALF_SIZE_PER_SCALE 8.0f
#define LANDED_DROP_SMALLEST_HALF_SIZE 4.0f
#define RAIN_SPLASH_SMALLEST_HALF_SIZE 1.0f
#define RAIN_SPLASH_LARGEST_HALF_SIZE 16

/*
 * Updates the rain and draws it while it rains, unless the main camera is in mode 2 or 8. Falling
 * drops are drawn as streaks between their projected head and tail, and landed drops and splashes
 * as frames of sRainSplashImage.
 */
void DrawRain(Gfx **displayListHead, CameraMatrices *matrices) {
    ScreenPoint point;
    ScreenPoint splashPoint;
    Gfx *gfx = *displayListHead;
    s32 index;
    /* Separate variables for each loop are there to match */
    Raindrop *drop;
    Raindrop *streak;
    Raindrop *landedDrop;
    RainSplash *splash;
    f32 lengthX; /* Then the streak's widening along y */
    f32 lengthY; /* Then the streak's widening along x */
    f32 widthScale;
    s32 halfSize;
    s32 splashSize;
    s32 splashStep;
    s32 textureStep;

    if (g_HoleConditions.isRaining == TRUE && sWeather == WEATHER_RAIN
        /* Comparing with FALSE is there to match */
        && (g_Cameras[0].view.mode == CAMERA_MODE_UNK2 || g_Cameras[0].view.mode == CAMERA_MODE_UNK8) == FALSE) {
        UpdateRain();
        for (index = 0; index != sRaindropCount; index++) {
            drop = &sRaindrops[index];
            if (ProjectPoint(drop->tail[AXIS_X], drop->tail[AXIS_Y], drop->tail[AXIS_Z], &point)
                < RAINDROP_NEAREST_DEPTH) {
                drop->tailScreenX = point.x * QUARTER_PIXELS_PER_PIXEL;
                drop->tailScreenY = point.y * QUARTER_PIXELS_PER_PIXEL;
                /* Storing the depth before the flags is there to match */
                drop->tailDepth = point.depth;
                drop->flags &= ~RAINDROP_OFF_SCREEN;
                drop->tailScale = point.scaleX;
                if (!(drop->flags & RAINDROP_HEAD_GROUNDED)) {
                    if (ProjectPointToScreen(drop->head[AXIS_X], drop->head[AXIS_Y], drop->head[AXIS_Z], &point)
                        < RAINDROP_NEAREST_DEPTH) {
                        drop->headScreenX = point.x * QUARTER_PIXELS_PER_PIXEL;
                        drop->headScreenY = point.y * QUARTER_PIXELS_PER_PIXEL;
                    } else {
                        drop->flags |= RAINDROP_OFF_SCREEN;
                    }
                }
            } else {
                drop->flags |= RAINDROP_OFF_SCREEN;
            }
        }
        guMtxIdent(&sRainModelViewMatrix);
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
        gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
        gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sRainModelViewMatrix), G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPVertex(gfx++, sRainStreakVertices, ARRAY_COUNT(sRainStreakVertices), 0);
        for (index = 0; index != sRaindropCount; index++) {
            streak = &sRaindrops[index];
            /* On screen, with the head still falling */
            if (streak->flags == 0) {
                lengthX = streak->headScreenX - streak->tailScreenX;
                lengthY = streak->headScreenY - streak->tailScreenY;
                if (lengthX == 0.0f && lengthY == 0.0f) {
                    /* The empty branch is there to match */
                } else {
                    widthScale = RAINDROP_WIDTH / sqrtf(lengthX * lengthX + lengthY * lengthY);
                    gDPPipeSync(gfx++);
                    gDPSetPrimDepth(gfx++, streak->tailDepth, 0);
                    gDPPipeSync(gfx++);
                    /* Scaling the lengths in place is there to match */
                    lengthY *= widthScale;
                    gSPModifyVertex(gfx++, 0, G_MWO_POINT_XYSCREEN,
                                    ((streak->headScreenX + (s32)lengthY) << 16) + streak->headScreenY);
                    lengthX *= widthScale;
                    gSPModifyVertex(gfx++, 1, G_MWO_POINT_XYSCREEN,
                                    (streak->headScreenX << 16) + (streak->headScreenY + (s32)lengthX));
                    gSPModifyVertex(gfx++, 2, G_MWO_POINT_XYSCREEN,
                                    ((streak->tailScreenX + (s32)lengthY) << 16) + streak->tailScreenY);
                    gSPModifyVertex(gfx++, 3, G_MWO_POINT_XYSCREEN,
                                    (streak->tailScreenX << 16) + (streak->tailScreenY + (s32)lengthX));
                    gSP2Triangles(gfx++, 1, 0, 2, 0, 1, 2, 3, 0);
                }
            }
        }
        gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_NONE);
        gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE,
                          0);
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
        gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, RAIN_ALPHA_THRESHOLD);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gDPLoadTextureBlock_4b(gfx++, TEXTURE_ADDRESS(sRainSplashImage + IMAGE_HEADER_SIZE), G_IM_FMT_I,
                               RAIN_SPLASH_IMAGE_WIDTH, RAIN_SPLASH_IMAGE_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                               G_TX_NOMIRROR | G_TX_WRAP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
        gDPSetPrimColor(gfx++, 0, 0, RAIN_SPLASH_GRAY, RAIN_SPLASH_GRAY, RAIN_SPLASH_GRAY, RAIN_SPLASH_ALPHA);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        for (index = 0; index != sRaindropCount; index++) {
            landedDrop = &sRaindrops[index];
            if (landedDrop->landedFrameCount != RAINDROP_FALLING && !(landedDrop->flags & RAINDROP_OFF_SCREEN)) {
                gDPPipeSync(gfx++);
                gDPSetPrimDepth(gfx++, landedDrop->tailDepth, 0);
                gDPPipeSync(gfx++);
                halfSize = landedDrop->tailScale * RAIN_SPLASH_HALF_SIZE_PER_SCALE;
                if (halfSize >= LANDED_DROP_SMALLEST_HALF_SIZE) {
                    if (halfSize > (f32)RAIN_SPLASH_LARGEST_HALF_SIZE) {
                        halfSize = RAIN_SPLASH_LARGEST_HALF_SIZE;
                    }
                    textureStep = ((RAIN_SPLASH_FRAME_SIZE * 2) << 10) / halfSize;
                    gSPScisTextureRectangle(gfx++, landedDrop->tailScreenX - halfSize,
                                            landedDrop->tailScreenY - halfSize, landedDrop->tailScreenX + halfSize,
                                            landedDrop->tailScreenY + halfSize, G_TX_RENDERTILE,
                                            landedDrop->splashColumn << RAIN_SPLASH_FRAME_SHIFT,
                                            (landedDrop->landedFrameCount / RAIN_SPLASH_FRAME_DURATION)
                                                << RAIN_SPLASH_FRAME_SHIFT,
                                            textureStep, textureStep);
                }
            }
        }
        for (index = 0; index != sRainSplashCount; index++) {
            splash = &sRainSplashes[index];
            if (ProjectPointToScreen(splash->position[AXIS_X], splash->position[AXIS_Y], splash->position[AXIS_Z],
                                     &splashPoint)
                < RAIN_SPLASH_NEAREST_DEPTH) {
                splashSize = splashPoint.scaleX * RAIN_SPLASH_HALF_SIZE_PER_SCALE;
                if (splashSize >= RAIN_SPLASH_SMALLEST_HALF_SIZE) {
                    if (splashSize > (f32)RAIN_SPLASH_LARGEST_HALF_SIZE) {
                        splashSize = RAIN_SPLASH_LARGEST_HALF_SIZE;
                    }
                    splashStep = ((RAIN_SPLASH_FRAME_SIZE * 2) << 10) / splashSize;
                    gSPScisTextureRectangle(gfx++, splashPoint.x * QUARTER_PIXELS_PER_PIXEL - splashSize,
                                            splashPoint.y * QUARTER_PIXELS_PER_PIXEL - splashSize,
                                            splashPoint.x * QUARTER_PIXELS_PER_PIXEL + splashSize,
                                            splashPoint.y * QUARTER_PIXELS_PER_PIXEL + splashSize, G_TX_RENDERTILE,
                                            splash->column << RAIN_SPLASH_FRAME_SHIFT,
                                            (splash->frameCount / RAIN_SPLASH_FRAME_DURATION)
                                                << RAIN_SPLASH_FRAME_SHIFT,
                                            splashStep, splashStep);
                }
            }
        }
        gDPPipeSync(gfx++);
        gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&matrices->projection), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
        gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
        gDPPipeSync(gfx++);
        gDPSetDepthSource(gfx++, G_ZS_PIXEL);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gDPPipeSync(gfx++);
        *displayListHead = gfx;
    }
}

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

#define BADGE_SIZE 24
#define BADGE_FRAME_SIZE (BADGE_SIZE * BADGE_SIZE / 2)
#define BADGE_SPIN_FRAME_COUNT 6
#define BADGE_LEFT 216
#define BADGE_RESTING_TOP 20
#define BADGE_COUNT_LEFT 258
#define BADGE_COUNT_TOP 24
#define BADGE_COUNT_SHOWN_FRAME_COUNT 60
#define BADGE_COUNT_RISE_FRAME 40
#define BADGE_COUNT_ADD_FRAME 15
#define BADGE_COUNT_SOUND 0x6B
#define COUNT_GLYPHS_HEIGHT 256

/*
 * Drops the spinning badge and its count in at the top right, adds the new badge to the count,
 * then lifts them out of view. ShowBirdieBadgeCount loaded their images in place of the cloud
 * and clump images.
 */
void DrawBirdieBadgeCount(Gfx **displayListHead) {
    Gfx *gfx;
    s32 spinFrame;
    s32 offsetY;

    gfx = *displayListHead;
    if (sIsBadgeCountShown) {
        gSPDisplayList(gfx++, D_800C3FC0);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_POINT);
        spinFrame = sBadgeCountFrame / 2 % BADGE_SPIN_FRAME_COUNT;
        if (sBadgeCountFrame < BADGE_COUNT_RISE_FRAME) {
            offsetY = sBadgeCountFrame * 4 - 24;
            if (offsetY > 0) {
                offsetY = 0;
            }
        } else {
            offsetY = (BADGE_COUNT_RISE_FRAME - sBadgeCountFrame) * 4;
            if (offsetY < -100) {
                offsetY = -100;
            }
        }
        LoadSmallPalette(&gfx, 0, sCloudImage + IMAGE_HEADER_SIZE);
        /* Grouping the frame's offset is there to match */
        gDPLoadTextureTile_4b(gfx++,
                              TEXTURE_ADDRESS(sCloudImage + (spinFrame * BADGE_FRAME_SIZE + PALETTED_TEXELS_OFFSET)),
                              G_IM_FMT_CI, BADGE_SIZE, BADGE_SIZE, 0, 0, BADGE_SIZE - 1, BADGE_SIZE - 1, 0, 0, 0, 0,
                              0, 0, 0);
        DrawTextureRect(&gfx, BADGE_LEFT << 2, (offsetY + BADGE_RESTING_TOP) << 2, (BADGE_LEFT + BADGE_SIZE) << 2,
                        (offsetY + BADGE_RESTING_TOP + BADGE_SIZE) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        LoadSmallPalette(&gfx, 0, sClumpImage + IMAGE_HEADER_SIZE);
        LoadNibbleTextureBlock(&gfx, sClumpImage + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI, 16, COUNT_GLYPHS_HEIGHT, 0,
                               0, 0, 0, 0, 0, 0);
        DrawDigitText(&gfx, BADGE_LEFT + BADGE_SIZE, offsetY + BADGE_COUNT_TOP, "*");
        sprintf(g_PrintBuffer, "%d", sShownBadgeCount);
        DrawDigitText(&gfx, BADGE_COUNT_LEFT, offsetY + BADGE_COUNT_TOP, g_PrintBuffer);
        sBadgeCountFrame++;
        if (sBadgeCountFrame == BADGE_COUNT_ADD_FRAME) {
            sShownBadgeCount++;
            PlaySound(BADGE_COUNT_SOUND, g_NextSoundChannel, 100);
        }
        if (sBadgeCountFrame == BADGE_COUNT_SHOWN_FRAME_COUNT) {
            sBadgeCountFrame = 0;
            sIsBadgeCountShown = FALSE;
        }
        *displayListHead = gfx;
    }
}

