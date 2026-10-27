#include "common.h"
#include "character.h"
#include "menu.h"
#include "menu_memory.h"
#include "save.h"

/* Slots marked with MARKED_CHARACTER_OFFSET hold values below this; empty ones hold -1 */
#define MARKED_CHARACTER_LIMIT (-10)

extern s32 D_801B7070;
extern s32 D_8022FF1C_ovl_01[4];
extern s32 D_8022FF50_ovl_01[];

void func_80209500_ovl_01(void) {
    s32 firstSlot = g_CharacterGrid[2][0];
    s32 secondSlot = g_CharacterGrid[2][1];
    s32 thirdSlot = g_CharacterGrid[2][2];
    s32 fourthSlot = g_CharacterGrid[2][3];

    g_CharacterGrid[2][0] = -1;
    g_CharacterGrid[2][1] = -1;
    g_CharacterGrid[2][2] = -1;
    g_CharacterGrid[2][3] = -1;
    D_8022FF1C_ovl_01[0] = firstSlot;
    D_8022FF1C_ovl_01[1] = secondSlot;
    D_8022FF1C_ovl_01[2] = thirdSlot;
    D_8022FF1C_ovl_01[3] = fourthSlot;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_80209568_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_80209894_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_80209BE0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_80209D50_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020B2C4_ovl_01);

void func_8020B3CC_ovl_01(void) {
    MenuMemory *memory = func_8005AF74();
    s32 row;
    s32 column;

    for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
        for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
            if (g_CharacterGrid[row][column] < MARKED_CHARACTER_LIMIT) {
                memory->characterColumn = column;
                memory->characterRow = row;
            }
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020B438_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020B590_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020BA08_ovl_01);

/* Doesn't match: the first loop's index and count take each other's registers */
#ifdef NON_MATCHING
s32 func_8020C2D8_ovl_01(s32 choiceIndex) {
    s32 freePalette = 0;
    s32 playerIndex = 0;
    s32 sameCount = 0;
    s32 character = D_8022FF50_ovl_01[choiceIndex];
    s32 unused[4];
    s32 samePlayers[MAX_PLAYER_COUNT];
    s32 palette;
    s32 sameIndex;

    for (; playerIndex != D_801B7070; playerIndex++) {
        if ((s8)g_Players[playerIndex].characterIndex == character) {
            samePlayers[sameCount++] = playerIndex;
        }
    }
    if (sameCount != 0) {
        for (palette = 0; palette != CHARACTER_PALETTE_COUNT; palette++) {
            for (sameIndex = 0; sameIndex != sameCount; sameIndex++) {
                if ((s8)g_Players[samePlayers[sameIndex]].paletteIndex == palette) {
                    break;
                }
            }
            if (sameIndex == sameCount) {
                freePalette = palette;
                break;
            }
        }
        return freePalette;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020C2D8_ovl_01);
#endif

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020C3B8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020C6D8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020CD20_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020D338_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020D734_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020DB30_ovl_01);

const char D_80231810_ovl_01[] = "Sand Saves";
const char D_8023181C_ovl_01[] = "Par Ons";
const char D_80231824_ovl_01[] = "Par Saves";
const char D_80231830_ovl_01[] = "Avg. Putt";
const char D_8023183C_ovl_01[] = "On Fairway";
const char D_80231848_ovl_01[] = "Avg. Drive";
const char D_80231854_ovl_01[] = "Best Drive";
const char D_80231860_ovl_01[] = "Average";
const char D_80231868_ovl_01[] = "Best Score";
const char D_80231874_ovl_01[] = "Round";
