#include "common.h"
#include "asset.h"
#include "menu_memory.h"

/* The images of D_800C2064 that func_8021CF10 reads */
#define PLAYER_STATS_IMAGE_COUNT 6

/* The bytes of each of the four images that func_8021CFAC reads into one buffer */
#define PLAYER_STATS_IMAGE_SIZE 0x258

extern u16 D_800C2064[];
extern void **D_8023B630_ovl_01;
extern void *D_8023B634_ovl_01;
extern void *D_8023B638_ovl_01;
extern u8 *D_8023B63C_ovl_01;
extern s16 D_8023B640_ovl_01[][5];

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_802178B0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_80217B88_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_80217D20_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021826C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_80219460_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_80219948_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_80219BD4_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021B1E0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021B618_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021B87C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021BE88_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021C200_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021C67C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021CBDC_ovl_01);

void func_8021CF10_ovl_01(void **buffers) {
    EncodedAsset asset;
    s16 imageIndex;

    D_8023B630_ovl_01 = buffers;
    for (imageIndex = 0; imageIndex != PLAYER_STATS_IMAGE_COUNT; imageIndex++) {
        OpenEncodedAsset(D_800C2064[imageIndex], &asset);
        ReadEncodedAsset(D_8023B630_ovl_01[imageIndex], &asset);
    }
}

void func_8021CFAC_ovl_01(void *buffer, s16 bufferKind) {
    EncodedAsset asset;

    switch (bufferKind) {
        case 1:
            D_8023B634_ovl_01 = buffer;
            break;
        case 2:
            D_8023B638_ovl_01 = buffer;
            OpenEncodedAsset(0x6EF, &asset);
            ReadEncodedAsset(D_8023B638_ovl_01, &asset);
            break;
        case 3:
            D_8023B63C_ovl_01 = buffer;
            OpenEncodedAsset(0x82D, &asset);
            ReadEncodedAsset(D_8023B63C_ovl_01, &asset);
            OpenEncodedAsset(0x82F, &asset);
            ReadEncodedAsset(D_8023B63C_ovl_01 + PLAYER_STATS_IMAGE_SIZE, &asset);
            OpenEncodedAsset(0x82E, &asset);
            ReadEncodedAsset(D_8023B63C_ovl_01 + PLAYER_STATS_IMAGE_SIZE * 2, &asset);
            OpenEncodedAsset(0x830, &asset);
            ReadEncodedAsset(D_8023B63C_ovl_01 + PLAYER_STATS_IMAGE_SIZE * 3, &asset);
            break;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021D0B8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021D3AC_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021E01C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021E340_ovl_01);

s16 func_8021E468_ovl_01(void) {
    MenuMemory *memory = func_8005AF74();

    return D_8023B640_ovl_01[memory->unk3A][memory->unk39];
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021E4B0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021E97C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021EF64_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021F124_ovl_01);
