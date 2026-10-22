#ifndef SCORING_H
#define SCORING_H

#include <ultra64.h>

/* GetMatchStanding's results, from the first player's side */
#define MATCH_STANDING_DECIDED -1 /* The lead is more than the holes left */
#define MATCH_STANDING_OPEN 0
#define MATCH_STANDING_PLAYER_DORMIE 1 /* The player leads by the holes left */
#define MATCH_STANDING_OPPONENT_DORMIE 2
#define MATCH_STANDING_DECIDING_HOLE 3 /* Whoever wins the next hole wins */

/* The side that GetMatchWinner and GetMatchHoleResult return */
#define MATCH_SIDE_NONE 0
#define MATCH_SIDE_PLAYER 1
#define MATCH_SIDE_OPPONENT 2
#define MATCH_HOLE_HALVED 3

/* GetGroupHoleResult's results, and the marks it gives each player */
#define GROUP_HOLE_OPEN 0
#define GROUP_HOLE_WON 1
#define GROUP_HOLE_NO_WINNER 2
#define GROUP_PLAYER_CONTENDING 0
#define GROUP_PLAYER_WINNER 1
#define GROUP_PLAYER_OUT 2

/* What GetHolingOutStakes says the player's next stroke would do if holed */
#define HOLING_OUT_WINS_HOLE 0
#define HOLING_OUT_TIES 1
#define HOLING_OUT_WINS_MATCH 2
#define HOLING_OUT_STAKES_NONE 3
#define HOLING_OUT_HALVES_HOLE 4

s32 GetFirstHoleIndex(void);
s32 GetRoundHoleCount(void);
s32 GetLastHoleIndex(void);
s32 IsOnDrivingRange(void);
s32 GetHandicapOffset(s32 courseIndex, s32 holeIndex, s32 handicap);
s32 GetNetScore(s32 playerIndex);
void RenumberHoleHandicaps(void);
s32 GetMatchStanding(s32 playerIndex, s32 opponentIndex);
s32 GetMatchWinner(s32 playerIndex, s32 opponentIndex);
s32 GetMatchHoleResult(s32 playerIndex, s32 opponentIndex);
s32 GetGroupHoleResult(s8 *playerResults);
s32 GetHolingOutStakes(s32 playerIndex);

#endif
