#include "common.h"
#include <lib/libnusys/nusys.h>
#include "debug_flags.h"
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

typedef struct Unk800FE3D8 {
    /* 0x00 */ s16 unk0;
    /* 0x02 */ s16 unk2;
    /* 0x04 */ s16 unk4;
    /* 0x06 */ s16 unk6;
    /* 0x08 */ s8 unk8[12];
    /* 0x14 */ s8 unk14;
} Unk800FE3D8; /* size = 0x16 */

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
extern Unk800FE3D8 D_800FE3D8[4];
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
extern s16 D_800C28E4[15][7];
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

INCLUDE_ASM("asm/nonmatchings/menu", func_8005C674);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005CA48);

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
    return D_800C28E4[row][column] < 0 ? -1 : 0;
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

    for (index = 0; index != ARRAY_COUNT(D_800FE3D8); index++) {
        D_800FE3D8[index].unk0 = 0;
        D_800FE3D8[index].unk2 = 0;
        D_800FE3D8[index].unk4 = 0;
        D_800FE3D8[index].unk6 = 0;
        for (byteIndex = 0; byteIndex != ARRAY_COUNT(D_800FE3D8[index].unk8); byteIndex++) {
            D_800FE3D8[index].unk8[byteIndex] = 0;
        }
        D_800FE3D8[index].unk14 = 0;
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
    D_800FE3D8[0].unk14 = 0;
    D_800FE3D8[1].unk14 = 0;
    D_800FE3D8[2].unk14 = 0;
    D_800FE3D8[3].unk14 = 0;
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

