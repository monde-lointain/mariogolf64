#ifndef SCORING_H
#define SCORING_H

#include <ultra64.h>

#include "course.h"
#include "language.h"

/* GetMatchStanding's results, from the first player's side */
enum MatchStanding {
  MATCH_STANDING_DECIDED = -1, /* The lead is more than the holes left */
  MATCH_STANDING_OPEN,
  /* The player leads by the holes left */
  MATCH_STANDING_PLAYER_DORMIE,
  MATCH_STANDING_OPPONENT_DORMIE,
  MATCH_STANDING_DECIDING_HOLE /* Whoever wins the next hole wins */
};

/* The side that GetMatchWinner and GetMatchHoleResult return */
enum MatchSide {
  MATCH_SIDE_NONE,
  MATCH_SIDE_PLAYER,
  MATCH_SIDE_OPPONENT,
  MATCH_HOLE_HALVED
};

/* GetGroupHoleResult's results */
enum GroupHoleResult {
  GROUP_HOLE_OPEN,
  GROUP_HOLE_WON,
  GROUP_HOLE_NO_WINNER
};

/* The marks GetGroupHoleResult gives each player */
enum GroupPlayerMark {
  GROUP_PLAYER_CONTENDING,
  GROUP_PLAYER_WINNER,
  GROUP_PLAYER_OUT
};

/* A hole's strokes against its par */
enum ScoreToPar {
  SCORE_TO_PAR_ALBATROSS = -3,
  SCORE_TO_PAR_EAGLE,
  SCORE_TO_PAR_BIRDIE,
  SCORE_TO_PAR_PAR,
  SCORE_TO_PAR_BOGEY,
  SCORE_TO_PAR_DOUBLE_BOGEY,
  SCORE_TO_PAR_TRIPLE_BOGEY
};

/* Whether a score to par lies from best to worst, both included */
#define IsScoreToParWithin(score, best, worst) \
  ((score) >= (best) && (score) <= (worst))

/* What GetHolingOutStakes says the player's next stroke would do if holed */
enum HolingOutStakes {
  HOLING_OUT_WINS_HOLE,
  HOLING_OUT_TIES,
  HOLING_OUT_WINS_MATCH,
  HOLING_OUT_STAKES_NONE,
  HOLING_OUT_HALVES_HOLE
};

s32 GetFirstHoleIndex(void);
s32 GetRoundHoleCount(void);
s32 GetLastHoleIndex(void);
Boolean IsOnDrivingRange(void);
s32 GetHandicapOffset(CourseIndex courseIndex, s32 holeIndex, s32 handicap);
s32 GetNetScore(s32 playerIndex);
void RenumberHoleHandicaps(void);
s32 GetMatchStanding(s32 playerIndex, s32 opponentIndex);
s32 GetMatchWinner(s32 playerIndex, s32 opponentIndex);
s32 GetMatchHoleResult(s32 playerIndex, s32 opponentIndex);
s32 GetGroupHoleResult(s8* playerResults);
s32 GetHolingOutStakes(s32 playerIndex);

#endif
