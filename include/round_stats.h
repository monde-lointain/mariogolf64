#ifndef ROUND_STATS_H
#define ROUND_STATS_H

#include "common.h"
#include "save.h"

/* What each player has won on each BET_* bet this game */
extern s16 g_BetTallies[MAX_PLAYER_COUNT][BET_COUNT];
extern RoundStats g_RoundStats;

void StartBets(void);
void ResumeBets(void);
s32 GetContestWinner(void);
void SetContestWinner(s32 playerIndex);
void ClearContestWinner(void);
void UpdateContest(s32 isHoleOver);
void StartRoundStats(void);
void RestartRoundStats(void);
void RecordHoleStats(void);
s32 RecordDrive(void);

#endif
