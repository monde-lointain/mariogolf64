#include "common.h"
#include "asset.h"
#include "character.h"
#include "game_flags.h"
#include "hole_display.h"
#include "menu.h"
#include "menu_memory.h"
#include "save.h"
#include "screen_layers.h"
#include "sound.h"
#include "sram.h"
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
/* The bit and the kind field in a ranked record's unk3A */
#define RANKED_RECORD_SAVED 0x80
#define RANKED_RECORD_KIND_MASK 0xF
/* func_8021E01C lists each kind's records in rows one longer than the grid */
#define STATS_KIND_LIST_SIZE (STATS_GRID_COLUMN_COUNT + 1)

/* The buffers that func_802178B0 fills, and the assets it reads into them */
#define PLAYER_STATS_BUFFER_ICONS 1
#define PLAYER_STATS_BUFFER_UNK2 2
#define PLAYER_STATS_BUFFER_UNK3 3
#define PLAYER_STATS_BUFFER_UNK4 4
#define PLAYER_STATS_BUFFER_UNK5 5
#define PLAYER_STATS_BUFFER_UNK6 6
#define PLAYER_STATS_BUFFER_UNK7 7
#define PLAYER_STATS_BUFFER_UNK8 8
#define PLAYER_STATS_BUFFER_UNK9 9
#define PLAYER_STATS_BUFFER_PLAYER_LABELS 10
#define PLAYER_STATS_BUFFER_UNK11 11
#define PLAYER_STATS_BUFFER_UNK12 12
#define PLAYER_STATS_BUFFER_UNK13 13
#define PLAYER_STATS_ICON_COUNT 8
#define PLAYER_STATS_ICON_SIZE 0x218
#define ASSET_PLAYER_STATS_UNK2 0x671
#define ASSET_PLAYER_STATS_UNK7 0x720
#define ASSET_PLAYER_STATS_UNK8 0x6C4
#define ASSET_PLAYER_STATS_UNK9 0x6E8
#define ASSET_PLAYER_STATS_UNK11 0x721
#define ASSET_PLAYER_STATS_UNK12 0x828
#define ASSET_PLAYER_STATS_UNK13 0x829
#define STATS_INDEX_COLUMN_COUNT 6

/* The text slots that screen 12 clears on leaving */
#define STATS_TEXT_SLOT_COUNT 4

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
extern void *D_8023B340_ovl_01;
extern void *D_8023B344_ovl_01;
extern void *D_8023B348_ovl_01;
extern void *D_8023B34C_ovl_01;
extern void *D_8023B350_ovl_01;
extern void *D_8023B354_ovl_01;
extern void *D_8023B358_ovl_01;
extern void *D_8023B35C_ovl_01;
extern void *D_8023B360_ovl_01;
extern void *D_8023B364_ovl_01;
extern void *D_8023B368_ovl_01;
extern void *D_8023B36C_ovl_01;
extern u32 D_802303AC_ovl_01[PLAYER_STATS_ICON_COUNT];
extern u32 D_802303CC_ovl_01[][2];
extern s16 D_8023040C_ovl_01[][STATS_INDEX_COLUMN_COUNT];
extern u8 D_801B5634;
extern s32 D_8022F8C0_ovl_01;
extern u8 D_8022F8C4_ovl_01;
extern u8 D_8022F8C5_ovl_01;
extern u8 D_802303A0_ovl_01;
extern u8 D_802308A0_ovl_01;
extern s8 D_801052E8;
extern s32 D_800FE334;
extern s8 D_801B60B9;
extern u8 D_801B9410;
extern s16 D_8023BFDC_ovl_01[MAX_PLAYER_COUNT];

void func_8021B87C_ovl_01(Gfx **displayListHead, u8 slide);
extern u8 D_8023BFF0_ovl_01[STATS_GRID_ROW_COUNT * STATS_GRID_COLUMN_COUNT][PLAYER_STATS_FILE_SIZE];

void func_802178B0_ovl_01(void *buffer, s16 bufferKind) {
    EncodedAsset asset;
    MenuMemory *memory = func_8005AF74();
    s16 iconIndex;
    u8 rowNumber; /* D_801B5634 counts the index tables' rows from 1 */
    u16 assetId;
    void **imageSlot;

    switch (bufferKind) {
        case PLAYER_STATS_BUFFER_ICONS:
            D_8023B340_ovl_01 = buffer;
            for (iconIndex = 0; iconIndex != PLAYER_STATS_ICON_COUNT; iconIndex++) {
                OpenEncodedAsset(D_802303AC_ovl_01[iconIndex], &asset);
                ReadEncodedAsset((u8 *)D_8023B340_ovl_01 + iconIndex * PLAYER_STATS_ICON_SIZE, &asset);
            }
            break;
        case PLAYER_STATS_BUFFER_UNK2:
            D_8023B348_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_PLAYER_STATS_UNK2, &asset);
            ReadEncodedAsset(D_8023B348_ovl_01, &asset);
            break;
        case PLAYER_STATS_BUFFER_UNK3:
            D_8023B34C_ovl_01 = buffer;
            break;
        case PLAYER_STATS_BUFFER_UNK4:
            rowNumber = D_801B5634;
            imageSlot = &D_8023B350_ovl_01;
            *imageSlot = buffer;
            OpenEncodedAsset(D_802303CC_ovl_01[D_8023040C_ovl_01[rowNumber - 1][memory->unkD]][0], &asset);
            ReadEncodedAsset(*imageSlot, &asset);
            break;
        case PLAYER_STATS_BUFFER_UNK5:
            rowNumber = D_801B5634;
            imageSlot = &D_8023B354_ovl_01;
            *imageSlot = buffer;
            OpenEncodedAsset(D_802303CC_ovl_01[D_8023040C_ovl_01[rowNumber - 1][memory->unkD]][1], &asset);
            ReadEncodedAsset(*imageSlot, &asset);
            break;
        case PLAYER_STATS_BUFFER_UNK6:
            D_8023B344_ovl_01 = buffer;
            break;
        case PLAYER_STATS_BUFFER_UNK7:
            D_8023B358_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_PLAYER_STATS_UNK7, &asset);
            ReadEncodedAsset(D_8023B358_ovl_01, &asset);
            break;
        case PLAYER_STATS_BUFFER_UNK8:
            D_8023B360_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_PLAYER_STATS_UNK8, &asset);
            ReadEncodedAsset(D_8023B360_ovl_01, &asset);
            break;
        case PLAYER_STATS_BUFFER_UNK9:
            D_8023B364_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_PLAYER_STATS_UNK9, &asset);
            ReadEncodedAsset(D_8023B364_ovl_01, &asset);
            break;
        case PLAYER_STATS_BUFFER_PLAYER_LABELS:
            D_8023B368_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_PLAYER_LABELS, &asset);
            ReadEncodedAsset(D_8023B368_ovl_01, &asset);
            break;
        case PLAYER_STATS_BUFFER_UNK11:
            D_8023B36C_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_PLAYER_STATS_UNK11, &asset);
            ReadEncodedAsset(D_8023B36C_ovl_01, &asset);
            break;
        case PLAYER_STATS_BUFFER_UNK12:
            assetId = ASSET_PLAYER_STATS_UNK12;
            D_8023B35C_ovl_01 = buffer;
            OpenEncodedAsset(assetId, &asset);
            ReadEncodedAsset(D_8023B35C_ovl_01, &asset);
            break;
        case PLAYER_STATS_BUFFER_UNK13:
            assetId = ASSET_PLAYER_STATS_UNK13;
            D_8023B35C_ovl_01 = buffer;
            OpenEncodedAsset(assetId, &asset);
            ReadEncodedAsset(D_8023B35C_ovl_01, &asset);
            break;
    }
    D_8022F8C5_ovl_01 = 0;
    D_8022F8C0_ovl_01 = 0;
    D_8022F8C4_ovl_01 = 0;
    D_802303A0_ovl_01 = 0;
    D_802303A1_ovl_01 = 0;
}

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

s16 func_8021E468_ovl_01(void);

/* func_8021E340 is called without a prototype; the original extends its result from 16 bits, not 8 */
void func_8021D0B8_ovl_01(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = func_8005AF74();
    s32 slot;
    s32 moved;
    s32 column;

    if (IsHudAnimationDone() == TRUE) {
        if (GetWipeKind() != WIPE_NONE && D_801B9410 == 0) {
            if (GetWipeProgress() == 0) {
                g_MenuScreen = MENU_SCREEN_EXIT;
                D_800FE334 = 0;
                D_801B60B9 = 1;
                SetGameFlag(GAME_FLAG_UNK1E);
                for (slot = 0; slot != STATS_TEXT_SLOT_COUNT; slot++) {
                    SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
                }
            }
        } else if (GetWipeKind() == WIPE_NONE || D_801B9410 != 1) {
            if (g_MenuPressedButtons & A_BUTTON) {
                if (func_8021E468_ovl_01() != STATS_CELL_EMPTY) {
                    PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                    D_801B9410 = 0;
                    column = memory->unk39;
                    LoadWipeShape((s8)save->rankedRecords[D_8023B640_ovl_01[memory->unk3A][column]].unk38);
                    StartWipe(WIPE_CLOSING);
                    D_801052E8 = 0;
                }
            } else if (g_MenuPressedButtons & B_BUTTON) {
                StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
                PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
                StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
                g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
                for (slot = 0; slot != STATS_TEXT_SLOT_COUNT; slot++) {
                    SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
                }
            } else if (g_MenuRepeatButtons & U_JPAD) {
                moved = func_8021E340_ovl_01(memory->unk39, memory->unk3A, STATS_DIRECTION_UP);
                if ((s16)moved != STATS_CELL_EMPTY) {
                    PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    memory->unk3A = moved;
                }
            } else if (g_MenuRepeatButtons & D_JPAD) {
                moved = func_8021E340_ovl_01(memory->unk39, memory->unk3A, STATS_DIRECTION_DOWN);
                if ((s16)moved != STATS_CELL_EMPTY) {
                    PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    memory->unk3A = moved;
                }
            } else if (g_MenuRepeatButtons & L_JPAD) {
                moved = func_8021E340_ovl_01(memory->unk39, memory->unk3A, STATS_DIRECTION_LEFT);
                if ((s16)moved != STATS_CELL_EMPTY) {
                    PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    memory->unk39 = moved;
                }
            } else if (g_MenuRepeatButtons & R_JPAD) {
                moved = func_8021E340_ovl_01(memory->unk39, memory->unk3A, STATS_DIRECTION_RIGHT);
                if ((s16)moved != STATS_CELL_EMPTY) {
                    PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    memory->unk39 = moved;
                }
            }
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021D3AC_ovl_01);

/*
 * Doesn't match: the original copies the chosen row's zero into the column
 * rather than loading zero, and so reads the last row into a1 instead of v1.
 */
#ifdef NON_MATCHING
void func_8021E01C_ovl_01(void) {
    SaveData *save = GetSaveData();
    s32 kindRecords[STATS_GRID_ROW_COUNT][STATS_KIND_LIST_SIZE];
    s32 index;
    s32 recordIndex;
    s32 kindThreeCount;
    s32 kindTwoCount;
    s32 kindOneCount;
    s32 kindZeroCount;
    s32 flags;
    u8 row;
    s32 column;
    MenuMemory *memory;

    InitSram();
    TransferSaveData(save, FALSE);
    for (recordIndex = 0; recordIndex != STATS_GRID_COLUMN_COUNT; recordIndex++) {
        kindRecords[0][recordIndex] = STATS_CELL_EMPTY;
        kindRecords[1][recordIndex] = STATS_CELL_EMPTY;
        kindRecords[2][recordIndex] = STATS_CELL_EMPTY;
        kindRecords[3][recordIndex] = STATS_CELL_EMPTY;
    }
    kindThreeCount = 0;
    kindTwoCount = 0;
    kindZeroCount = 0;
    kindOneCount = 0;
    for (recordIndex = 0; recordIndex != RANKED_RECORD_COUNT; recordIndex++) {
        flags = (s8)save->rankedRecords[recordIndex].unk3A;
        if (flags & RANKED_RECORD_SAVED) {
            switch (flags & RANKED_RECORD_KIND_MASK) {
                case 3:
                    if (kindThreeCount < STATS_GRID_COLUMN_COUNT) {
                        kindRecords[0][kindThreeCount++] = recordIndex;
                    }
                    break;
                case 2:
                    if (kindTwoCount < STATS_GRID_COLUMN_COUNT) {
                        kindRecords[1][kindTwoCount++] = recordIndex;
                    }
                    break;
                case 1:
                    if (kindOneCount < STATS_GRID_COLUMN_COUNT) {
                        kindRecords[3][kindOneCount++] = recordIndex;
                    }
                    break;
                case 0:
                    if (kindZeroCount < STATS_GRID_COLUMN_COUNT) {
                        kindRecords[2][kindZeroCount++] = recordIndex;
                    }
                    break;
            }
        }
    }
    row = 0;
    memory = func_8005AF74();
    column = row;
    if (kindThreeCount == 0) {
        if (kindTwoCount != 0) {
            row = 1;
        } else if (kindOneCount != 0) {
            row = 2;
        } else if (kindZeroCount != 0) {
            row = 3;
        }
    }
    switch (memory->unk3A) {
        case 0:
            if (kindThreeCount != 0) {
                row = 0;
                if (memory->unk39 < kindThreeCount && memory->unk39 >= 0) {
                    column = memory->unk39;
                }
            }
            break;
        case 1:
            if (kindTwoCount != 0) {
                row = 1;
                if (memory->unk39 < kindTwoCount && memory->unk39 >= 0) {
                    column = memory->unk39;
                }
            }
            break;
        case 2:
            if (kindOneCount != 0) {
                row = 2;
                if (memory->unk39 < kindOneCount && memory->unk39 >= 0) {
                    column = memory->unk39;
                }
            }
            break;
        case 3:
            if (kindZeroCount != 0) {
                row = 3;
                if (memory->unk39 < kindZeroCount && memory->unk39 >= 0) {
                    column = memory->unk39;
                }
            }
            break;
    }
    memory->unk39 = column;
    memory->unk3A = row;
    for (recordIndex = 0; recordIndex != STATS_GRID_COLUMN_COUNT; recordIndex++) {
        D_8023B640_ovl_01[0][recordIndex] = kindRecords[0][recordIndex];
        D_8023B640_ovl_01[1][recordIndex] = kindRecords[1][recordIndex];
        D_8023B640_ovl_01[2][recordIndex] = kindRecords[3][recordIndex];
        D_8023B640_ovl_01[3][recordIndex] = kindRecords[2][recordIndex];
    }
    recordIndex = 0;
    D_802308A0_ovl_01 = 0;
    for (; recordIndex != STATS_GRID_ROW_COUNT; recordIndex++) {
        for (index = 0; index != STATS_GRID_COLUMN_COUNT; index++) {
            if (D_8023B640_ovl_01[recordIndex][index] != STATS_CELL_EMPTY) {
                D_802308A0_ovl_01++;
                break;
            }
        }
        if (D_802308A0_ovl_01 != 0) {
            break;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021E01C_ovl_01);
#endif

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
