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

/* Mirrors of the save data's options */
extern u8 g_OptionBits;
/* A copy of g_OptionBits, from which the music code reads OPTION_BIT_MONO */
extern u8 g_SoundOptionBits;

extern SaveCopyLocation g_SaveCopyLocations[];
extern char g_RomBuildDate[];

void *func_8005AF74(void);
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

INCLUDE_ASM("asm/nonmatchings/menu", func_8005AF74);

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

INCLUDE_ASM("asm/nonmatchings/menu", func_8005B070);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005B0A0);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005B0B4);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005B150);

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

INCLUDE_ASM("asm/nonmatchings/menu", func_8005C458);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005C4B4);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005C510);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005C5B4);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005C614);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005C674);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005CA48);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005CEE0);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005CF78);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005D0D8);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005D1E4);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005D218);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005D23C);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005D248);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005D258);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005D274);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005D2A0);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005D2B8);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005D2E4);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005D308);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005D334);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005D3B8);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005D9A0);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005DAD4);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005DAFC);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005DC50);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005DCDC);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005DD10);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005DD44);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005DD78);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005DDAC);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005DE00);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005DE20);

INCLUDE_ASM("asm/nonmatchings/menu", func_8005DE40);

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

