#ifndef SAVE_H
#define SAVE_H

#include <ultra64.h>

#include "character.h"
#include "course.h"
#include "course_stats.h"
#include "game_state.h"
#include "language.h"
#include "play_record.h"
#include "player.h"
#include "round_stats.h"

/* Marks save data that has been written at least once */
#define SAVE_MAGIC 0x12345678

#define SAVE_COPY_COUNT 2
#define RING_SHOT_RECORD_COUNT 5
#define RANKED_RECORD_COUNT 20
/* The games saved partway through, to continue later */
#define SAVED_GAME_COUNT 3

/*
 * The games kept for continuing by game mode rather than by slot, and the
 * players and records kept with them
 */
#define MODE_GAME_COUNT 3
#define MODE_PLAYER_COUNT 7
#define MODE_RECORD_COUNT 6

/*
 * Where each game mode keeps its game in the save's mode tables. Mode 4's
 * two records overlap mode 5's one.
 */
#define MODE4_GAME_INDEX 0
#define MODE4_PLAYER_INDEX 0
#define MODE4_RECORD_INDEX 0
#define MODE4_PLAYER_COUNT 2
#define MODE5_GAME_INDEX 1
#define MODE5_PLAYER_INDEX 2
#define MODE5_RECORD_INDEX 1
#define MODE5_PLAYER_COUNT 1
#define MODE0_GAME_INDEX 2
#define MODE0_PLAYER_INDEX 3
#define MODE0_RECORD_INDEX 2
#define MODE0_PLAYER_COUNT 4

/* Indices of SaveData's options */
/* The options screen's MUSIC set to OFF */
#define SAVE_OPTION_MUSIC_OFF 0
/* Plays the sound in mono instead of stereo */
#define SAVE_OPTION_MONO 1
/* Copied into each player's Player.gridColor */
#define SAVE_OPTION_GRID_COLOR 2
/* The options screen's CAMERA set to SIMPLE instead of DYNAMIC */
#define SAVE_OPTION_SIMPLE_CAMERA 3
#define SAVE_OPTION_COUNT 4

/* SAVE_OPTION_GRID_COLOR values */
enum GridColor {
  GRID_COLOR_WHITE,
  GRID_COLOR_BLACK
};

/* Bits of MenuMemory's optionBits, each mirroring one of the saved options */
#define OPTION_BIT_MUSIC_OFF (1 << 0)
#define OPTION_BIT_MONO (1 << 1)
#define OPTION_BIT_SIMPLE_CAMERA (1 << 2)
/* A copy of MenuMemory's optionBits, from which the music code reads
 * OPTION_BIT_MONO */
extern u8 g_SoundOptionBits;

/* Results of reading the save data */
enum SaveReadResult {
  SAVE_READ_NEVER_WRITTEN = -2, /* Neither copy has the magic number */
  SAVE_READ_FAILED,
  SAVE_READ_FIRST_COPY = 1,
  SAVE_READ_SECOND_COPY
};

typedef struct SavedGame {
  /* 0x00 */ Boolean isValid;
  /* 0x04 */ u8 unusedBeforeHole[4];
  /* 0x08 */ HoleSetup hole;
  /* 0x3C */ GameState state;
  /* 0x80 */ RoundStats roundStats;
  /* 0x9C */ u8 unusedAfterRoundStats[4];
} SavedGame; /* size = 0xA0 */

/* The best times kept for each course, with what was recorded alongside */
#define BEST_TIME_COUNT 3

/* MatchupRecord.winBits keeps a count of wins in these bits */
#define WIN_COUNT_MASK 0x7F
/* Set by a win in a game that was never saved partway; the character table
 * shows such a matchup with a star */
#define WON_UNSAVED_FLAG 0x80

/* A character's record against a computer character in Get Character */
typedef struct MatchupRecord {
  /* 0x0 */ u8 lossCount;
  /* 0x1 */ u8 winBits; /* The win count, and WON_UNSAVED_FLAG */
} MatchupRecord; /* size = 0x2 */

/*
 * The birdie badges and the Get Character matchups won. Routines that update
 * them reach them through a pointer to this; where it ends is unknown.
 */
typedef struct Awards {
  /* 0x000 */ u8 unusedBeforeBadges[0xF4];
  /* 0x0F4 */ s8
      birdieBadgeCounts[COURSE_COUNT]
                       [COURSE_HOLE_COUNT]; /* Nonzero holds the badge */
  /* By the player's character, then the computer's */
  /* 0x160 */ MatchupRecord matchups[CHARACTER_COUNT][CHARACTER_COUNT];
} Awards; /* size = 0x3E8 */

/* The save's records, which BuildStatusData reads through a pointer to them */
typedef struct SaveRecords {
  /* 0x000 */ Awards awards;
  /* Each best time's net score to par, which breaks ties between equal times */
  /* 0x3E8 */ s32 bestTimeScoresToPar[COURSE_COUNT][BEST_TIME_COUNT];
  /* 0x430 */ s32 bestTimes[COURSE_COUNT]
                           [BEST_TIME_COUNT]; /* Fastest first; unset is -1 */
  /* The character and color of each best time's golfer */
  /* 0x478 */ s32 bestTimeCharacters[COURSE_COUNT][BEST_TIME_COUNT];
  /* 0x4C0 */ s32 bestTimePalettes[COURSE_COUNT][BEST_TIME_COUNT];
  /* 0x508 */ CourseStats courseStats[COURSE_COUNT];
  /* 0x7C0 */ u8 unusedAfterCourseStats[0x990 - 0x7C0];
  /* 0x990 */ s8 hasRingShotStar[RING_SHOT_COURSE_COUNT][RING_SHOT_HOLE_COUNT];
} SaveRecords; /* size = 0x9B4 */

typedef struct SaveData {
  /* 0x0000 */ u32 magic;
  /* 0x0004 */ u32
      longestDisplayListLength; /* In commands, over every frame drawn */
  /* 0x0008 */ u8 invertedBuildDate[0x20]; /* The writing build's date, with
                                              each bit inverted */
  /* 0x0028 */ s8 options[SAVE_OPTION_COUNT];
  /* 0x002C */ u8 unusedAfterOptions[0x38 - 0x2C];
  /* 0x0038 */ SavedGame modeGames[MODE_GAME_COUNT];
  /* 0x0218 */ Player modePlayers[MODE_PLAYER_COUNT];
  /* 0x0720 */ PlayRecord modeRecords[MODE_RECORD_COUNT];
  /* 0x0990 */ SaveRecords records;
  /* 0x1344 */ s8 ringShotClearCounts[RING_SHOT_COURSE_COUNT]
                                     [RING_SHOT_HOLE_COUNT][CHARACTER_COUNT];
  /* 0x15CC */ s32 coursePoints; /* Earned in total, so it never goes down */
  /* 0x15D0 */ PlayRecord rankedRecords[RANKED_RECORD_COUNT];
  /* 0x1DF0 */ RingShotRecord ringShotRecords[RING_SHOT_RECORD_COUNT];
  /* 0x1FF8 */ SavedGame savedGames[SAVED_GAME_COUNT];
  /* 0x21D8 */ Player savedPlayers[SAVED_GAME_COUNT][MAX_PLAYER_COUNT];
} SaveData; /* size = 0x2A78 */

/* Each copy is stored with this, to check that it was read back intact */
typedef struct SaveCheck {
  /* 0x0 */ u16 key; /* Random */
  /* 0x2 */ u16 keyedCrc; /* The data's CRC-16 exclusive-ored with the key */
} SaveCheck;

/* Where a copy of the save data and its check are kept */
typedef struct SaveCopyLocation {
  /* 0x0 */ const char* dataFileName; /* On the development host */
  /* 0x4 */ const char* checkFileName; /* On the development host */
  /* 0x8 */ u32 dataSramAddress;
  /* 0xC */ u32 checkSramAddress;
} SaveCopyLocation;

SaveData* GetSaveData(void);
void TransferSaveData(SaveData* save, u8 isWrite);
void ResetSaveData(void);
void InitSaveData(void);
void RecordDisplayListLength(u32 commandCount);
void WriteSaveData(void);
s32 CountOpenCourses(s32 coursePoints);
s32 GetCoursePointsNeeded(CourseIndex courseIndex);

#endif
