#include "common.h"
#include "asset.h"
#include "billboard.h"
#include "camera.h"
#include "character_slot.h"
#include "effects.h"
#include "functions.h"
#include "heap.h"
#include "sound.h"
#include "variables.h"

/* The view that the opening draws its characters through */
#define OPENING_VIEW_INDEX 2

/* The size of the heap that the opening's billboards are allocated from */
#define OPENING_BILLBOARD_HEAP_SIZE 0x7038

#define ASSET_SHADOW_TEXTURE 0x4C3

/* The face-printed golf balls that the opening shows, two at a time */
#define OPENING_BALL_IMAGE_COUNT 5
#define OPENING_BALL_SLOT_COUNT 2
#define OPENING_BALL_IMAGE_SIZE 0x1208 /* A 64x64 CI8 image with its header and palette */

extern s32 D_8020AC4C_ovl_05;
extern s32 D_8020AC50_ovl_05;
extern f32 D_8020AC5C_ovl_05;
extern s32 D_8020AC60_ovl_05;
extern s32 D_8020AC64_ovl_05;
extern u8 D_80212830_ovl_05[];
extern u8 D_80213A40_ovl_05[];
extern s32 D_8020AD70_ovl_05[OPENING_BALL_SLOT_COUNT];
extern f32 D_8020AD80_ovl_05[OPENING_BALL_SLOT_COUNT];
extern s32 sOpeningBallAssets[OPENING_BALL_IMAGE_COUNT];
extern u8 D_80214C50_ovl_05[OPENING_BALL_SLOT_COUNT][OPENING_BALL_IMAGE_SIZE];
/* The buffers of character slots 1 and 2 */
extern u8 *sOpeningCharacterBuffers[2];
extern void *sOpeningBillboardHeap;
extern s32 D_8022BFA8_ovl_05;
extern u8 *g_ShadowTexture;

void AssignCharacterBuffers(void);
void func_80200B18_ovl_05(void);

void func_801F4A40_ovl_05(void) {
    D_8020AC4C_ovl_05++;
    if (D_8020AC4C_ovl_05 == 11) {
        D_8020AC4C_ovl_05 = 0;
    }
}

void func_801F4A6C_ovl_05(void) {
    if (D_8020AC60_ovl_05 != 0) {
        PlaySoundWithEffects(0x51, 12, 48 - (s32)(D_8020AC5C_ovl_05 * 0.2f), 0, 127 - (D_8020AC50_ovl_05 - 160) / 2);
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801F4AEC_ovl_05);

void LoadOpening(void) {
    EncodedAsset asset;
    s32 bufferIndex;

    osSyncPrintf("ConstOpening\n");
    sOpeningBillboardHeap = AllocMemory(OPENING_BILLBOARD_HEAP_SIZE);
    InitBillboardManager(sOpeningBillboardHeap, OPENING_BILLBOARD_HEAP_SIZE);
    D_8022BFA8_ovl_05 = 31;
    for (bufferIndex = 0; bufferIndex < ARRAY_COUNT(sOpeningCharacterBuffers); bufferIndex++) {
        sOpeningCharacterBuffers[bufferIndex] = AllocMemory(GetCharacterBufferSize());
        SetCharacterBuffer(bufferIndex + 1, sOpeningCharacterBuffers[bufferIndex]);
    }
    SetCharacterBuffer(3, (u8 *)g_DepthBuffer);
    g_ShadowTexture = AllocMemory(OpenEncodedAsset(ASSET_SHADOW_TEXTURE, &asset));
    ReadEncodedAsset(g_ShadowTexture, &asset);
    D_8020AC64_ovl_05 = 0;
    LoadEffectImages();
    func_80200B18_ovl_05();
}

void FreeOpening(void) {
    s32 bufferIndex;

    FreeMemory(&g_ShadowTexture);
    for (bufferIndex = 0; bufferIndex < ARRAY_COUNT(sOpeningCharacterBuffers); bufferIndex++) {
        FreeMemory(&sOpeningCharacterBuffers[bufferIndex]);
    }
    AssignCharacterBuffers();
    FreeMemory(&sOpeningBillboardHeap);
    SetFillColor(255, 255, 255);
    FreeEffectImages();
}

void DrawOpeningCharacters(CameraMatrices *matrices, Gfx **displayListHead) {
    s32 slotIndex = 0;

    g_DisplayListHead = *displayListHead;
    do {
        DrawCharacterAndShadow(matrices, OPENING_VIEW_INDEX, slotIndex);
        slotIndex++;
    } while (slotIndex < CHARACTER_SLOT_COUNT);
    *displayListHead = g_DisplayListHead;
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801F51F0_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801F5454_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801F58EC_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801F6788_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FA4EC_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FA83C_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FA8D8_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FB864_ovl_05);

/* Loads ball image ballIndex into slot and resets the slot */
void func_801FBB10_ovl_05(s32 ballIndex, s32 slot) {
    EncodedAsset asset;

    OpenEncodedAsset(sOpeningBallAssets[ballIndex], &asset);
    ReadEncodedAsset(D_80214C50_ovl_05[slot], &asset);
    D_8020AD70_ovl_05[slot] = 0;
    D_8020AD80_ovl_05[slot] = 1.0f;
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FBB94_ovl_05);

void func_801FE270_ovl_05(void) {
    EncodedAsset asset;

    OpenEncodedAsset(0x908, &asset);
    ReadEncodedAsset(D_80212830_ovl_05, &asset);
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FE2A0_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FE7B4_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FE8F8_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FF5E8_ovl_05);

void func_801FF6DC_ovl_05(void) {
    EncodedAsset asset;

    OpenEncodedAsset(0x93D, &asset);
    ReadEncodedAsset(D_80212830_ovl_05, &asset);
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FF70C_ovl_05);

void func_801FFC10_ovl_05(void) {
    EncodedAsset asset;

    OpenEncodedAsset(0x939, &asset);
    ReadEncodedAsset(D_80213A40_ovl_05, &asset);
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FFC40_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_802008D8_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_80200B18_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_80200CBC_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_80202D34_ovl_05);
