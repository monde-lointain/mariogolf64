#include "common.h"
#include <lib/libnusys/nusys.h>
#include "debug_flags.h"
#include "asset.h"
#include "heap.h"
#include "host_file.h"
#include "libc.h"
#include "math_util.h"
#include "save.h"
#include "scene.h"
#include "sram.h"
#include "variables.h"

/* Keeps the save data on the development host instead of in SRAM */
#define DEBUG_FLAG_SAVE_TO_HOST 0x53

/* The SRAM check buffer is read and written by DMA, so it starts on a cache line */
#define DMA_ALIGNMENT 0x10
#define ALIGN_FOR_DMA(address) (((u32)(address) + DMA_ALIGNMENT - 1) & ~(DMA_ALIGNMENT - 1))

/* The records that unlock two of the characters */
#define DONKEY_KONG_STAR_COUNT 30
#define MAPLE_BADGE_COUNT 50

/* Each Ring Shot course after the first opens after this many more stars */
#define STARS_PER_RING_SHOT_COURSE 4

/* Mirrors of the save data's options */
extern u8 g_OptionBits;
/* A copy of g_OptionBits, from which the music code reads OPTION_BIT_MONO */
extern u8 g_SoundOptionBits;

extern SaveCopyLocation g_SaveCopyLocations[];
/* The course points needed in total to open each course */
extern s32 g_CoursePointsNeeded[COURSE_COUNT];
extern char g_RomBuildDate[];

#define IMPORTED_CHARACTER_COUNT 4
#define IMPORTED_CHARACTER_NAME_LENGTH 12
/* The character ids that stand for the imported characters in the grid */
#define FIRST_IMPORTED_CHARACTER 14

/* A character imported from the Game Boy Color version */
typedef struct ImportedCharacter {
    /* 0x00 */ s16 character;
    /* 0x02 */ s16 unk2;
    /* 0x04 */ s16 unk4;
    /* 0x06 */ s16 unk6;
    /* 0x08 */ s8 name[IMPORTED_CHARACTER_NAME_LENGTH];
    /* 0x14 */ s8 unk14;
} ImportedCharacter; /* size = 0x16 */

typedef struct Unk800FF1E8 {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 name[IMPORTED_CHARACTER_NAME_LENGTH];
    /* 0x0D */ u8 unkD[0x19 - 0xD];
    /* 0x19 */ u8 unk19;
    /* 0x1A */ u8 unk1A[0x8C - 0x1A];
} Unk800FF1E8; /* size = 0x8C */

typedef struct Unk2D8 {
    /* 0x000 */ u8 unk0[4];
    /* 0x004 */ s16 unk4;
    /* 0x006 */ u8 unk6[4];
    /* 0x00A */ s16 unkA;
    /* 0x00C */ u8 unkC[0x1E - 0xC];
    /* 0x01E */ s16 unk1E;
    /* 0x020 */ u8 unk20[0x2D8 - 0x20];
} Unk2D8; /* size = 0x2D8 */

typedef struct Unk800C2B90 {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ s16 column;
    /* 0x4 */ s16 unk4;
    /* 0x6 */ s16 row;
} Unk800C2B90; /* size = 0x8 */

extern Unk800FF1E8 D_800FF1E8[IMPORTED_CHARACTER_COUNT];
extern Unk2D8 D_800BB260[];
extern Unk800C2B90 D_800C2B90[IMPORTED_CHARACTER_COUNT];
extern s8 D_800C2AA8[IMPORTED_CHARACTER_COUNT][IMPORTED_CHARACTER_NAME_LENGTH];
/* The imported characters that debug flag 0x54 adds */
extern s16 D_800C2B40[IMPORTED_CHARACTER_COUNT];
extern s16 D_800C2B48[IMPORTED_CHARACTER_COUNT];
extern s16 D_800C2B50[IMPORTED_CHARACTER_COUNT];
extern s16 D_800C2B58[IMPORTED_CHARACTER_COUNT];
extern u8 D_800C2B60[IMPORTED_CHARACTER_COUNT][IMPORTED_CHARACTER_NAME_LENGTH];

extern s8 D_800C1FF0;
extern s8 D_800C1FF4;
extern s8 D_800C1FF5;
extern s8 D_800C1FF6;
extern s8 D_800C1FF7;
extern s32 D_800C2BD0;
extern s8 D_800C2BD4;
extern s16 D_800FBD40;
extern s8 D_800FBDB9;
extern s8 D_800FBE08[4];
extern s16 D_800FBE2C;
extern s32 D_800FE334;
extern s16 D_800FE33C;
extern ImportedCharacter g_ImportedCharacters[IMPORTED_CHARACTER_COUNT];
extern s8 D_800FF4C0;
extern s8 D_801B5634;
extern s8 D_800C2BD5;
extern s8 D_801052E8;
extern s32 D_80105B68;
extern s8 D_80105DC1;
extern s8 D_80105DC2;
extern s32 D_80105DC4;
extern s32 D_80105DC8;
extern s32 D_80105DCC;
extern u8 D_8012D400[3];
extern s32 D_8012F480;
extern s32 D_8012F484;
extern s32 D_8012F488;
extern s32 D_8012F48C;
extern u8 D_801323A0[0x49];
extern s8 D_801323B8;
extern s8 D_801323D0;
extern s32 D_801B6094;
extern s32 D_801B6098;
extern s32 D_801B60AC;
extern s8 D_801B60B9;
extern s32 D_801B7070;
extern s8 D_801B70F0;
extern s8 D_801B70F1;
extern s8 D_801B70F2;
extern s8 D_801B70F3;
extern s16 D_801B71CC;
extern s8 D_801B9410;
extern s32 D_801EFFA0;
extern u8 g_MapCamera[];
extern void *D_801F4A20;
extern void *D_8012D3AC;
extern void *D_8012F4B0[7];
extern void *D_801049A0[7];
extern void *D_801052C4[2];
extern void *D_800FF420[2];
extern void *D_801B60D0;
extern void *D_8012D420[2];
extern void *D_801B5CD0[2];
extern void *D_801052BC[2];
extern void *D_800FBDFC[2];
extern void *D_800FF4C4[2];
extern void *D_800FBE38[3];
extern void *D_801B93E0[5];
extern void *D_8010526C[2];
extern void *D_800FF180[2];
extern void *D_80106100;
extern void *D_800FC898;
extern void *D_8012D430[18];
extern void *D_80105B78[18];
extern void *D_800FBE30[2];
extern void *D_800FC6A0;
extern void *D_801B69EC;
#define CHARACTER_GRID_ROW_COUNT 3
#define CHARACTER_GRID_COLUMN_COUNT 7
/* The rows of the grid that hold the characters to choose from */
#define CHARACTER_CHOICE_ROW_COUNT 2

/*
 * The first empty slot gets a negative mark: the id of the character that
 * slot holds in g_EveryCharacterGrid, less this offset. What reads it is
 * unknown.
 */
#define MARKED_CHARACTER_OFFSET 50
/* D_800C29B8 lists slots in id order up to this count */
#define MARKABLE_CHARACTER_COUNT 10

#define CHARACTER_MAPLE 10
#define CHARACTER_DONKEY_KONG 11
#define CHARACTER_UNK12 12
#define CHARACTER_METAL_MARIO 13

typedef struct Unk800C2590 {
    /* 0x0 */ s16 assetId;
    /* 0x2 */ u8 unk2[6];
} Unk800C2590; /* size = 0x8 */

typedef struct Unk800C2650 {
    /* 0x0 */ u16 assetId;
    /* 0x2 */ u8 unk2[0xE];
} Unk800C2650; /* size = 0x10 */

extern Unk800C2590 D_800C2590[];
extern Unk800C2650 D_800C2650[];
extern s16 g_CharacterGrid[CHARACTER_GRID_ROW_COUNT][CHARACTER_GRID_COLUMN_COUNT];
extern s16 g_EveryCharacterGrid[CHARACTER_CHOICE_ROW_COUNT][CHARACTER_GRID_COLUMN_COUNT];
extern s16 D_800C293C[CHARACTER_CHOICE_ROW_COUNT][CHARACTER_GRID_COLUMN_COUNT];
extern s16 D_800C2968[13];
extern s16 g_StartingCharacterGrid[CHARACTER_GRID_ROW_COUNT][CHARACTER_GRID_COLUMN_COUNT];
extern void *D_80105BB0[4];
extern void *D_8012D468[4];

s32 func_80050598(AssetFile *file);
void func_8005062C(u16 assetId, void *unk);
void func_800506D4(void *unk, void *unk2);
void func_8005CA48(void);
typedef struct Unk800C29B8 {
    /* 0x0 */ s16 column;
    /* 0x2 */ s16 row;
} Unk800C29B8; /* size = 0x4 */
extern Unk800C29B8 D_800C29B8[];
extern s16 D_800C2BB0[15];

void *func_8005AF74(void);
s32 func_8005D23C(void);
void func_8005D9A0(void);
s32 ReadSaveCopies(SaveData *save, u8 isInSram);
s32 WriteSaveCopies(SaveData *save, u8 isInSram);
void SyncSaveOptions(SaveData *save, s32 isWrite, s32 readResult);
void TransferSaveData(SaveData *save, u8 isWrite);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005A580);

void func_8005ACF8(void) {
    s32 index;

    D_800C1FF0 = -1;
    FreeMemory(&D_801F4A20);
    FreeMemory(&D_8012D3AC);
    for (index = 0; index != ARRAY_COUNT(D_801049A0); index++) {
        FreeMemory(&D_801049A0[index]);
        FreeMemory(&D_8012F4B0[index]);
    }
    FreeMemory(&D_801052C4[0]);
    FreeMemory(&D_801052C4[1]);
    FreeMemory(&D_800FF420[0]);
    FreeMemory(&D_800FF420[1]);
    FreeMemory(&D_801B60D0);
    FreeMemory(&D_8012D420[0]);
    FreeMemory(&D_801B5CD0[0]);
    FreeMemory(&D_8012D420[1]);
    FreeMemory(&D_801B5CD0[1]);
    FreeMemory(&D_801052BC[0]);
    FreeMemory(&D_801052BC[1]);
    FreeMemory(&D_800FBDFC[0]);
    FreeMemory(&D_800FBDFC[1]);
    for (index = 0; index != ARRAY_COUNT(D_800FF4C4); index++) {
        FreeMemory(&D_800FF4C4[index]);
    }
    for (index = 0; index != ARRAY_COUNT(D_800FBE38); index++) {
        FreeMemory(&D_800FBE38[index]);
    }
    for (index = 0; index != ARRAY_COUNT(D_801B93E0); index++) {
        FreeMemory(&D_801B93E0[index]);
    }
    FreeMemory(&D_8010526C[0]);
    FreeMemory(&D_8010526C[1]);
    FreeMemory(&D_800FF180[0]);
    FreeMemory(&D_800FF180[1]);
    FreeMemory(&D_80106100);
    FreeMemory(&D_800FC898);
    for (index = 0; index != ARRAY_COUNT(D_8012D430); index++) {
        FreeMemory(&D_8012D430[index]);
    }
    for (index = 0; index != ARRAY_COUNT(D_80105B78); index++) {
        FreeMemory(&D_80105B78[index]);
    }
    FreeMemory(&D_800FBE30[0]);
    FreeMemory(&D_800FBE30[1]);
    FreeMemory(&D_800FC6A0);
    FreeMemory(&D_801B69EC);
}

SaveData *GetSaveData(void) {
    func_8005AF74();
    return &g_SaveData;
}

void *func_8005AF74(void) {
    return D_801323A0;
}

void ResetSaveData(void) {
    SaveData *save;
    s8 row;
    s8 column;

    save = GetSaveData();
    memset(save, 0, sizeof(SaveData));
    save->magic = SAVE_MAGIC;
    for (row = 0; row != ARRAY_COUNT(save->unkDC0); row++) {
        for (column = 0; column != ARRAY_COUNT(save->unkDC0[0]); column++) {
            save->unkDC0[row][column] = -1;
        }
    }
    TransferSaveData(save, TRUE);
}

void InitSaveData(void) {
    SaveData *save;

    save = GetSaveData();
    InitSram();
    TransferSaveData(save, FALSE);
}

s32 CountOpenCourses(s32 coursePoints) {
    s32 courseIndex;

    for (courseIndex = COURSE_COUNT - 1; courseIndex != 0; courseIndex--) {
        if (coursePoints >= g_CoursePointsNeeded[courseIndex]) {
            break;
        }
    }
    return courseIndex + 1;
}

s32 GetCoursePointsNeeded(s32 courseIndex) {
    return g_CoursePointsNeeded[courseIndex];
}

s32 func_8005B0B4(Unk68 *record) {
    s32 value;
    s32 bits;
    s32 unk2C;
    s32 kind;
    s32 maxValue;

    bits = record->unk3A;
    kind = bits & 0xF;
    value = 0;
    unk2C = record->unk2C;
    maxValue = 0x64;
    if (bits & 0x80) {
        switch (kind) {
            case 3:
                value = 0x32;
                if (bits & 0x40) {
                    value = 0x3C;
                }
                break;
            case 2:
                value = 0x3C;
                if (bits & 0x40) {
                    value = 0x46;
                }
                if (unk2C < 5) {
                    value += 0xA;
                }
                break;
            case 0:
                value = 0x5A;
                if (unk2C < 4) {
                    break;
                }
                /* fallthrough */
            case 1:
                value = maxValue;
                break;
        }
    }
    return value;
}

/* Fills order with the indices of values from the largest value down, keeping ties in order */
void SortIndicesDescending(s32 *values, s32 *order, s32 count) {
    s32 sortedValues[count];
    s32 index;
    s32 searchIndex;
    s32 shiftIndex;
    s32 value;
    s32 valueIndex;

    for (index = 0; index < count; index++) {
        order[index] = index;
    }
    for (index = 0; index < count; index++) {
        sortedValues[index] = values[index];
    }
    for (index = 0; index < count; index++) {
        value = sortedValues[index];
        valueIndex = order[index];
        for (searchIndex = index - 1; searchIndex >= 0; searchIndex--) {
            if (sortedValues[searchIndex] >= value) {
                break;
            }
        }
        if (searchIndex < index - 1) {
            for (shiftIndex = index; searchIndex + 1 < shiftIndex; shiftIndex--) {
                sortedValues[shiftIndex] = sortedValues[shiftIndex - 1];
                order[shiftIndex] = order[shiftIndex - 1];
            }
            sortedValues[shiftIndex] = value;
            order[shiftIndex] = valueIndex;
        }
    }
}

void func_8005B28C(s32 recordIndex, Unk68 *record) {
    g_SaveData.unk1DF0[recordIndex] = *record;
    TransferSaveData(GetSaveData(), TRUE);
}

INCLUDE_ASM("asm/nonmatchings/menu", func_8005B314);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005B7BC);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005BC10);

void RecordDisplayListLength(u32 commandCount) {
    if (g_SaveData.longestDisplayListLength < commandCount) {
        g_SaveData.longestDisplayListLength = commandCount;
    }
}

INCLUDE_ASM("asm/nonmatchings/menu", func_8005C038);

s32 HasEveryRingShotStar(void) {
    SaveData *save;
    s32 starCount;
    s32 courseIndex;
    s32 holeIndex;

    save = GetSaveData();
    starCount = 0;
    for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
            starCount += save->hasRingShotStar[courseIndex][holeIndex] != FALSE;
        }
    }
    return starCount == RING_SHOT_COURSE_COUNT * RING_SHOT_HOLE_COUNT;
}

s32 IsDonkeyKongUnlocked(void) {
    SaveData *save;
    s32 starCount;
    s32 courseIndex;
    s32 holeIndex;

    save = GetSaveData();
    starCount = 0;
    for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
            starCount += save->hasRingShotStar[courseIndex][holeIndex] != FALSE;
        }
    }
    return starCount >= DONKEY_KONG_STAR_COUNT;
}

/* The Ring Shot course that opens at exactly the stars earned so far, or 0 for none */
s32 GetRingShotCourseOpenedAtStarCount(void) {
    SaveData *save;
    s32 starCount;
    s32 courseIndex;
    s32 holeIndex;

    save = GetSaveData();
    starCount = 0;
    for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
            starCount += save->hasRingShotStar[courseIndex][holeIndex] != FALSE;
        }
    }
    if (starCount == STARS_PER_RING_SHOT_COURSE * 1) {
        return 1;
    } else if (starCount == STARS_PER_RING_SHOT_COURSE * 2) {
        return 2;
    } else if (starCount == STARS_PER_RING_SHOT_COURSE * 3) {
        return 3;
    } else if (starCount == STARS_PER_RING_SHOT_COURSE * 4) {
        return 4;
    } else if (starCount == STARS_PER_RING_SHOT_COURSE * 5) {
        return 5;
    }
    return 0;
}

s32 IsMapleUnlocked(void) {
    SaveData *save;
    s32 badgeCount;
    s32 courseIndex;
    s32 holeIndex;

    save = GetSaveData();
    badgeCount = 0;
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
            badgeCount += save->hasBirdieBadge[courseIndex][holeIndex] != FALSE;
        }
    }
    return badgeCount >= MAPLE_BADGE_COUNT;
}

s32 IsMetalMarioUnlocked(void) {
    SaveData *save;
    s32 badgeCount;
    s32 courseIndex;
    s32 holeIndex;

    save = GetSaveData();
    badgeCount = 0;
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
            badgeCount += save->hasBirdieBadge[courseIndex][holeIndex] != FALSE;
        }
    }
    return badgeCount == COURSE_COUNT * COURSE_HOLE_COUNT;
}

/* arg0 is reused below as a record count and a character id, to match */
void func_8005C674(s32 arg0, s32 arg1) {
    u8 unk[0x20];
    AssetFile file;
    SaveData *save;
    s32 row;
    s32 column;
    s32 index;
    s32 recordIndex; /* Also holds a grid entry at the end, to match */

    save = GetSaveData();
    for (row = 0; row != CHARACTER_GRID_ROW_COUNT; row++) {
        for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
            g_CharacterGrid[row][column] = g_StartingCharacterGrid[row][column];
        }
    }
    if (IsDebugFlagSet(DEBUG_FLAG_ALL_CHARACTERS)) {
        for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
            for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
                g_CharacterGrid[row][column] = g_EveryCharacterGrid[row][column];
            }
        }
        func_8005CA48();
        return;
    }
    if (arg0 >= 2) {
        for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
            for (column = 0; column != 5; column++) {
                g_CharacterGrid[row][column] = g_EveryCharacterGrid[row][column];
            }
        }
    }
    for (index = 0; index != ARRAY_COUNT(D_800C2968); index++) {
        recordIndex = 0;
        arg0 = 0;
        for (; recordIndex != SAVE_UNKAF0_ROW_COUNT; recordIndex++) {
            if (save->unkAF0[recordIndex][index].unk1 & 0x7F) {
                arg0++;
                break;
            }
        }
        if (arg0 > 0) {
            for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
                for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
                    if (D_800C293C[row][column] == index) {
                        g_CharacterGrid[row][column] = D_800C2968[index];
                    }
                }
            }
        }
    }
    if (IsMapleUnlocked()) {
        g_CharacterGrid[0][5] = CHARACTER_MAPLE;
    }
    if (IsDonkeyKongUnlocked()) {
        g_CharacterGrid[1][5] = CHARACTER_DONKEY_KONG;
    }
    if (IsMetalMarioUnlocked()) {
        g_CharacterGrid[1][6] = CHARACTER_METAL_MARIO;
    }
    func_8005CA48();
    for (index = 0; index != MARKABLE_CHARACTER_COUNT; index++) {
        row = D_800C29B8[index].row;
        column = D_800C29B8[index].column;
        if (g_CharacterGrid[row][column] < 0) {
            g_CharacterGrid[row][column] = index - MARKED_CHARACTER_OFFSET;
            break;
        }
    }
    if (index == MARKABLE_CHARACTER_COUNT) {
        if (g_CharacterGrid[0][6] != CHARACTER_UNK12) {
            g_CharacterGrid[0][6] = CHARACTER_UNK12 - MARKED_CHARACTER_OFFSET;
        }
    }
    if ((u8)arg1 != 1) {
        for (index = 0; index != ARRAY_COUNT(g_ImportedCharacters); index++) {
            recordIndex = g_CharacterGrid[2][index];
            if (recordIndex >= 0) {
                arg0 = g_ImportedCharacters[index].character;
                func_8005062C(D_800C2650[arg0].assetId, unk);
                func_800506D4(D_8012D468[index], unk);
                OpenAssetFile(D_800C2590[arg0].assetId, &file);
                ReadAssetFile(D_80105BB0[index], func_80050598(&file), &file);
            }
        }
    }
}

void func_8005CA48(void) {
    s16 index;
    s16 byteIndex;
    s16 count; /* Also the byte index of the first loops, to match */
    u8 *name;
    u8 letter;

    for (index = 0; index != IMPORTED_CHARACTER_COUNT; index++) {
        g_ImportedCharacters[index].character = 0;
        g_ImportedCharacters[index].unk2 = 0;
        g_ImportedCharacters[index].unk4 = 0;
        g_ImportedCharacters[index].unk6 = 0;
        for (count = 0; count != IMPORTED_CHARACTER_NAME_LENGTH; count++) {
            g_ImportedCharacters[index].name[count] = 0;
        }
    }
    for (index = 0; index != IMPORTED_CHARACTER_COUNT; index++) {
        for (count = 0; count != IMPORTED_CHARACTER_NAME_LENGTH; count++) {
            D_800C2AA8[index][count] = 0;
        }
    }
    count = 0;
    for (index = 0; index != IMPORTED_CHARACTER_COUNT; index++) {
        if (D_800FF1E8[index].unk0 == 1) {
            count++;
        }
    }
    if (IsDebugFlagSet(DEBUG_FLAG_ALL_CHARACTERS)) {
        count = IMPORTED_CHARACTER_COUNT;
    }
    if (count > 0) {
        if (count > IMPORTED_CHARACTER_COUNT) {
            count = IMPORTED_CHARACTER_COUNT;
        }
        for (index = 0; index != count; index++) {
            if (IsDebugFlagSet(DEBUG_FLAG_ALL_CHARACTERS)) {
                name = D_800C2B60[index];
            } else {
                name = D_800FF1E8[index].name;
            }
            for (byteIndex = 0; byteIndex != IMPORTED_CHARACTER_NAME_LENGTH; byteIndex++) {
                letter = *name++;
                D_800C2AA8[index][byteIndex] = letter;
                g_ImportedCharacters[index].name[byteIndex] = letter;
            }
            g_CharacterGrid[D_800C2B90[index].row][D_800C2B90[index].column] = FIRST_IMPORTED_CHARACTER + index;
            if (IsDebugFlagSet(DEBUG_FLAG_ALL_CHARACTERS)) {
                g_ImportedCharacters[index].character = D_800C2B40[index];
                g_ImportedCharacters[index].unk2 = D_800C2B48[index];
                g_ImportedCharacters[index].unk4 = D_800C2B50[index];
                g_ImportedCharacters[index].unk6 = D_800C2B58[index];
                D_800BB260[FIRST_IMPORTED_CHARACTER + index].unk4 = D_800C2B48[index];
                D_800BB260[FIRST_IMPORTED_CHARACTER + index].unkA = D_800C2B50[index];
                D_800BB260[FIRST_IMPORTED_CHARACTER + index].unk1E = D_800C2B58[index];
            } else {
                g_ImportedCharacters[index].character = D_800FF1E8[index].unk19 + FIRST_IMPORTED_CHARACTER;
                g_ImportedCharacters[index].unk2 = D_800BB260[FIRST_IMPORTED_CHARACTER + index].unk4;
                g_ImportedCharacters[index].unk4 = D_800BB260[FIRST_IMPORTED_CHARACTER + index].unkA;
                g_ImportedCharacters[index].unk6 = D_800BB260[FIRST_IMPORTED_CHARACTER + index].unk1E;
            }
        }
    }
}

s32 func_8005CEE0(s32 index) {
    s32 slot;
    s32 row;
    s32 column;

    if (index < 0) {
        return -1;
    }
    if (index >= ARRAY_COUNT(D_800C2BB0)) {
        return -1;
    }
    func_8005C674(1, 1);
    slot = D_800C2BB0[index];
    row = D_800C29B8[slot].row;
    column = D_800C29B8[slot].column;
    return g_CharacterGrid[row][column] < 0 ? -1 : 0;
}


#ifdef NON_MATCHING
void func_8005CF78(void) {
    SaveData *save;
    s32 isInvalid[SAVE_UNKA0_COUNT];
    s32 index;
    s32 entryIndex;
    UnkA0 *unk;
    UnkA0Sub3C *sub;
    UnkB8 *entries;

    save = GetSaveData();
    for (index = 0; index != SAVE_UNKA0_COUNT; index++) {
        isInvalid[index] = FALSE;
    }
    for (index = 0; index != SAVE_UNKA0_COUNT; index++) {
        if (save->unk1FF8[index].unk0 == 0) {
            continue;
        }
        unk = &save->unk1FF8[index];
        sub = &unk->unk3C;
        if (unk->unk76 == 0) {
            switch (sub->unk4) {
                case 0:
                case 2:
                case 4:
                case 5:
                case 10:
                case 11:
                    break;
                default:
                    isInvalid[index] = TRUE;
                    break;
            }
        }
        if (sub->unk8 < 1 || sub->unk8 > 4) {
            isInvalid[index] = TRUE;
        }
        if (sub->unkC < 0 || sub->unkC >= 12) {
            isInvalid[index] = TRUE;
        }
        if (sub->unk10 < 0 || sub->unk10 >= COURSE_HOLE_COUNT) {
            isInvalid[index] = TRUE;
        }
        entries = save->unk21D8[index];
        for (entryIndex = 0; entryIndex != sub->unk8; entryIndex++) {
            if (entries[entryIndex].unk1D >= COURSE_HOLE_COUNT) {
                isInvalid[index] = TRUE;
            }
            if (entries[entryIndex].unk7B >= 4) {
                isInvalid[index] = TRUE;
            }
        }
    }
    for (index = 0; index != SAVE_UNKA0_COUNT; index++) {
        if (isInvalid[index] == TRUE) {
            save->unk1FF8[index].unk0 = 0;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/menu", func_8005CF78);
#endif


#ifdef NON_MATCHING
void func_8005D0D8(void) {
    SaveData *save;
    s32 isInvalid[SAVE_RECORD_COUNT];
    s32 recordIndex;
    Unk68 *record;
    s32 kind;

    save = GetSaveData();
    for (recordIndex = 0; recordIndex != SAVE_RECORD_COUNT; recordIndex++) {
        isInvalid[recordIndex] = FALSE;
    }
    for (recordIndex = 0; recordIndex != SAVE_RECORD_COUNT; recordIndex++) {
        record = &save->unk1DF0[recordIndex];
        if (record->unk36 & 0x80) {
            if ((record->unk36 & 0xF) >= 5 || (record->unk36 & 0xF) < 0) {
                isInvalid[recordIndex] = TRUE;
            }
        }
        if (record->unk37 >= COURSE_HOLE_COUNT) {
            isInvalid[recordIndex] = TRUE;
        }
        if (record->unk39 >= 4) {
            isInvalid[recordIndex] = TRUE;
        }
        if (record->unk34 >= 6) {
            isInvalid[recordIndex] = TRUE;
        }
        if (record->unk35 >= 6) {
            isInvalid[recordIndex] = TRUE;
        }
    }
    /* Toggling rather than clearing sets the flag on records that fail with it clear */
    for (recordIndex = 0; recordIndex != SAVE_RECORD_COUNT; recordIndex++) {
        if (isInvalid[recordIndex] == TRUE) {
            save->unk1DF0[recordIndex].unk36 ^= 0x80;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/menu", func_8005D0D8);
#endif


void func_8005D1E4(void) {
    u8 *unk;

    unk = func_8005AF74();
    unk[0x2E] = D_801B6098;
    unk[0x2F] = D_801B6094;
}

s32 func_8005D218(void) {
    s32 value;

    switch (D_80105DC1) {
        case 0:
        default:
            value = 0;
            break;
        case 1:
            value = 7;
            break;
    }
    return value;
}


s32 func_8005D23C(void) {
    return D_80105DC2;
}

s32 func_8005D248(void) {
    return D_80105DC4 != 0;
}

s32 func_8005D258(void) {
    s32 value;

    value = D_80105DC8;
    if (value < 0) {
        value = 0;
    }
    return value + 0x16F;
}

s32 func_8005D274(void) {
    s32 value;

    value = func_8005D23C();
    if (value < 0) {
        value = 0;
    }
    return value + 0x1EB;
}

s32 func_8005D2A0(void) {
    s32 value;

    value = D_80105DCC;
    if (value <= 0) {
        value = 1;
    }
    return value;
}

void func_8005D2B8(u8 *destination) {
    s32 index;

    for (index = 0; index != ARRAY_COUNT(D_8012D400); index++) {
        *destination++ = D_8012D400[index];
    }
}

s32 func_8005D2E4(void) {
    s32 value;

    switch (D_80105DC1) {
        case 0:
        default:
            value = 0x9E;
            break;
        case 1:
            value = 0x9D;
            break;
    }
    return value;
}


s32 func_8005D308(void) {
    s32 value;

    if (D_80105DC2 >= 6) {
        value = 0xC7;
        if (D_80105DC2 >= 9) {
            value = 0xC8;
        }
    } else {
        value = D_80105DC2 + 0x1EB;
    }
    return value;
}


void func_8005D334(void) {
    s32 index;
    s32 byteIndex;

    for (index = 0; index != ARRAY_COUNT(g_ImportedCharacters); index++) {
        g_ImportedCharacters[index].character = 0;
        g_ImportedCharacters[index].unk2 = 0;
        g_ImportedCharacters[index].unk4 = 0;
        g_ImportedCharacters[index].unk6 = 0;
        for (byteIndex = 0; byteIndex != IMPORTED_CHARACTER_NAME_LENGTH; byteIndex++) {
            g_ImportedCharacters[index].name[byteIndex] = 0;
        }
        g_ImportedCharacters[index].unk14 = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/menu", func_8005D3B8);

void func_8005D9A0(void) {
    s8 index;

    D_800C2BD0 = 6;
    D_800FE334 = 0;
    D_801EFFA0 = 0;
    D_80105B68 = 0;
    D_801B7070 = 0;
    D_8012F480 = -1;
    D_8012F484 = -1;
    D_8012F488 = -1;
    D_8012F48C = -1;
    D_801B70F0 = 0;
    D_801B70F1 = 0;
    D_801B70F2 = 0;
    D_801B70F3 = 0;
    memset(D_801323A0, 0, sizeof(D_801323A0));
    D_800FBD40 = 0;
    D_801B71CC = 0;
    D_800FBE2C = 0;
    D_800FBDB9 = 0;
    D_800FE33C = 0;
    D_800FF4C0 = 0;
    g_MapCamera[0x4C] = 0;
    for (index = 0; index != ARRAY_COUNT(D_800FBE08); index++) {
        D_800FBE08[index] = 0;
    }
    D_801323B8 = 3;
    D_801B60AC = 0;
    D_801323D0 = 6;
    D_801052E8 = 0;
    D_800C2BD4 = 0;
}

void func_8005DAD4(void) {
    D_800FE334 = 0;
    D_801052E8 = 1;
    D_801B9410 = 0;
    D_800C2BD4 = 0;
}

void func_8005DAFC(void) {
    switch (g_GameMode) {
        case 5:
            D_800FF4C0 = 2;
            g_MapCamera[0x4C] = 0;
            D_801B5634 = 0;
            break;
        case 0:
            D_800FF4C0 = 1;
            g_MapCamera[0x4C] = 1;
            if (D_80105B68 == 4) {
                D_801B5634 = 2;
            } else {
                D_801B5634 = 1;
            }
            D_800C2BD5 = 1;
            break;
        case 2:
            D_800FF4C0 = 1;
            g_MapCamera[0x4C] = 1;
            D_801B5634 = 3;
            break;
        case 10:
            D_800FF4C0 = 1;
            g_MapCamera[0x4C] = 1;
            D_801B5634 = 4;
            break;
        case 3:
        case 4:
        case 6:
        case 7:
        case 9:
            D_800FF4C0 = 1;
            g_MapCamera[0x4C] = 2;
            D_801B5634 = 0;
            break;
        case 11:
            D_800FF4C0 = 1;
            g_MapCamera[0x4C] = 1;
            D_801B5634 = 5;
            break;
    }
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    D_801EFFA0 = 6;
}

void func_8005DC50(void) {
    func_8005D9A0();
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    g_ImportedCharacters[0].unk14 = 0;
    g_ImportedCharacters[1].unk14 = 0;
    g_ImportedCharacters[2].unk14 = 0;
    g_ImportedCharacters[3].unk14 = 0;
    D_800C1FF4 = -1;
    D_800C1FF5 = -1;
    D_800C1FF6 = -1;
    D_800C1FF7 = -1;
    D_801EFFA0 = 0;
}

void func_8005DCDC(void) {
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    D_801EFFA0 = 0x17;
}

void func_8005DD10(void) {
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    D_801EFFA0 = 5;
}

void func_8005DD44(void) {
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    D_801EFFA0 = 0x12;
}

void func_8005DD78(void) {
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    D_801EFFA0 = 0xD;
}

void func_8005DDAC(void) {
    D_800FE334 = 0;
    D_801052E8 = 1;
    D_800C2BD4 = 0;
    D_801B9410 = 1;
    D_801B60B9 = 0;
    ClearDebugFlag(0x1E);
    D_801EFFA0 = 0xC;
}

s8 func_8005DE00(void) {
    return GetSaveData()->options[SAVE_OPTION_UNK0];
}

s8 func_8005DE20(void) {
    return GetSaveData()->options[SAVE_OPTION_MONO];
}

s8 func_8005DE40(void) {
    return GetSaveData()->options[SAVE_OPTION_UNK3];
}

void WriteSaveData(void) {
    TransferSaveData(GetSaveData(), TRUE);
}

/* Copies the options between the save data and g_OptionBits, in the direction of the transfer */
void SyncSaveOptions(SaveData *save, s32 isWrite, s32 readResult) {
    u8 *bits;
    s8 *options;

    options = save->options;
    if (!isWrite) {
        bits = &g_OptionBits;
        *bits = 0;
        if (readResult > 0) {
            if (options[SAVE_OPTION_UNK0]) {
                *bits = OPTION_BIT_UNK0;
            }
            if (options[SAVE_OPTION_MONO]) {
                *bits |= OPTION_BIT_MONO;
                g_SoundOptionBits = *bits;
            }
            if (options[SAVE_OPTION_UNK3]) {
                *bits |= OPTION_BIT_UNK3;
            }
        }
    } else {
        if (g_OptionBits & OPTION_BIT_UNK0) {
            options[SAVE_OPTION_UNK0] = TRUE;
        } else {
            options[SAVE_OPTION_UNK0] = FALSE;
        }
        if (g_OptionBits & OPTION_BIT_MONO) {
            options[SAVE_OPTION_MONO] = TRUE;
        } else {
            options[SAVE_OPTION_MONO] = FALSE;
        }
        if (g_OptionBits & OPTION_BIT_UNK3) {
            options[SAVE_OPTION_UNK3] = TRUE;
        } else {
            options[SAVE_OPTION_UNK3] = FALSE;
        }
    }
}

void TransferSaveData(SaveData *save, u8 isWrite) {
    u8 isSramSelected;
    u8 isInSram;
    s32 readResult;

    isSramSelected = !IsDebugFlagSet(DEBUG_FLAG_SAVE_TO_HOST);
    isInSram = isSramSelected;
    if (!isWrite) {
        readResult = ReadSaveCopies(save, isInSram);
        SyncSaveOptions(save, FALSE, readResult);
    } else {
        /* readResult is never set on this path, but SyncSaveOptions ignores it when writing */
        SyncSaveOptions(save, TRUE, readResult);
        WriteSaveCopies(save, isInSram);
    }
}


/* Reads the first intact copy; if neither is, the save data starts over */
s32 ReadSaveCopies(SaveData *save, u8 isInSram) {
    u8 unusedBuffer[0x90]; /* Only takes up stack */
    u8 checkBuffer[0x40];
    SaveCheck *check;
    u32 size;
    s32 unmarkedCount;
    s32 copyIndex;
    u16 crc;

    check = (SaveCheck *)ALIGN_FOR_DMA(checkBuffer);
    unmarkedCount = 0;
    for (copyIndex = 0; copyIndex < SAVE_COPY_COUNT; copyIndex++) {
        if (!isInSram) {
            if (ReadHostFile(g_SaveCopyLocations[copyIndex].dataFileName, &size) != HOST_FILE_OK ||
                size != sizeof(SaveData)) {
                continue;
            }
            if (ReadHostFile(g_SaveCopyLocations[copyIndex].checkFileName, &size) != HOST_FILE_OK ||
                size != sizeof(SaveCheck)) {
                continue;
            }
        } else {
            nuPiReadSram(g_SaveCopyLocations[copyIndex].dataSramAddress, save, sizeof(SaveData));
            nuPiReadSram(g_SaveCopyLocations[copyIndex].checkSramAddress, check, sizeof(SaveCheck));
        }
        if (save->magic != SAVE_MAGIC) {
            unmarkedCount++;
            continue;
        }
        crc = check->keyedCrc ^ check->key;
        if (crc == ComputeCrc16((u8 *)save, sizeof(SaveData))) {
            break;
        }
    }
    if (copyIndex == 0) {
        return SAVE_READ_FIRST_COPY;
    }
    if (copyIndex == 1) {
        return SAVE_READ_SECOND_COPY;
    }
    ResetSaveData();
    return (unmarkedCount == SAVE_COPY_COUNT) ? SAVE_READ_NEVER_WRITTEN : SAVE_READ_FAILED;
}

s32 WriteSaveCopies(SaveData *save, u8 isInSram) {
    u8 checkBuffer[0xC0];
    SaveCheck *check;
    u16 crc;
    s32 dateIndex;
    s32 copyIndex;

    check = (SaveCheck *)ALIGN_FOR_DMA(checkBuffer);
    for (dateIndex = 0; dateIndex < ARRAY_COUNT(save->invertedBuildDate); dateIndex++) {
        save->invertedBuildDate[dateIndex] = ~g_RomBuildDate[dateIndex];
    }
    crc = ComputeCrc16((u8 *)save, sizeof(SaveData));
    for (copyIndex = 0; copyIndex < SAVE_COPY_COUNT; copyIndex++) {
        check->key = rand();
        check->keyedCrc = check->key ^ crc;
        if (!isInSram) {
            if (WriteHostFile(g_SaveCopyLocations[copyIndex].dataFileName, sizeof(SaveData)) == HOST_FILE_OK) {
                WriteHostFile(g_SaveCopyLocations[copyIndex].checkFileName, sizeof(SaveCheck));
            }
        } else {
            nuPiWriteSram(g_SaveCopyLocations[copyIndex].dataSramAddress, save, sizeof(SaveData));
            nuPiWriteSram(g_SaveCopyLocations[copyIndex].checkSramAddress, check, sizeof(SaveCheck));
        }
    }
    return TRUE;
}

