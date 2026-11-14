/*
 * The round's statistics and contests, and the course's records of rounds.
 */

#ifndef ROUND_STATS_H
#define ROUND_STATS_H

#include <ultra64.h>

#include "round/bets.h"
#include "round/player.h"
#include "system/c_language.h"

// The current round's statistics, for the course's records
typedef struct RoundStats {
    // Holes played from a bunker in par or fewer strokes, and in more
    s32 sandSaveCount;
    s32 missedSandSaveCount;
    s32 measuredDriveCount;
    s32 longestDrive; // Starts as the course's record
    s32 averagedDriveTotal;
    s32 averagedDriveCount;
    // Whether the drive was hit with the 1W
    Boolean isDriveAveraged;
} RoundStats;

// What each player has won on each BET_* bet this game
extern s16 g_BetTallies[PLAYER_COUNT_MAX][BET_COUNT];
extern RoundStats g_RoundStats;
// The pots of the contests and of the hole, which a tie carries over
extern s32 g_LongestDrivePot;
extern s32 g_NearestPinPot;
extern s32 g_HolePot;

#define NO_LAST_HOLE_SKINS (-1)

// What GetContestWinner returns when nobody won the contest outright
#define NO_CONTEST_WINNER (-1)

// How TallySkins left the skins at the last tallied hole
typedef struct SkinsCarryover {
    s8 intoLastHole; // The skins carried into it, or NO_LAST_HOLE_SKINS
    s8 onLastHole; // The skins at stake on it
    s8 fromLastHole; // The skins carried over from it
} SkinsCarryover;

s32 GetNetScoreToPar(s32 endHole, s32 playerIndex);
void TallySkins(s32 endHole);
void StartBets(void);
void SettleHoleBets(void);
void ResumeBets(void);
s32 GetContestWinner(void);
void SetContestWinner(s32 playerIndex);
void ClearContestWinner(void);
void UpdateContest(Boolean isHoleOver);
void StartRoundStats(void);
void RestartRoundStats(void);
void RecordHoleStats(void);
s32 RecordDrive(void);
void RecordCourseRound(void);

#endif
