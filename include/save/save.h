/*
 * The layout of the save data in SRAM, and the routines that read and write it.
 */

#ifndef SAVE_H
#define SAVE_H

#include <ultra64.h>

#include "characters/character.h"
#include "course/course.h"
#include "course/hole_setup.h"
#include "round/game_state.h"
#include "round/player.h"
#include "round/round_stats.h"
#include "menus/menu_memory.h"
#include "save/course_stats.h"
#include "save/play_record.h"
#include "system/c_language.h"

// Marks save data that has been written at least once
#define SAVE_MAGIC 0x12345678

#define SAVE_COPY_COUNT 2
#define RING_SHOT_RECORD_COUNT 5
#define RANKED_RECORD_COUNT 20
// The games saved partway through, to continue later
#define SAVED_GAME_COUNT 3

// The games kept for continuing by game mode rather than by slot, and the
// players and records kept with them
#define MODE_GAME_COUNT 3
#define MODE_PLAYER_COUNT 7
#define MODE_RECORD_COUNT 6

// Where each game mode keeps its game in the save's mode tables. Mode 4's
// two records overlap mode 5's one.
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

// Indices of SaveData's options
// The options screen's MUSIC set to OFF
#define SAVE_OPTION_MUSIC_OFF 0
// Plays the sound in mono instead of stereo
#define SAVE_OPTION_MONO 1
// Copied into each player's Player.gridColor
#define SAVE_OPTION_GRID_COLOR 2
// The options screen's CAMERA set to SIMPLE instead of DYNAMIC
#define SAVE_OPTION_SIMPLE_CAMERA 3
#define SAVE_OPTION_COUNT 4

// A copy of MenuMemory's optionBits, from which the music code reads
// OPTION_BIT_MONO
extern UOptionBits8 g_SoundOptionBits;

// Results of reading the save data
enum SaveReadResult {
    SAVE_READ_NEVER_WRITTEN = -2, // Neither copy has the magic number
    SAVE_READ_FAILED,
    SAVE_READ_FIRST_COPY = 1,
    SAVE_READ_SECOND_COPY
};

typedef s32 SaveReadResult;

typedef struct SavedGame {
    Boolean isValid;
    u8 unusedBeforeHole[4];
    HoleSetup hole;
    GameState state;
    RoundStats roundStats;
    u8 unusedAfterRoundStats[4];
} SavedGame;

// The best times kept for each course, with what was recorded alongside
#define BEST_TIME_COUNT 3
#define NO_BEST_TIME (-1)
#define NO_BEST_TIME_HOLDER (-1)

// MatchupRecord.winBits keeps a count of wins in these bits
#define WIN_COUNT_MASK 0x7F
// Set by a win in a game that was never saved partway; the character table
// shows such a matchup with a star
#define WON_UNSAVED_FLAG 0x80

// A character's record against a computer character in Get Character
typedef struct MatchupRecord {
    u8 lossCount;
    u8 winBits; // The win count, and WON_UNSAVED_FLAG
} MatchupRecord;

// The birdie badges and the Get Character matchups won. Routines that update
// them reach them through a pointer to this; where it ends is unknown.
typedef struct Awards {
    u8 unusedBeforeBadges[0xF4];
    s8 birdieBadgeCounts[COURSE_COUNT][COURSE_HOLE_COUNT]; // Nonzero holds the badge
    // By the player's character, then the computer's
    MatchupRecord matchups[CHARACTER_COUNT][CHARACTER_COUNT];
} Awards;

// The save's records, which BuildStatusData reads through a pointer to them
typedef struct SaveRecords {
    Awards awards;
    // Each best time's net score to par, which breaks ties between equal times
    s32 bestTimeScoresToPar[COURSE_COUNT][BEST_TIME_COUNT];
    s32 bestTimes[COURSE_COUNT][BEST_TIME_COUNT]; // Fastest first; unset is -1
    // The character and color of each best time's golfer
    CharacterIndex bestTimeCharacters[COURSE_COUNT][BEST_TIME_COUNT];
    CButtonPalette bestTimePalettes[COURSE_COUNT][BEST_TIME_COUNT];
    CourseStats courseStats[COURSE_COUNT];
    u8 unusedAfterCourseStats[0x990 - 0x7C0];
    Boolean8 hasRingShotStar[RING_SHOT_COURSE_COUNT][RING_SHOT_HOLE_COUNT];
} SaveRecords;

typedef struct SaveData {
    u32 magic;
    u32 longestDisplayListLength; // In commands, over every frame drawn
    u8 invertedBuildDate[0x20]; // The writing build's date, with each bit inverted
    s8 options[SAVE_OPTION_COUNT];
    u8 unusedAfterOptions[0x38 - 0x2C];
    SavedGame modeGames[MODE_GAME_COUNT];
    Player modePlayers[MODE_PLAYER_COUNT];
    PlayRecord modeRecords[MODE_RECORD_COUNT];
    SaveRecords records;
    s8 ringShotClearCounts[RING_SHOT_COURSE_COUNT][RING_SHOT_HOLE_COUNT][CHARACTER_COUNT];
    s32 coursePoints; // Earned in total, so it never goes down
    PlayRecord rankedRecords[RANKED_RECORD_COUNT];
    RingShotRecord ringShotRecords[RING_SHOT_RECORD_COUNT];
    SavedGame savedGames[SAVED_GAME_COUNT];
    Player savedPlayers[SAVED_GAME_COUNT][PLAYER_COUNT_MAX];
} SaveData;

// Each copy is stored with this, to check that it was read back intact
typedef struct SaveCheck {
    u16 key; // Random
    u16 keyedCrc; // The data's CRC-16 exclusive-ored with the key
} SaveCheck;

// Where a copy of the save data and its check are kept
typedef struct SaveCopyLocation {
    const char *dataFileName; // On the development host
    const char *checkFileName; // On the development host
    u32 dataSramAddress;
    u32 checkSramAddress;
} SaveCopyLocation;

SaveData *GetSaveData(void);
void TransferSaveData(SaveData *save, UBoolean8 isWrite);
void ResetSaveData(void);
void InitSaveData(void);
void RecordDisplayListLength(u32 commandCount);
void WriteSaveData(void);
// The option queries return a byte, but the original's main_menu.c,
// shot_camera.c and sound.c use the whole return register, as if the result
// were a word. Those files define WIDENS_CHOSEN_OPTIONS to see it so, unless
// AVOID_UB is set.
#if defined(AVOID_UB) || !defined(WIDENS_CHOSEN_OPTIONS)
Boolean8 IsMusicOffChosen(void);
Boolean8 IsMonoChosen(void);
Boolean8 IsSimpleCameraChosen(void);
#else
Boolean IsMusicOffChosen(void);
Boolean IsMonoChosen(void);
Boolean IsSimpleCameraChosen(void);
#endif
s32 CountOpenCourses(s32 coursePoints);
s32 GetCoursePointsNeeded(CourseIndex courseIndex);

#endif
