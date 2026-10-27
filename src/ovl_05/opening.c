#include "common.h"
#include "asset.h"
#include "camera.h"
#include "character_slot.h"
#include "sound.h"
#include "variables.h"

/* The view that the opening draws its characters through */
#define OPENING_VIEW_INDEX 2

extern s32 D_8020AC4C_ovl_05;
extern s32 D_8020AC50_ovl_05;
extern f32 D_8020AC5C_ovl_05;
extern s32 D_8020AC60_ovl_05;
extern u8 D_80212830_ovl_05[];
extern u8 D_80213A40_ovl_05[];

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

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801F5018_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801F5100_ovl_05);

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

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FBB10_ovl_05);

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
