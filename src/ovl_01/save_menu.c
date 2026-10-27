#include "common.h"
#include "asset.h"
#include "menu_memory.h"
#include "save.h"

/* The image listing the game modes, in orange */
#define ASSET_MENU_GAME_MODES 0x528

extern s32 D_80230A90_ovl_01;
extern s32 D_80230A94_ovl_01;
extern s32 D_80230A98_ovl_01;
extern void *D_8023B670_ovl_01;
extern void *D_8023B674_ovl_01;
extern void *D_8023B678_ovl_01;
extern void *D_8023B67C_ovl_01;
/* The slots that the save menu steps through, which hold -1 when empty */
#define SAVE_MENU_SLOT_COUNT 5

extern s16 D_8023B684_ovl_01[SAVE_MENU_SLOT_COUNT];
extern void *D_8023BD78_ovl_01;
extern void *D_8023BD7C_ovl_01;

void func_8021F5F0_ovl_01(void *buffer, s16 bufferKind) {
    EncodedAsset asset;

    switch (bufferKind) {
        case 1:
            D_8023B670_ovl_01 = buffer;
            break;
        case 2:
            D_8023B674_ovl_01 = buffer;
            OpenEncodedAsset(0x5F7, &asset);
            ReadEncodedAsset(D_8023B674_ovl_01, &asset);
            break;
        case 3:
            D_8023B678_ovl_01 = buffer;
            OpenEncodedAsset(0x70E, &asset);
            ReadEncodedAsset(D_8023B678_ovl_01, &asset);
            break;
        case 4:
            D_8023B67C_ovl_01 = buffer;
            OpenEncodedAsset(0x720, &asset);
            ReadEncodedAsset(D_8023B67C_ovl_01, &asset);
            D_80230A90_ovl_01 = 0;
            D_80230A94_ovl_01 = 0;
            D_80230A98_ovl_01 = 0;
            break;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_8021F6E0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_8021F79C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_8021FC5C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80220A30_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80220BB0_ovl_01);

s16 func_80220D68_ovl_01(s8 startSlot, u8 isForward) {
    s32 slot = startSlot;
    s32 step;
    s32 tryCount;

    if (slot < 0) {
        return -1;
    }
    if (slot >= SAVE_MENU_SLOT_COUNT) {
        return -1;
    }
    step = isForward ? 1 : -1;
    slot += step;
    for (tryCount = 0; tryCount != SAVE_MENU_SLOT_COUNT - 1; tryCount++) {
        if (slot >= SAVE_MENU_SLOT_COUNT) {
            slot = 0;
        }
        if (slot < 0) {
            slot = SAVE_MENU_SLOT_COUNT - 1;
        }
        if (D_8023B684_ovl_01[slot] != -1) {
            break;
        }
        slot += step;
    }
    if (tryCount == SAVE_MENU_SLOT_COUNT - 1) {
        return -1;
    }
    return slot;
}

s16 func_80220DF8_ovl_01(void) {
    return D_8023B684_ovl_01[func_8005AF74()->unk42];
}

s32 func_80220E28_ovl_01(Unk68 *record) {
    return (u8)record->unk36 >> 7;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80220E34_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_802213C8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_8022181C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_802218C0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80221B20_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80221CC0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80221EBC_ovl_01);

void func_80222038_ovl_01(void *buffer, u8 bufferIndex) {
    EncodedAsset asset;

    switch (bufferIndex) {
        case 1:
            D_8023BD78_ovl_01 = buffer;
            break;
        case 2:
            D_8023BD7C_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_MENU_GAME_MODES, &asset);
            ReadEncodedAsset(D_8023BD7C_ovl_01, &asset);
            break;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_8022209C_ovl_01);

s32 func_80222C94_ovl_01(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = func_8005AF74();
    SavedGame *game = &save->savedGames[memory->savedGameIndex];

    return -(game->isValid == FALSE);
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80222CDC_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80222F2C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_802230C0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80224AFC_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80225358_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80225960_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80226190_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_8022644C_ovl_01);
