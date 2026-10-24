#include "common.h"
#include "asset.h"
#include "character_slot.h"
#include "heap.h"

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

extern s32 sAreEffectImagesLoaded;
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

INCLUDE_ASM("asm/nonmatchings/effects", func_80078DC0);

INCLUDE_ASM("asm/nonmatchings/effects", func_80078FA8);

INCLUDE_ASM("asm/nonmatchings/effects", func_80079358);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007955C);

INCLUDE_ASM("asm/nonmatchings/effects", func_800796F8);

INCLUDE_ASM("asm/nonmatchings/effects", func_80079940);

INCLUDE_ASM("asm/nonmatchings/effects", func_80079A08);

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

INCLUDE_ASM("asm/nonmatchings/effects", func_8007DE9C);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007DFD0);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007E104);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007E19C);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007E234);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007E2B0);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007E30C);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007E438);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007E664);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007E7B8);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007E980);
INCLUDE_RODATA("asm/nonmatchings/effects", D_800D1A88);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007EF0C);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007FE44);

INCLUDE_ASM("asm/nonmatchings/effects", func_8007FEAC);

