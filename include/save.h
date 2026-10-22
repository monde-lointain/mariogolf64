#ifndef SAVE_H
#define SAVE_H

#include <ultra64.h>

/* Marks save data that has been written at least once */
#define SAVE_MAGIC 0x12345678

#define SAVE_COPY_COUNT 2
#define SAVE_RECORD_COUNT 5
#define SAVE_UNKA0_COUNT 3

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

#define COURSE_COUNT 6
#define COURSE_HOLE_COUNT 18
#define RING_SHOT_COURSE_COUNT 6
#define RING_SHOT_HOLE_COUNT 6

typedef struct Unk68 {
    /* 0x00 */ s32 unk0[0x2C / 4];
    /* 0x2C */ s8 unk2C;
    /* 0x2D */ u8 unk2D[0x34 - 0x2D];
    /* 0x34 */ u8 unk34;
    /* 0x35 */ u8 unk35;
    /* 0x36 */ s8 unk36;
    /* 0x37 */ u8 unk37;
    /* 0x38 */ u8 unk38;
    /* 0x39 */ u8 unk39;
    /* 0x3A */ u8 unk3A;
    /* 0x3B */ u8 unk3B[0x68 - 0x3B];
} Unk68; /* size = 0x68 */

typedef struct UnkA0Sub3C {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
} UnkA0Sub3C; /* size = 0x14 */

typedef struct UnkA0 {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ u8 unk4[0x3C - 0x4];
    /* 0x3C */ UnkA0Sub3C unk3C;
    /* 0x50 */ u8 unk50[0x76 - 0x50];
    /* 0x76 */ s8 unk76;
    /* 0x77 */ u8 unk77[0xA0 - 0x77];
} UnkA0; /* size = 0xA0 */

typedef struct UnkB8 {
    /* 0x00 */ s32 unk0[0x1C / 4];
    /* 0x1C */ u8 unk1C;
    /* 0x1D */ u8 unk1D;
    /* 0x1E */ u8 unk1E[0x7B - 0x1E];
    /* 0x7B */ u8 unk7B;
    /* 0x7C */ u8 unk7C[0xB8 - 0x7C];
} UnkB8; /* size = 0xB8 */

typedef struct SaveData {
    /* 0x0000 */ u32 magic;
    /* 0x0004 */ u32 longestDisplayListLength; /* In commands, over every frame drawn */
    /* 0x0008 */ u8 invertedBuildDate[0x20];   /* The writing build's date, with each bit inverted */
    /* 0x0028 */ s8 options[SAVE_OPTION_COUNT];
    /* 0x002C */ u8 unk2C[0xA84 - 0x2C];
    /* 0x0A84 */ s8 hasBirdieBadge[COURSE_COUNT][COURSE_HOLE_COUNT];
    /* 0x0AF0 */ u8 unkAF0[0xDC0 - 0xAF0];
    /* 0x0DC0 */ s32 unkDC0[6][3];
    /* 0x0E08 */ u8 unkE08[0x1320 - 0xE08];
    /* 0x1320 */ s8 hasRingShotStar[RING_SHOT_COURSE_COUNT][RING_SHOT_HOLE_COUNT];
    /* 0x1344 */ u8 unk1344[0x15CC - 0x1344];
    /* 0x15CC */ s32 coursePoints; /* Earned in total, so it never goes down */
    /* 0x15D0 */ u8 unk15D0[0x1DF0 - 0x15D0];
    /* 0x1DF0 */ Unk68 unk1DF0[SAVE_RECORD_COUNT];
    /* 0x1FF8 */ UnkA0 unk1FF8[SAVE_UNKA0_COUNT];
    /* 0x21D8 */ UnkB8 unk21D8[SAVE_UNKA0_COUNT][4];
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
s32 CountOpenCourses(s32 coursePoints);
s32 GetCoursePointsNeeded(s32 courseIndex);

#endif
