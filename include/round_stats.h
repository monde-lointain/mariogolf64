#ifndef ROUND_STATS_H
#define ROUND_STATS_H

#include "common.h"

#include "save.h"

/* What each player has won on each BET_* bet this game */
extern s16 g_BetTallies[MAX_PLAYER_COUNT][BET_COUNT];
extern RoundStats g_RoundStats;
/* The pots of the contests and of the hole, which a tie carries over */
extern s32 g_LongestDrivePot;
extern s32 g_NearestPinPot;
extern s32 g_HolePot;

#define NO_LAST_HOLE_SKINS -1

/* What GetContestWinner returns when nobody won the contest outright */
#define NO_CONTEST_WINNER -1

/* How TallySkins left the skins at the last tallied hole */
typedef struct SkinsCarryover {
  /* 0x0 */ s8
      intoLastHole; /* The skins carried into it, or NO_LAST_HOLE_SKINS */
  /* 0x1 */ s8 onLastHole;   /* The skins at stake on it */
  /* 0x2 */ s8 fromLastHole; /* The skins carried over from it */
} SkinsCarryover;            /* size = 0x3 */

s32 GetNetScoreToPar(s32 endHole, s32 playerIndex);
void TallySkins(s32 endHole);
void StartBets(void);
void SettleHoleBets(void);
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
