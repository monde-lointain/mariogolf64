#ifndef SAVE_H
#define SAVE_H

#include <ultra64.h>
#include "character.h"
#include "club.h"
#include "variables.h"

/* Marks save data that has been written at least once */
#define SAVE_MAGIC 0x12345678

#define SAVE_COPY_COUNT 2
#define SAVE_RECORD_COUNT 5
#define RANKED_RECORD_COUNT 20
/* The games saved partway through, to continue later */
#define SAVED_GAME_COUNT 3
#define MAX_PLAYER_COUNT 4

/* The most rings that a hole of Ring Shot has */
#define RING_COUNT 3

/*
 * The games kept for continuing by game mode rather than by slot, and the
 * players and records kept with them
 */
#define MODE_GAME_COUNT 3
#define MODE_PLAYER_COUNT 7
#define MODE_RECORD_COUNT 6

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
#define SAVE_UNK1344_ENTRY_COUNT 18

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

/* GameState.holeSelection values; code tests the nines as bits */
#define HOLE_SELECTION_ALL 0
#define HOLE_SELECTION_BACK_NINE (1 << 1)
#define HOLE_SELECTION_FRONT_NINE (1 << 2)

/* The bets of a game, indexing GameState.betRates and UnkB8.betTallies */
#define BET_LONGEST_DRIVE 0 /* On marked par 5 holes */
#define BET_NEAREST_PIN 1   /* On par 3 holes */
#define BET_STROKES 2       /* Paid per stroke, between each pair of players */
#define BET_NASSAU 3        /* Paid on the front nine, the back nine and the round */
#define BET_UNK4 4          /* Settled on each hole; a tie carries the pot over */
#define BET_UNK5 5          /* Settled as BET_UNK4, when that bet is not set */
#define BET_LAS_VEGAS 6     /* Paid per stroke, between the two teams of two */
#define BET_COUNT 7

/* The contest distances that are not measured yet, or do not count */
#define UNCOUNTED_DISTANCE -1
#define UNMEASURED_DISTANCE -2

/* Indices of GameState.carriedPots */
#define CARRIED_POT_LONGEST_DRIVE 0
#define CARRIED_POT_NEAREST_PIN 1
#define CARRIED_POT_HOLE 2
#define CARRIED_POT_COUNT 3

/*
 * The current game's settings and place, which a saved game keeps for it
 * to be continued with. Code that reads one field alone also names some
 * of them on their own, such as g_GameMode, g_CourseIndex and g_HoleIndex.
 */
typedef struct GameState {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 gameMode;
    /* 0x08 */ s32 playerCount;
    /* 0x0C */ s32 courseIndex;
    /* 0x10 */ s32 holeIndex;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s32 unk20;
    /* 0x24 */ s32 holeSelection; /* A HOLE_SELECTION_* value */
    /* 0x28 */ u8 unk28[0x30 - 0x28];
    /* 0x30 */ s8 unk30;
    /* 0x31 */ u8 unk31;
    /* 0x32 */ s8 betRates[BET_COUNT]; /* What each BET_* bet is worth */
    /* 0x39 */ u8 unk39;
    /* 0x3A */ s8 isMiniGolf;
    /* 0x3B */ u8 unk3B[0x3D - 0x3B];
    /* 0x3D */ s8 saveCount; /* Raised by one, up to 99, when the game is saved */
    /* 0x3E */ s16 carriedPots[CARRIED_POT_COUNT]; /* The pots that tied holes carry over */
} GameState; /* size = 0x44 */

typedef struct Unk800BA9D8 {
    /* 0x00 */ CoursePoint cupPosition;
    /* 0x0C */ CoursePoint unkC;
    /* 0x18 */ HoleConditions conditions; /* g_HoleConditions */
    /* 0x2C */ s8 par;                     /* g_HolePar */
    /* 0x2D */ s8 nextCupIndex;            /* sNextCupIndex */
    /* 0x2E */ u8 unk2E[0x34 - 0x2E];
} Unk800BA9D8; /* size = 0x34 */

extern Unk800BA9D8 D_800BA9D8;

/* The current round's statistics, for the course's records */
typedef struct RoundStats {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 measuredDriveCount;
    /* 0x0C */ s32 longestDrive; /* Starts as the course's record */
    /* 0x10 */ s32 averagedDriveTotal;
    /* 0x14 */ s32 averagedDriveCount;
    /* 0x18 */ s32 isDriveAveraged; /* Whether the drive was hit with the 1W */
} RoundStats; /* size = 0x1C */

/* Indices of UnkE98.hasPlace */
#define THIRD_PLACE_INDEX 0
#define SECOND_PLACE_INDEX 1
#define FIRST_PLACE_INDEX 2
#define TOURNAMENT_PLACE_COUNT 3

typedef struct UnkE98 {
    /* 0x00 */ s32 roundCount;
    /* 0x04 */ s32 bestScore;          /* The lowest net score to par over a round */
    /* 0x08 */ s32 strokeTotal;
    /* 0x0C */ s32 longestDrive;
    /* 0x10 */ s32 averageDrive;
    /* 0x14 */ s32 averagedDriveCount;
    /* 0x18 */ s32 measuredDriveTotal; /* The sum of RoundStats.measuredDriveCount */
    /* 0x1C */ s32 puttTotal;
    /* 0x20 */ s32 parOrBetterCount;   /* Holes played in par or fewer strokes */
    /* 0x24 */ s32 parOnCount;         /* Holes whose green was reached with two strokes to spare */
    /* 0x28 */ s32 unk28;              /* The sum of RoundStats.unk0 */
    /* 0x2C */ s32 unk2C;              /* The sum of RoundStats.unk4 */
    /* 0x30 */ s32 hasPlace[TOURNAMENT_PLACE_COUNT]; /* In a tournament on the course */
    /* 0x3C */ s16 characterBestScores[CHARACTER_COUNT]; /* bestScore, by character */
    /* 0x60 */ u8 hasCharacterBestScore[CHARACTER_COUNT];
    /* 0x72 */ u8 unk72[0x74 - 0x72];
} UnkE98; /* size = 0x74 */

typedef struct SavedGame {
    /* 0x00 */ s32 isValid;
    /* 0x04 */ u8 unk4[4];
    /* 0x08 */ Unk800BA9D8 unk8;
    /* 0x3C */ GameState state;
    /* 0x80 */ RoundStats roundStats;
    /* 0x9C */ u8 unk9C[4];
} SavedGame; /* size = 0xA0 */

/* PlayerHole.putts when they were not counted */
#define UNCOUNTED_PUTTS -1

/* A player's results on one hole */
typedef struct PlayerHole {
    /* 0x0 */ s8 strokes;
    /* 0x1 */ s8 putts; /* Or UNCOUNTED_PUTTS */
} PlayerHole; /* size = 0x2 */

typedef struct UnkB8 {
    /* 0x00 */ s32 unk0[0xC / 4];
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10[(0x1C - 0x10) / 4];
    /* 0x1C */ u8 unk1C;
    /* 0x1D */ u8 characterIndex; /* Also the index of the character's club set */
    /* 0x1E */ u8 unk1E[0x22 - 0x1E];
    /* 0x22 */ s8 unk22;
    /* 0x23 */ s8 unk23;
    /* 0x24 */ u8 unk24[2];
    /* 0x26 */ s8 unk26;
    /* 0x27 */ u8 unk27[2];
    /* 0x29 */ s8 handicap; /* Gives strokes on the holes it ranks within */
    /* 0x2A */ u8 unk2A;
    /* 0x2B */ PlayerHole holes[COURSE_HOLE_COUNT];
    /* 0x4F */ u8 unk4F[0x52 - 0x4F];
    /* 0x52 */ s16 clubs[CLUB_SLOT_COUNT]; /* Indices into g_Clubs, by bag slot */
    /* 0x72 */ u8 unk72[0x78 - 0x72];
    /* 0x78 */ s8 ringStates[RING_COUNT]; /* RING_STATE_* values, for Ring Shot */
    /* 0x7B */ u8 unk7B;
    /* 0x7C */ u8 unk7C[4];
    /* 0x80 */ s32 driveDistance; /* UNMEASURED_DISTANCE, or UNCOUNTED_DISTANCE */
    /* 0x84 */ s32 pinDistance;   /* UNMEASURED_DISTANCE, or UNCOUNTED_DISTANCE */
    /* 0x88 */ u8 unk88[0x8E - 0x88];
    /* 0x8E */ s16 betTallies[BET_COUNT]; /* What the player has won on each BET_* bet */
    /* 0x9C */ s8 unk9C;
    /* 0x9D */ u8 unk9D[3];
    /* 0xA0 */ s32 lastDriveDistance; /* The driveDistance that RecordDrive took */
    /* 0xA4 */ u8 unkA4[0xB8 - 0xA4];
} UnkB8; /* size = 0xB8 */

/* The best times kept for each course, with what was recorded alongside */
#define BEST_TIME_COUNT 3

#define SAVE_UNKAF0_ROW_COUNT 18
#define SAVE_UNKAF0_COLUMN_COUNT 18

/* UnkAF0.unk1 keeps a count of wins in these bits, and a flag in the top one */
#define WIN_COUNT_MASK 0x7F
#define UNKAF0_FLAG 0x80

typedef struct UnkAF0 {
    /* 0x0 */ u8 unk0;
    /* 0x1 */ u8 unk1;
} UnkAF0; /* size = 0x2 */

/*
 * Routines that update the birdie badges reach them through a pointer to
 * this; where it ends is unknown
 */
typedef struct Unk990 {
    /* 0x000 */ u8 unk0[0xF4];
    /* 0x0F4 */ s8 birdieBadgeCounts[COURSE_COUNT][COURSE_HOLE_COUNT]; /* Nonzero holds the badge */
    /* 0x160 */ UnkAF0 unkAF0[SAVE_UNKAF0_ROW_COUNT][SAVE_UNKAF0_COLUMN_COUNT];
} Unk990; /* size = 0x3E8 */

typedef struct SaveData {
    /* 0x0000 */ u32 magic;
    /* 0x0004 */ u32 longestDisplayListLength; /* In commands, over every frame drawn */
    /* 0x0008 */ u8 invertedBuildDate[0x20];   /* The writing build's date, with each bit inverted */
    /* 0x0028 */ s8 options[SAVE_OPTION_COUNT];
    /* 0x002C */ u8 unk2C[0x38 - 0x2C];
    /* 0x0038 */ SavedGame modeGames[MODE_GAME_COUNT];
    /* 0x0218 */ UnkB8 modePlayers[MODE_PLAYER_COUNT];
    /* 0x0720 */ Unk68 modeRecords[MODE_RECORD_COUNT];
    /* 0x0990 */ Unk990 unk990;
    /* 0x0D78 */ s32 unkD78[COURSE_COUNT][BEST_TIME_COUNT];
    /* 0x0DC0 */ s32 bestTimes[COURSE_COUNT][BEST_TIME_COUNT]; /* Fastest first; unset is -1 */
    /* 0x0E08 */ s32 unkE08[COURSE_COUNT][BEST_TIME_COUNT];
    /* 0x0E50 */ s32 unkE50[COURSE_COUNT][BEST_TIME_COUNT];
    /* 0x0E98 */ UnkE98 unkE98[COURSE_COUNT];
    /* 0x1150 */ u8 unk1150[0x1320 - 0x1150];
    /* 0x1320 */ s8 hasRingShotStar[RING_SHOT_COURSE_COUNT][RING_SHOT_HOLE_COUNT];
    /* 0x1344 */ s8 unk1344[RING_SHOT_COURSE_COUNT][RING_SHOT_HOLE_COUNT][SAVE_UNK1344_ENTRY_COUNT];
    /* 0x15CC */ s32 coursePoints; /* Earned in total, so it never goes down */
    /* 0x15D0 */ Unk68 rankedRecords[RANKED_RECORD_COUNT];
    /* 0x1DF0 */ Unk68 unk1DF0[SAVE_RECORD_COUNT];
    /* 0x1FF8 */ SavedGame savedGames[SAVED_GAME_COUNT];
    /* 0x21D8 */ UnkB8 savedPlayers[SAVED_GAME_COUNT][MAX_PLAYER_COUNT];
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

extern GameState g_GameState;

/* The current game's, which a saved game is loaded into and stored from */
extern UnkB8 g_Players[MAX_PLAYER_COUNT];

SaveData *GetSaveData(void);
void TransferSaveData(SaveData *save, u8 isWrite);
void ResetSaveData(void);
void InitSaveData(void);
void RecordDisplayListLength(u32 commandCount);
void WriteSaveData(void);
s32 CountOpenCourses(s32 coursePoints);
s32 GetCoursePointsNeeded(s32 courseIndex);

#endif
