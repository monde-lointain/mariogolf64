#include "common.h"
#include "asset.h"
#include "character_slot.h"
#include "controller.h"
#include "cpu_projection.h"
#include "functions.h"
#include "heap.h"
#include "math_util.h"
#include "save.h"
#include "sound.h"
#include <lib/libnusys/nusys.h>

/* The particle effects drawn in the scene, such as the sparkles of a passed ring */
typedef struct Effect {
    /* 0x00 */ Vec3 position; /* In world units */
    /* 0x0C */ Vec3 velocity; /* In world units a frame */
    /* 0x18 */ u8 unk18[0x10];
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

#define EFFECT_KIND_NONE (-1)
#define EFFECT_KIND_SPARKLE 8
#define EFFECT_KIND_RIPPLE 0xC
#define EFFECT_KIND_UNKE 0xE
#define EFFECT_KIND_SPREADING_CIRCLE 0x13 /* Drawn with sArcImage */
#define EFFECT_KIND_TEAR 0x14

#define WEATHER_NONE (-1)
#define WEATHER_RAIN 2

/* Each holds an image asset: a header, then texels */
extern u8 *sRippleImage;
extern u8 *sArcImage;
extern u8 *sDropImage;
extern u8 *sSparkImage;
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
extern s32 sSpawnGroundHeight;
extern s8 D_801B60B9;
extern u8 *sRainSplashImage;
extern void *D_801EFFA8;
extern void *D_801B8BB8;

s32 D_800C52B0 = 0;
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
s32 D_800C5400 = 48;
s32 D_800C5404 = 32;
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
void func_8007E7B8(void);

#define RIPPLE_IMAGE_ASSET 0x6B0
#define DROP_IMAGE_ASSET 0x6B2
#define CLOUD_IMAGE_ASSET 0x6B5
#define CLUMP_IMAGE_ASSET 0x6B8
#define SPARK_IMAGE_ASSET 0x6C0
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
        FreeMemory(&sSparkImage);
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
        sSparkImage = AllocMemory(OpenEncodedAsset(SPARK_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sSparkImage, &asset);
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

INCLUDE_ASM("asm/nonmatchings/effects", func_80078BDC);

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

INCLUDE_ASM("asm/nonmatchings/effects", func_800796F8);

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

INCLUDE_ASM("asm/nonmatchings/effects", func_80079EBC);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007A10C);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007A40C);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007A6C8);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007B054);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007B994);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007C5D8);
INCLUDE_RODATA("asm/nonmatchings/effects", D_800D1A40);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007CF10);

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
#define UNK801EFFA8_BUFFER_SIZE 0x900
#define UNK801B8BB8_BUFFER_SIZE 0x380
#define RAIN_SOUND_CHANNEL 9

/* Sets the weather, and for rain loads its image and sets up its buffers */
void StartWeather(s32 weather) {
    EncodedAsset asset;

    if (weather == WEATHER_RAIN) {
        sRainSplashImage = AllocMemory(OpenEncodedAsset(RAIN_SPLASH_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sRainSplashImage, &asset);
        D_801EFFA8 = AllocMemory(UNK801EFFA8_BUFFER_SIZE);
        D_801B8BB8 = AllocMemory(UNK801B8BB8_BUFFER_SIZE);
        func_8007E7B8();
    }
    sWeather = weather;
}

/* Frees what StartWeather loaded and stops the rain's sound */
void EndWeather(void) {
    if (sWeather == WEATHER_RAIN) {
        FreeMemory(&sRainSplashImage);
        FreeMemory(&D_801EFFA8);
        FreeMemory(&D_801B8BB8);
    }
    StopSound(RAIN_SOUND_CHANNEL);
    sWeather = WEATHER_NONE;
}

INCLUDE_ASM("asm/nonmatchings/effects", func_8007E30C);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007E438);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007E664);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007E7B8);

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

