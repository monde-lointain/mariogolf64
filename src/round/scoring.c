/*
 * Scoring: the round's holes, handicaps and net scores, and who wins a hole of
 * a match or of a group.
 */

#include "round/scoring.h"

#include <ultra64.h>

#include "course/course.h"
#include "debug/game_flags.h"
#include "game/scene.h"
#include "math/math_util.h"
#include "menus/menu.h"
#include "round/game_state.h"
#include "round/player.h"
#include "system/c_language.h"

#define DEBUG_ROUND_HOLE_COUNT 2
// Above any score, so the first player compared sets the best one
#define HIGHER_THAN_ANY_SCORE 0x40000000

s32 GetFirstHoleIndex(void) {
    return (g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE) ? HOLES_PER_NINE : 0;
}

s32 GetRoundHoleCount(void) {
    if (IsGameFlagSet(GAME_FLAG_TWO_HOLE_ROUNDS)) {
        return DEBUG_ROUND_HOLE_COUNT;
    }

    if (g_GameState.holeSelection & (HOLE_SELECTION_BACK_NINE | HOLE_SELECTION_FRONT_NINE)) {
        return HOLES_PER_NINE;
    }

    return COURSE_HOLE_COUNT;
}

s32 GetLastHoleIndex(void) {
    return GetFirstHoleIndex() + GetRoundHoleCount() - 1;
}

Boolean IsOnDrivingRange(void) {
    return g_GameState.courseIndex == DRIVING_RANGE_COURSE_INDEX;
}

// The strokes that a handicap takes off a score on the hole: one for each
// full 18, and one more on the holes whose handicap is within the rest.
// GetCourseRecordIndex ignores the courseIndex passed to it and reads
// g_GameState.courseIndex.
s32 GetHandicapOffset(CourseIndex courseIndex, s32 holeIndex, s32 handicap) {
    return (-handicap / COURSE_HOLE_COUNT) -
        (handicap % COURSE_HOLE_COUNT >=
            g_CourseRecords[GetCourseRecordIndex(courseIndex)].holes[holeIndex].handicap);
}

// strokeCount with the current hole's handicap offset, which the matches
// compare
s32 GetNetScore(s32 playerIndex) {
    return g_Players[playerIndex].strokeCount +
        GetHandicapOffset(
            g_GameState.courseIndex,
            g_GameState.holeIndex,
            g_Players[playerIndex].handicap
        );
}

// Numbers each course's holes from 1 to 18, keeping the handicaps' order
void RenumberHoleHandicaps(void) {
    s32 handicaps[COURSE_HOLE_COUNT];
    s32 order[COURSE_HOLE_COUNT];
    CourseRecordIndex recordIndex;
    s32 holeIndex;

    for (recordIndex = 0; recordIndex < COURSE_RECORD_DRIVING_RANGE; recordIndex++) {
        for (holeIndex = 0; holeIndex < COURSE_HOLE_COUNT; holeIndex++) {
            handicaps[holeIndex] = g_CourseRecords[recordIndex].holes[holeIndex].handicap;
        }

        SortIndicesDescending(handicaps, order, COURSE_HOLE_COUNT);
        for (holeIndex = 0; holeIndex < COURSE_HOLE_COUNT; holeIndex++) {
            g_CourseRecords[recordIndex].holes[order[holeIndex]].handicap =
                COURSE_HOLE_COUNT - holeIndex;
        }
    }
}

// Compares the holes before g_GameState.holeIndex by strokes, without handicaps
MatchStanding GetMatchStanding(s32 playerIndex, s32 opponentIndex) {
    s32 holesAfterCurrent;
    s32 lead;
    s32 holeIndex;

    lead = 0;
    holesAfterCurrent = GetFirstHoleIndex() + GetRoundHoleCount();
    if (g_GameState.isSuddenDeath) {
        return MATCH_STANDING_DECIDING_HOLE;
    }

    holesAfterCurrent = holesAfterCurrent - g_GameState.holeIndex - 1;
    for (holeIndex = GetFirstHoleIndex(); holeIndex < g_GameState.holeIndex; holeIndex++) {
        if (g_Players[playerIndex].holes[holeIndex].strokeCount <
            g_Players[opponentIndex].holes[holeIndex].strokeCount) {
            lead++;
        }
        else if (g_Players[opponentIndex].holes[holeIndex].strokeCount <
            g_Players[playerIndex].holes[holeIndex].strokeCount) {
            lead--;
        }
    }

    if (Abs(lead) == holesAfterCurrent + 1) {
        if (lead < 0) {
            return MATCH_STANDING_OPPONENT_DORMIE;
        }

        return MATCH_STANDING_PLAYER_DORMIE;
    }

    if (holesAfterCurrent + 1 < Abs(lead)) {
        return MATCH_STANDING_DECIDED;
    }

    if (Abs(lead) == holesAfterCurrent && holesAfterCurrent == 0) {
        return MATCH_STANDING_DECIDING_HOLE;
    }

    return MATCH_STANDING_OPEN;
}

// Compares the holes before g_GameState.holeIndex by net score. With
// isSuddenDeath set, the first hole that either player wins decides.
MatchSide GetMatchWinner(s32 playerIndex, s32 opponentIndex) {
    s32 endHoleIndex;
    s32 holesAfterCurrent;
    s32 lead;
    s32 holeIndex;
    s32 playerScore;
    s32 opponentScore;

    endHoleIndex = GetFirstHoleIndex() + GetRoundHoleCount();
    lead = 0;
    if (g_GameState.isSuddenDeath) {
        for (holeIndex = GetFirstHoleIndex(); holeIndex < g_GameState.holeIndex; holeIndex++) {
            playerScore = g_Players[playerIndex].holes[holeIndex].strokeCount +
                GetHandicapOffset(
                    g_GameState.courseIndex,
                    holeIndex,
                    g_Players[playerIndex].handicap
                );
            opponentScore = g_Players[opponentIndex].holes[holeIndex].strokeCount +
                GetHandicapOffset(
                    g_GameState.courseIndex,
                    holeIndex,
                    g_Players[opponentIndex].handicap
                );
            if (playerScore < opponentScore) {
                return MATCH_SIDE_PLAYER;
            }

            if (opponentScore < playerScore) {
                return MATCH_SIDE_OPPONENT;
            }
        }

        return MATCH_SIDE_NONE;
    }

    holesAfterCurrent = endHoleIndex - g_GameState.holeIndex - 1;
    for (holeIndex = GetFirstHoleIndex(); holeIndex < g_GameState.holeIndex; holeIndex++) {
        playerScore = g_Players[playerIndex].holes[holeIndex].strokeCount +
            GetHandicapOffset(g_GameState.courseIndex, holeIndex, g_Players[playerIndex].handicap);
        opponentScore = g_Players[opponentIndex].holes[holeIndex].strokeCount +
            GetHandicapOffset(
                g_GameState.courseIndex,
                holeIndex,
                g_Players[opponentIndex].handicap
            );
        lead += playerScore < opponentScore;
        lead -= opponentScore < playerScore;
    }

    if (Abs(lead) > holesAfterCurrent + 1) {
        if (lead > 0) {
            return MATCH_SIDE_PLAYER;
        }

        return MATCH_SIDE_OPPONENT;
    }

    return MATCH_SIDE_NONE;
}

// Whether the hole is halved for the side that the standing says is dormie: it
// is when that side is done with the hole and its net score is no more than one
// over the other side's
#define IsHalvedForDormie(standing, dormieStanding, dormieIndex, otherIndex) \
    (((standing) == (dormieStanding)) && \
        (g_Players[dormieIndex].holeStatus != HOLE_STATUS_PLAYING) && \
        !(GetNetScore(dormieIndex) > GetNetScore(otherIndex) + 1))

// Scores a player still HOLE_STATUS_PLAYING as if they need at least one more
// stroke. When a dormie lead means a halve wins the match, the hole counts
// as halved once the trailing player can't win it.
MatchSide GetMatchHoleResult(s32 playerIndex, s32 opponentIndex) {
    MatchStanding standing;

    if (g_Players[playerIndex].holeStatus != HOLE_STATUS_PLAYING ||
        g_Players[opponentIndex].holeStatus != HOLE_STATUS_PLAYING) {
        if (g_Players[playerIndex].holeStatus != HOLE_STATUS_PLAYING) {
            if (g_Players[opponentIndex].holeStatus != HOLE_STATUS_PLAYING) {
                if (GetNetScore(playerIndex) < GetNetScore(opponentIndex)) {
                    return MATCH_SIDE_PLAYER;
                }

                return (GetNetScore(opponentIndex) < GetNetScore(playerIndex)) ?
                    MATCH_SIDE_OPPONENT :
                    MATCH_HOLE_HALVED;
            }

            if (GetNetScore(playerIndex) < GetNetScore(opponentIndex) + 1) {
                return MATCH_SIDE_PLAYER;
            }
        }

        if (g_Players[opponentIndex].holeStatus != HOLE_STATUS_PLAYING &&
            GetNetScore(opponentIndex) < GetNetScore(playerIndex) + 1) {
            return MATCH_SIDE_OPPONENT;
        }

        standing = GetMatchStanding(playerIndex, opponentIndex);
        if (IsHalvedForDormie(standing, MATCH_STANDING_PLAYER_DORMIE, playerIndex, opponentIndex)) {
            return MATCH_HOLE_HALVED;
        }

        if (IsHalvedForDormie(
                standing,
                MATCH_STANDING_OPPONENT_DORMIE,
                opponentIndex,
                playerIndex
            )) {
            return MATCH_HOLE_HALVED;
        }
    }

    return MATCH_SIDE_NONE;
}

// Decides the hole among all the players, marking each in playerResults
// when it isn't NULL. Players who have holed out set the best score, and
// those still playing are scored as if they need another stroke.
GroupHoleResult GetGroupHoleResult(GroupPlayerMark *playerResults) {
    s8 ignoredResults[PLAYER_COUNT_MAX];
    s32 bestScore;
    s32 bestPlayerIndex;
    s32 tieCount;
    Boolean isDecided;
    s32 outCount;
    s32 playerIndex;

    bestScore = HIGHER_THAN_ANY_SCORE;
    bestPlayerIndex = -1;
    isDecided = FALSE;
    tieCount = 0;
    outCount = 0;
    if (playerResults == NULL) {
        playerResults = ignoredResults;
    }

    for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
        if (g_Players[playerIndex].holeStatus == HOLE_STATUS_HOLED_OUT) {
            if (GetNetScore(playerIndex) < bestScore) {
                bestScore = GetNetScore(playerIndex);
                bestPlayerIndex = playerIndex;
                tieCount = 0;
            }
            else {
                tieCount += (bestScore == GetNetScore(playerIndex));
            }
        }
        else {
            outCount += (g_Players[playerIndex].holeStatus == HOLE_STATUS_OUT);
        }
    }

    if (bestPlayerIndex == -1) {
        for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
            playerResults[playerIndex] = GROUP_PLAYER_CONTENDING;
        }

        if (outCount == g_GameState.playerCount) {
            return GROUP_HOLE_NO_WINNER;
        }

        return isDecided;
    }

    isDecided = TRUE;
    for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
        playerResults[playerIndex] = GROUP_PLAYER_CONTENDING;
        if (g_Players[playerIndex].holeStatus == HOLE_STATUS_PLAYING) {
            if (GetNetScore(playerIndex) >= bestScore) {
                playerResults[playerIndex] = GROUP_PLAYER_OUT;
            }
            else if (GetNetScore(playerIndex) < bestScore - 1) {
                isDecided = FALSE;
            }
            else if (tieCount == 0) {
                // Holing the next stroke would tie the lone leader
                isDecided = FALSE;
            }
        }
        else if (bestScore < GetNetScore(playerIndex) ||
            g_Players[playerIndex].holeStatus >= HOLE_STATUS_NEWLY_OUT) {
            playerResults[playerIndex] = GROUP_PLAYER_OUT;
        }
    }

    if (!isDecided) {
        return GROUP_HOLE_OPEN;
    }

    if (tieCount > 0) {
        return GROUP_HOLE_NO_WINNER;
    }

    for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
        if (g_Players[playerIndex].holeStatus == HOLE_STATUS_HOLED_OUT &&
            GetNetScore(playerIndex) == bestScore) {
            playerResults[playerIndex] = GROUP_PLAYER_WINNER;
        }
    }

    return GROUP_HOLE_WON;
}

// Compares the player's net score plus one, which holing the next stroke
// would give, with the other players' scores. Modes 2 and 4 compare the
// player with the one whose index differs in the lowest bit, and modes 10
// and 11 with every player.
s32 GetHolingOutStakes(s32 playerIndex) {
    HolingOutStakes stakes;
    s32 opponentIndex;
    Boolean hasAnyFinished;
    s32 bestScore;
    s32 otherIndex;

    stakes = HOLING_OUT_STAKES_NONE;
    switch (g_GameState.gameMode) {
        case GAME_MODE_MATCH_GAME:
        case GAME_MODE_GET_CHARACTER:
            opponentIndex = playerIndex ^ 1;
            if (GetNetScore(playerIndex) == GetNetScore(opponentIndex) - 1 &&
                g_Players[opponentIndex].holeStatus != HOLE_STATUS_PLAYING) {
                stakes = HOLING_OUT_HALVES_HOLE;
                if (GetMatchStanding(playerIndex, opponentIndex) == MATCH_STANDING_PLAYER_DORMIE) {
                    stakes = HOLING_OUT_WINS_MATCH;
                }
            }
            else if (GetNetScore(playerIndex) < GetNetScore(opponentIndex)) {
                stakes =
                    (GetMatchStanding(playerIndex, opponentIndex) == MATCH_STANDING_DECIDING_HOLE) ?
                    HOLING_OUT_WINS_MATCH :
                    HOLING_OUT_WINS_HOLE;
            }
            else if (GetNetScore(playerIndex) < GetNetScore(opponentIndex) + 1 &&
                g_Players[opponentIndex].holeStatus != HOLE_STATUS_HOLED_OUT &&
                GetMatchStanding(playerIndex, opponentIndex) == MATCH_STANDING_PLAYER_DORMIE) {
                stakes = HOLING_OUT_WINS_MATCH;
            }

            break;
        case GAME_MODE_SKINS_MATCH:
        case GAME_MODE_CLUB_SLOTS:
            hasAnyFinished = FALSE;
            bestScore = HIGHER_THAN_ANY_SCORE;
            stakes = HOLING_OUT_WINS_HOLE;
            for (otherIndex = 0; otherIndex < g_GameState.playerCount; otherIndex++) {
                if (g_Players[otherIndex].holeStatus == HOLE_STATUS_HOLED_OUT) {
                    hasAnyFinished = TRUE;
                    if (GetNetScore(otherIndex) < bestScore) {
                        bestScore = GetNetScore(otherIndex);
                    }
                }
            }

            for (otherIndex = 0; otherIndex < g_GameState.playerCount; otherIndex++) {
                if (otherIndex != playerIndex &&
                    g_Players[otherIndex].holeStatus == HOLE_STATUS_PLAYING) {
                    if (GetNetScore(otherIndex) < GetNetScore(playerIndex)) {
                        stakes = HOLING_OUT_STAKES_NONE;
                        break;
                    }

                    if (GetNetScore(otherIndex) == GetNetScore(playerIndex)) {
                        stakes = HOLING_OUT_TIES;
                    }
                }
            }

            if (stakes != HOLING_OUT_STAKES_NONE) {
                if (hasAnyFinished) {
                    if (GetNetScore(playerIndex) + 1 == bestScore) {
                        stakes = HOLING_OUT_TIES;
                    }

                    if (GetNetScore(playerIndex) + 1 < bestScore && stakes == HOLING_OUT_TIES) {
                        stakes = HOLING_OUT_STAKES_NONE;
                    }
                }
                else if (stakes == HOLING_OUT_TIES) {
                    stakes = HOLING_OUT_STAKES_NONE;
                }
            }

            break;
    }

    return stakes;
}
