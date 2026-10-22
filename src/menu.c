#include "common.h"
#include <lib/libnusys/nusys.h>
#include "debug_flags.h"
#include "host_file.h"
#include "libc.h"
#include "math_util.h"
#include "save.h"
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

void *func_8005AF74(void);
s32 func_8005D23C(void);
void func_8005D9A0(void);
s32 ReadSaveCopies(SaveData *save, u8 isInSram);
s32 WriteSaveCopies(SaveData *save, u8 isInSram);
void SyncSaveOptions(SaveData *save, s32 isWrite, s32 readResult);
void TransferSaveData(SaveData *save, u8 isWrite);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005A580);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005ACF8);

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

INCLUDE_ASM("asm/nonmatchings/menu", func_8005B0B4);

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

INCLUDE_ASM("asm/nonmatchings/menu", func_8005B28C);

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

INCLUDE_ASM("asm/nonmatchings/menu", func_8005CEE0);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005CF78);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005D0D8);

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

INCLUDE_ASM("asm/nonmatchings/menu", func_8005DAFC);

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

