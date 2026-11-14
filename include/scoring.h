#ifndef SCORING_H
#define SCORING_H

#include <ultra64.h>

#include "course.h"
#include "language.h"

enum MatchStanding {
    MATCH_STANDING_DECIDED = -1,
    MATCH_STANDING_OPEN,
    MATCH_STANDING_PLAYER_DORMIE,
    MATCH_STANDING_OPPONENT_DORMIE,
    MATCH_STANDING_DECIDING_HOLE
};

typedef s32 MatchStanding;

enum MatchSide {
    MATCH_SIDE_NONE,
    MATCH_SIDE_PLAYER,
    MATCH_SIDE_OPPONENT,
    MATCH_HOLE_HALVED
};

typedef s32 MatchSide;

enum GroupHoleResult {
    GROUP_HOLE_OPEN,
    GROUP_HOLE_WON,
    GROUP_HOLE_NO_WINNER
};

typedef s32 GroupHoleResult;

enum GroupPlayerMark {
    GROUP_PLAYER_CONTENDING,
    GROUP_PLAYER_WINNER,
    GROUP_PLAYER_OUT
};

typedef s8 GroupPlayerMark;

enum ScoreToPar {
    SCORE_TO_PAR_ALBATROSS = -3,
    SCORE_TO_PAR_EAGLE,
    SCORE_TO_PAR_BIRDIE,
    SCORE_TO_PAR_PAR,
    SCORE_TO_PAR_BOGEY,
    SCORE_TO_PAR_DOUBLE_BOGEY,
    SCORE_TO_PAR_TRIPLE_BOGEY
};

typedef s32 ScoreToPar;

#define IsScoreToParWithin(score, best, worst) ((score) >= (best) && (score) <= (worst))

enum HolingOutStakes {
    HOLING_OUT_WINS_HOLE,
    HOLING_OUT_TIES,
    HOLING_OUT_WINS_MATCH,
    HOLING_OUT_STAKES_NONE,
    HOLING_OUT_HALVES_HOLE
};

typedef s32 HolingOutStakes;

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
s32 GetGroupHoleResult(GroupPlayerMark *playerResults);
s32 GetHolingOutStakes(s32 playerIndex);

#endif
