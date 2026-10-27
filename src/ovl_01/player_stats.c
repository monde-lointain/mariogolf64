#include "common.h"
#include "asset.h"
#include "menu_memory.h"

/* The images of D_800C2064 that func_8021CF10 reads */
#define PLAYER_STATS_IMAGE_COUNT 6

/* The bytes of each of the four images that func_8021CFAC reads into one buffer */
#define PLAYER_STATS_IMAGE_SIZE 0x258

/* The grid of stats that the cursor moves over, and the value of its empty cells */
#define STATS_GRID_COLUMN_COUNT 5
#define STATS_GRID_ROW_COUNT 4
#define STATS_CELL_EMPTY -1

/* The directions that func_8021E340 moves the cursor in, which index D_802309C4 */
#define STATS_DIRECTION_LEFT 0
#define STATS_DIRECTION_RIGHT 1
#define STATS_DIRECTION_UP 2
#define STATS_DIRECTION_DOWN 3

extern u16 D_800C2064[];
extern s8 D_802309C4_ovl_01[][2];
extern void **D_8023B630_ovl_01;
extern void *D_8023B634_ovl_01;
extern void *D_8023B638_ovl_01;
extern u8 *D_8023B63C_ovl_01;
extern s16 D_8023B640_ovl_01[][STATS_GRID_COLUMN_COUNT];

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

s8 func_8021E340_ovl_01(s8 column, s8 row, u8 direction) {
    s32 stepCount;
    s32 stepIndex;

    if (direction == STATS_DIRECTION_LEFT || direction == STATS_DIRECTION_RIGHT) {
        stepCount = STATS_GRID_COLUMN_COUNT;
    } else if (direction == STATS_DIRECTION_UP || direction == STATS_DIRECTION_DOWN) {
        stepCount = STATS_GRID_ROW_COUNT;
    } else {
        stepCount = 0;
    }
    for (stepIndex = 0; stepIndex != stepCount; stepIndex++) {
        column += D_802309C4_ovl_01[direction][0];
        row += D_802309C4_ovl_01[direction][1];
        if (column < 0) {
            column = STATS_GRID_COLUMN_COUNT - 1;
        }
        if (column >= STATS_GRID_COLUMN_COUNT) {
            column = 0;
        }
        if (row < 0) {
            row = STATS_GRID_ROW_COUNT - 1;
        }
        if (row >= STATS_GRID_ROW_COUNT) {
            row = 0;
        }
        if (D_8023B640_ovl_01[row][column] != STATS_CELL_EMPTY) {
            break;
        }
    }
    if (stepIndex == stepCount) {
        return -1;
    }
    /* No return follows for other directions, which step no cells and so returned above */
    if (direction == STATS_DIRECTION_LEFT || direction == STATS_DIRECTION_RIGHT) {
        return column;
    } else if (direction == STATS_DIRECTION_UP || direction == STATS_DIRECTION_DOWN) {
        return row;
    }
}

s16 func_8021E468_ovl_01(void) {
    MenuMemory *memory = func_8005AF74();

    return D_8023B640_ovl_01[memory->unk3A][memory->unk39];
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021E4B0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021E97C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021EF64_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021F124_ovl_01);
