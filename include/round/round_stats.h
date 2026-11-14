#ifndef ROUND_STATS_H
#define ROUND_STATS_H

#include <ultra64.h>

#include "round/bets.h"
#include "round/player.h"
#include "system/c_language.h"

typedef struct RoundStats {
    s32 sandSaveCount;
    s32 missedSandSaveCount;
    s32 measuredDriveCount;
    s32 longestDrive;
    s32 averagedDriveTotal;
    s32 averagedDriveCount;
    Boolean isDriveAveraged;
} RoundStats;

extern s16 g_BetTallies[PLAYER_COUNT_MAX][BET_COUNT];
extern RoundStats g_RoundStats;
extern s32 g_LongestDrivePot;
extern s32 g_NearestPinPot;
extern s32 g_HolePot;

#define NO_LAST_HOLE_SKINS (-1)

#define NO_CONTEST_WINNER (-1)

typedef struct SkinsCarryover {
    s8 intoLastHole;
    s8 onLastHole;
    s8 fromLastHole;
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
