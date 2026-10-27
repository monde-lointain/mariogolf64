#include "common.h"
#include "asset.h"
#include "character.h"
#include "menu.h"
#include "menu_memory.h"
#include "save.h"
#include "sound.h"
#include "text.h"

/* The images of D_800C2064 that func_8021CF10 reads */
#define PLAYER_STATS_IMAGE_COUNT 6

/* The bytes of each of the four images that func_8021CFAC reads into one buffer */
#define PLAYER_STATS_IMAGE_SIZE 0x258

/* The most bytes of each ranked record holder's palette file that func_8021EF64 keeps */
#define PLAYER_STATS_FILE_SIZE 0x200

/* The grid of stats that the cursor moves over, and the value of its empty cells */
#define STATS_GRID_COLUMN_COUNT 5
#define STATS_GRID_ROW_COUNT 4
#define STATS_CELL_EMPTY -1

/* The directions that func_8021E340 moves the cursor in, which index D_802309C4 */
#define STATS_DIRECTION_LEFT 0
#define STATS_DIRECTION_RIGHT 1
#define STATS_DIRECTION_UP 2
#define STATS_DIRECTION_DOWN 3

/* The presets of func_80217B88; two of them have a variant that four players take instead */
#define MENU_UNKE_PRESET_COUNT 7
#define MENU_UNKE_PRESET_FIRST 0
#define MENU_UNKE_PRESET_FIRST_FOUR_PLAYER 1
#define MENU_UNKE_PRESET_SECOND 5
#define MENU_UNKE_PRESET_SECOND_FOUR_PLAYER 6

/*
 * The rows of the stats panel slide across together: D_80230784 holds their
 * offsets, and D_802307A8 holds where each slide after the first sends them
 */
#define STATS_SLIDE_ROW_COUNT 9
#define STATS_SLIDE_CURRENT 0
#define STATS_SLIDE_OUT_RIGHT 1
#define STATS_SLIDE_OUT_LEFT 2
#define STATS_SLIDE_IN_FROM_RIGHT 3
#define STATS_SLIDE_IN_FROM_LEFT 4
#define STATS_SLIDE_COUNT 5

/* The phases of the screen 20 handler, which D_8022BE88 holds */
#define MENU_PHASE_START 0
#define MENU_PHASE_CHOOSING 5
#define MENU_PHASE_CANCELLED 6
#define MENU_PHASE_CONFIRMED 7
#define MENU_UNK89_UNK4 4

/* Each player's choice on screen 20, and where the screen's slide starts */
#define PLAYER_OPTION_COUNT 37
#define PLAYER_OPTION_SLIDE_START -512

extern u16 D_800C2064[];
extern void *D_8012D430[CHARACTER_COUNT];
extern u16 D_802308A4_ovl_01[CHARACTER_COUNT];
extern s16 D_802308C8_ovl_01[CHARACTER_COUNT][CHARACTER_PALETTE_COUNT];
extern s32 D_80105B68;
extern s8 D_801B60BA[MENU_UNKE_COUNT];
extern u8 D_802304BC_ovl_01[MENU_UNKE_PRESET_COUNT][MENU_UNKE_COUNT];
extern u8 D_802304F4_ovl_01[];
extern s8 D_802309C4_ovl_01[][2];
extern void **D_8023B630_ovl_01;
extern void *D_8023B634_ovl_01;
extern void *D_8023B638_ovl_01;
extern u8 *D_8023B63C_ovl_01;
extern void *D_8023B5E0_ovl_01[STATS_GRID_ROW_COUNT][STATS_GRID_COLUMN_COUNT];
extern s16 D_8023B640_ovl_01[][STATS_GRID_COLUMN_COUNT];
extern s32 D_80230718_ovl_01[STATS_SLIDE_ROW_COUNT];
extern s32 D_8023073C_ovl_01[STATS_SLIDE_ROW_COUNT];
extern s32 D_80230784_ovl_01[STATS_SLIDE_ROW_COUNT];
extern s32 D_802307A8_ovl_01[STATS_SLIDE_COUNT - 1][STATS_SLIDE_ROW_COUNT];
extern u8 D_80230838_ovl_01;
extern u8 D_8022BE88_ovl_01;
extern u8 D_8022BE89_ovl_01;
extern u8 D_802303A1_ovl_01;
extern s32 D_8023BFD0_ovl_01;
extern s16 D_8023BFDC_ovl_01[MAX_PLAYER_COUNT];

void func_8021B87C_ovl_01(Gfx **displayListHead, u8 slide);
extern u8 D_8023BFF0_ovl_01[STATS_GRID_ROW_COUNT * STATS_GRID_COLUMN_COUNT][PLAYER_STATS_FILE_SIZE];

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_802178B0_ovl_01);

void func_80217B88_ovl_01(s32 requestedPreset) {
    s8 presetIndex = requestedPreset;
    MenuMemory *memory = func_8005AF74();
    s16 index;

    if ((u8)requestedPreset < MENU_UNKE_PRESET_COUNT) {
        if ((s8)requestedPreset == MENU_UNKE_PRESET_FIRST && D_80105B68 == MAX_PLAYER_COUNT) {
            presetIndex = MENU_UNKE_PRESET_FIRST_FOUR_PLAYER;
        }
        if (presetIndex == MENU_UNKE_PRESET_SECOND && D_80105B68 == MAX_PLAYER_COUNT) {
            presetIndex = MENU_UNKE_PRESET_SECOND_FOUR_PLAYER;
        }
        for (index = 0; index != MENU_UNKE_COUNT; index++) {
            memory->unkE[index] = D_802304BC_ovl_01[presetIndex][index];
            D_801B60BA[index] = D_802304F4_ovl_01[memory->unkE[index]];
        }
    } else {
        for (index = 0; index != MENU_UNKE_COUNT; index++) {
            memory->unkE[index] = -1;
            D_801B60BA[index] = -1;
        }
    }
    memory->unkD = 0;
    for (index = 0; index != MAX_PLAYER_COUNT; index++) {
        D_8023BFDC_ovl_01[index] = index < D_80105B68 ? 0 : -1;
    }
    memory->unk29 = 0;
    memory->unk2A = 0;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_80217D20_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021826C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_80219460_ovl_01);

/* Returns nothing, but is declared to return a value, which leaves v0 live at its end */
s32 func_80219948_ovl_01(void) {
    MenuMemory *memory = func_8005AF74();

    if (D_8022BE88_ovl_01 == MENU_PHASE_START) {
        PlaySong(SONG_TITLE, 0);
        g_MenuScreen = MENU_SCREEN_UNK6;
        D_8022BE89_ovl_01 = MENU_UNK89_UNK4;
        FreeAllCharacterSlots();
    }
    if (D_8022BE88_ovl_01 == MENU_PHASE_CHOOSING) {
        if (g_MenuPressedButtons & A_BUTTON) {
            PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
            SetTextSlot(0, 0, NO_MESSAGE, 1, 0, 1, 0xFF, 0xFF, 0xFF, 0xFF);
            D_8023BFD0_ovl_01 = PLAYER_OPTION_SLIDE_START;
            D_802303A1_ovl_01 = 0;
            D_8022BE88_ovl_01 = MENU_PHASE_CONFIRMED;
            return;
        }
        if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
            g_MenuScreen = MENU_SCREEN_UNK22;
            SetTextSlot(0, 0, NO_MESSAGE, 1, 0, 1, 0xFF, 0xFF, 0xFF, 0xFF);
            D_802303A1_ovl_01 = 0;
            D_8022BE88_ovl_01 = MENU_PHASE_CANCELLED;
            return;
        }
        if (g_MenuRepeatButtons & L_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            memory->unk29--;
            if (memory->unk29 < 0) {
                memory->unk29 = D_80105B68 - 1;
            }
            D_802303A1_ovl_01 = 0;
            return;
        }
        if (g_MenuRepeatButtons & R_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            memory->unk29++;
            if (memory->unk29 > D_80105B68 - 1) {
                memory->unk29 = 0;
            }
            D_802303A1_ovl_01 = 0;
            return;
        }
        if (g_MenuRepeatButtons & U_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            D_8023BFDC_ovl_01[memory->unk29]++;
            if (D_8023BFDC_ovl_01[memory->unk29] >= PLAYER_OPTION_COUNT) {
                D_8023BFDC_ovl_01[memory->unk29] = 0;
            }
            return;
        }
        if (g_MenuRepeatButtons & D_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            D_8023BFDC_ovl_01[memory->unk29]--;
            if (D_8023BFDC_ovl_01[memory->unk29] < 0) {
                D_8023BFDC_ovl_01[memory->unk29] = PLAYER_OPTION_COUNT - 1;
            }
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_80219BD4_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021B1E0_ovl_01);

s32 func_8021B618_ovl_01(Gfx **displayListHead, u8 slide) {
    Gfx *gfx = *displayListHead;
    s32 index;
    s32 rowIndex;
    s32 offset; /* The slide's first target, then the first row's offset */
    s32 first;

    if ((slide != STATS_SLIDE_CURRENT && slide < STATS_SLIDE_COUNT) == FALSE) {
        return -1;
    }
    offset = D_802307A8_ovl_01[slide - 1][0];
    first = D_80230784_ovl_01[0];
    for (index = 0; index != STATS_SLIDE_ROW_COUNT; index++) {
        if (D_80230784_ovl_01[index] != offset && first != D_80230784_ovl_01[index]) {
            break;
        }
    }
    if (index == STATS_SLIDE_ROW_COUNT && D_80230838_ovl_01 != slide) {
        D_80230838_ovl_01 = slide;
        switch (slide) {
            case STATS_SLIDE_OUT_RIGHT:
                for (rowIndex = 0; rowIndex != STATS_SLIDE_ROW_COUNT; rowIndex++) {
                    D_80230784_ovl_01[rowIndex] = D_802307A8_ovl_01[STATS_SLIDE_OUT_RIGHT - 1][rowIndex];
                    D_8023073C_ovl_01[rowIndex] = D_80230718_ovl_01[rowIndex];
                }
                break;
            case STATS_SLIDE_OUT_LEFT:
                for (rowIndex = 0; rowIndex != STATS_SLIDE_ROW_COUNT; rowIndex++) {
                    D_80230784_ovl_01[rowIndex] = D_802307A8_ovl_01[STATS_SLIDE_OUT_LEFT - 1][rowIndex];
                    D_8023073C_ovl_01[rowIndex] = D_80230718_ovl_01[rowIndex];
                }
                break;
            case STATS_SLIDE_IN_FROM_RIGHT:
            case STATS_SLIDE_IN_FROM_LEFT:
                for (rowIndex = 0; rowIndex != STATS_SLIDE_ROW_COUNT; rowIndex++) {
                    D_80230784_ovl_01[rowIndex] = D_802307A8_ovl_01[D_80230838_ovl_01 - 1][rowIndex];
                    D_8023073C_ovl_01[rowIndex] = D_80230718_ovl_01[rowIndex];
                }
                break;
        }
    }
    func_8021B87C_ovl_01(&gfx, D_80230838_ovl_01);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
    for (index = 0; index != STATS_SLIDE_ROW_COUNT; index++) {
        if ((offset = D_80230784_ovl_01[0]) != D_80230784_ovl_01[index]) {
            break;
        }
    }
    return index != STATS_SLIDE_ROW_COUNT;
}

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

/* Doesn't match: the original keeps the row and column counters in other saved registers */
#ifdef NON_MATCHING
void func_8021EF64_ovl_01(void) {
    EncodedAsset asset;
    AssetFile file;
    SaveData *save = GetSaveData();
    s32 index;
    s32 row;
    s32 column;
    s32 character;
    s32 palette;
    s32 cell;

    for (index = 0; index != CHARACTER_COUNT; index++) {
        OpenEncodedAsset(D_802308A4_ovl_01[index], &asset);
        ReadEncodedAsset(D_8012D430[index], &asset);
    }
    for (row = 0; row != STATS_GRID_ROW_COUNT; row++) {
        cell = row * STATS_GRID_COLUMN_COUNT;
        for (column = 0; column != STATS_GRID_COLUMN_COUNT; column++, cell++) {
            if (D_8023B640_ovl_01[row][column] != STATS_CELL_EMPTY) {
                character = (s8)save->rankedRecords[D_8023B640_ovl_01[row][column]].unk3B;
                palette = (s8)save->rankedRecords[D_8023B640_ovl_01[row][column]].unk3D;
                if (character < 0) {
                    character = 0;
                }
                if (character >= CHARACTER_COUNT) {
                    character = CHARACTER_COUNT - 1;
                }
                if (palette < 0) {
                    palette = 0;
                }
                if (palette >= CHARACTER_PALETTE_COUNT) {
                    palette = CHARACTER_PALETTE_COUNT - 1;
                }
            } else {
                character = 0;
                palette = 0;
            }
            D_8023B5E0_ovl_01[row][column] = D_8023BFF0_ovl_01[cell];
            OpenAssetFile(D_802308C8_ovl_01[character][palette], &file);
            ReadAssetFile(D_8023B5E0_ovl_01[row][column], GetAssetFileSize(&file), &file);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021EF64_ovl_01);
#endif

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021F124_ovl_01);
