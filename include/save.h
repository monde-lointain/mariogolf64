#ifndef SAVE_H
#define SAVE_H

#include <ultra64.h>

#include "characters/character.h"
#include "course.h"
#include "course_stats.h"
#include "game_state.h"
#include "hole_setup.h"
#include "play_record.h"
#include "player.h"
#include "round_stats.h"
#include "system/c_language.h"

#define SAVE_MAGIC 0x12345678

#define SAVE_COPY_COUNT 2
#define RING_SHOT_RECORD_COUNT 5
#define RANKED_RECORD_COUNT 20
#define SAVED_GAME_COUNT 3

#define MODE_GAME_COUNT 3
#define MODE_PLAYER_COUNT 7
#define MODE_RECORD_COUNT 6

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

#define SAVE_OPTION_MUSIC_OFF 0
#define SAVE_OPTION_MONO 1
#define SAVE_OPTION_GRID_COLOR 2
#define SAVE_OPTION_SIMPLE_CAMERA 3
#define SAVE_OPTION_COUNT 4

#define OPTION_BIT_MUSIC_OFF (1 << 0)
#define OPTION_BIT_MONO (1 << 1)
#define OPTION_BIT_SIMPLE_CAMERA (1 << 2)
extern u8 g_SoundOptionBits;

enum SaveReadResult {
    SAVE_READ_NEVER_WRITTEN = -2,
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

#define BEST_TIME_COUNT 3
#define NO_BEST_TIME (-1)
#define NO_BEST_TIME_HOLDER (-1)

#define WIN_COUNT_MASK 0x7F
#define WON_UNSAVED_FLAG 0x80

typedef struct MatchupRecord {
    u8 lossCount;
    u8 winBits;
} MatchupRecord;

typedef struct Awards {
    u8 unusedBeforeBadges[0xF4];
    s8 birdieBadgeCounts[COURSE_COUNT][COURSE_HOLE_COUNT];
    MatchupRecord matchups[CHARACTER_COUNT][CHARACTER_COUNT];
} Awards;

typedef struct SaveRecords {
    Awards awards;
    s32 bestTimeScoresToPar[COURSE_COUNT][BEST_TIME_COUNT];
    s32 bestTimes[COURSE_COUNT][BEST_TIME_COUNT];
    CharacterIndex bestTimeCharacters[COURSE_COUNT][BEST_TIME_COUNT];
    CButtonPalette bestTimePalettes[COURSE_COUNT][BEST_TIME_COUNT];
    CourseStats courseStats[COURSE_COUNT];
    u8 unusedAfterCourseStats[0x990 - 0x7C0];
    Boolean8 hasRingShotStar[RING_SHOT_COURSE_COUNT][RING_SHOT_HOLE_COUNT];
} SaveRecords;

typedef struct SaveData {
    u32 magic;
    u32 longestDisplayListLength;
    u8 invertedBuildDate[0x20];
    s8 options[SAVE_OPTION_COUNT];
    u8 unusedAfterOptions[0x38 - 0x2C];
    SavedGame modeGames[MODE_GAME_COUNT];
    Player modePlayers[MODE_PLAYER_COUNT];
    PlayRecord modeRecords[MODE_RECORD_COUNT];
    SaveRecords records;
    s8 ringShotClearCounts[RING_SHOT_COURSE_COUNT][RING_SHOT_HOLE_COUNT][CHARACTER_COUNT];
    s32 coursePoints;
    PlayRecord rankedRecords[RANKED_RECORD_COUNT];
    RingShotRecord ringShotRecords[RING_SHOT_RECORD_COUNT];
    SavedGame savedGames[SAVED_GAME_COUNT];
    Player savedPlayers[SAVED_GAME_COUNT][PLAYER_COUNT_MAX];
} SaveData;

typedef struct SaveCheck {
    u16 key;
    u16 keyedCrc;
} SaveCheck;

typedef struct SaveCopyLocation {
    const char *dataFileName;
    const char *checkFileName;
    u32 dataSramAddress;
    u32 checkSramAddress;
} SaveCopyLocation;

SaveData *GetSaveData(void);
void TransferSaveData(SaveData *save, UBoolean8 isWrite);
void ResetSaveData(void);
void InitSaveData(void);
void RecordDisplayListLength(u32 commandCount);
void WriteSaveData(void);
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
