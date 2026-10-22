#ifndef SAVE_H
#define SAVE_H

#include <ultra64.h>

/* Marks save data that has been written at least once */
#define SAVE_MAGIC 0x12345678

#define SAVE_COPY_COUNT 2

/* Indices of SaveData's options */
#define SAVE_OPTION_UNK0 0
#define SAVE_OPTION_MONO 1 /* Plays the sound in mono instead of stereo */
#define SAVE_OPTION_UNK3 3
#define SAVE_OPTION_COUNT 4

/* Bits of g_OptionBits, each mirroring one of the saved options */
#define OPTION_BIT_UNK0 (1 << 0)
#define OPTION_BIT_MONO (1 << 1)
#define OPTION_BIT_UNK3 (1 << 2)

/* Results of reading the save data */
#define SAVE_READ_NEVER_WRITTEN -2 /* Neither copy has the magic number */
#define SAVE_READ_FAILED -1
#define SAVE_READ_FIRST_COPY 1
#define SAVE_READ_SECOND_COPY 2

typedef struct SaveData {
    /* 0x0000 */ u32 magic;
    /* 0x0004 */ u32 longestDisplayListLength; /* In commands, over every frame drawn */
    /* 0x0008 */ u8 invertedBuildDate[0x20];   /* The writing build's date, with each bit inverted */
    /* 0x0028 */ s8 options[SAVE_OPTION_COUNT];
    /* 0x002C */ u8 unk2C[0xDC0 - 0x2C];
    /* 0x0DC0 */ s32 unkDC0[6][3];
    /* 0x0E08 */ u8 unkE08[0x2A78 - 0xE08];
} SaveData; /* size = 0x2A78 */

/* Each copy is stored with this, to check that it was read back intact */
typedef struct SaveCheck {
    /* 0x0 */ u16 key; /* Random */
    /* 0x2 */ u16 keyedCrc; /* The data's CRC-16 exclusive-ored with the key */
} SaveCheck;

/* Where a copy of the save data and its check are kept */
typedef struct SaveCopyLocation {
    /* 0x0 */ char *dataFileName;  /* On the development host */
    /* 0x4 */ char *checkFileName; /* On the development host */
    /* 0x8 */ u32 dataSramAddress;
    /* 0xC */ u32 checkSramAddress;
} SaveCopyLocation;

extern SaveData g_SaveData;

SaveData *GetSaveData(void);
void ResetSaveData(void);
void InitSaveData(void);
void RecordDisplayListLength(u32 commandCount);
void WriteSaveData(void);

#endif
