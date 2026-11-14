#include "round_stats.h"

#include <ultra64.h>

#include "bets.h"
#include "club.h"
#include "compiler.h"
#include "course.h"
#include "course_stats.h"
#include "debug_text.h"
#include "game_state.h"
#include "language.h"
#include "player.h"
#include "round_hud.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "scorecard.h"
#include "scoring.h"
#include "text.h"

#define NO_SKINS_WINNER (-1)
#define RECORD_TOTAL_LIMIT S32_MAX
#define NO_BEST_SCORE RECORD_TOTAL_LIMIT
#define ROUND_COUNT_LIMIT 9999
#define LOWEST_SCORE_TO_PAR (-54)
#define PAR_ON_SPARE_STROKES 2
#define WORST_HOLE_SCORE 16
#define FRONT_NINE_END 9
#define BACK_NINE_END 18
#define SKINS_DEBUG_COLUMN 17
#define SKINS_DEBUG_ROW 5

#define DRIVE_COMPARISON_SCALE 7.119878e-05f

s16 g_BetTallies[PLAYER_COUNT_MAX][BET_COUNT];
RoundStats g_RoundStats;
Scorecard g_Scorecard;

s32 g_LongestDrivePot = 0;
s32 g_NearestPinPot = 0;
s32 g_HolePot = 0;
static Boolean f_IsContestDecided = FALSE;
s32 g_ScorecardState = SCORECARD_CLOSED;

static s32 f_ContestWinner = NO_CONTEST_WINNER;
static void *f_UnusedPositionRoutines[6] UNUSED = {NULL, NULL, guPosition, NULL, NULL, NULL};

s32 GetNetScoreToPar(s32 endHole, s32 playerIndex) {
    s32 score = 0;
    CourseRecord *course = &g_CourseRecords[GetCourseRecordIndex()];
    s32 hole;

    if (g_GameState.holeIndex == endHole) {
        endHole += g_Players[playerIndex].holeStatus != HOLE_STATUS_PLAYING;
    }

    for (hole = GetFirstHoleIndex(); hole < endHole; hole++) {
        score += g_Players[playerIndex].holes[hole].strokeCount - course->holes[hole].par +
            GetHandicapOffset(g_GameState.courseIndex, hole, g_Players[playerIndex].handicap);
    }

    return score;
}

void TallySkins(s32 endHole) {
    s32 player;
    s32 hole;
    s32 winner;
    s32 skins;

    for (hole = 0; hole != g_GameState.playerCount; hole++) {
        g_RoundHud.skinsTotals[hole] = 0;
    }

    winner = FALSE;
    for (player = 0; player != g_GameState.playerCount; player++) {
        if (g_Players[player].holeStatus == HOLE_STATUS_PLAYING) {
            winner = TRUE;
        }
    }

    endHole += !winner;
    g_Scorecard.skinsCarryover.intoLastHole = NO_LAST_HOLE_SKINS;
    g_Scorecard.skinsCarryover.onLastHole = 0;
    g_Scorecard.skinsCarryover.fromLastHole = 0;
    skins = 1;
    for (hole = GetFirstHoleIndex(); hole < endHole; hole++) {
        winner = NO_SKINS_WINNER;
        for (player = 0; player != g_GameState.playerCount; player++) {
            if (g_Players[player].holes[hole].strokeCount != 0) {
                winner = player;
                g_Players[player].holes[hole].strokeCount = skins;
            }
        }

        if (hole == endHole - 1) {
            g_Scorecard.skinsCarryover.intoLastHole = skins - 1;
            g_Scorecard.skinsCarryover.onLastHole = skins;
        }

        if (winner == NO_SKINS_WINNER) {
            skins++;
            g_Scorecard.skinsCarryover.fromLastHole = skins - 1;
        }
        else {
            g_RoundHud.skinsTotals[winner] += skins;
            skins = 1;
            g_Scorecard.skinsCarryover.fromLastHole = 0;
        }
    }

    sprintf(
        g_PrintBuffer,
        "%2d %d    ",
        g_Scorecard.skinsCarryover.intoLastHole,
        g_Scorecard.skinsCarryover.onLastHole
    );
    PrintDebugText(g_PrintBuffer, SKINS_DEBUG_COLUMN, SKINS_DEBUG_ROW);
}

void StartBets(void) {
    s32 player;
    s32 bet;

    for (player = 0; player != PLAYER_COUNT_MAX; player++) {
        for (bet = 0; bet != BET_COUNT; bet++) {
            g_BetTallies[player][bet] = 0;
        }
    }

    for (player = 0; player != PLAYER_COUNT_MAX; player++) {
        for (bet = 0; bet != BET_COUNT; bet++) {
            g_Players[player].betTallies[bet] = g_BetTallies[player][bet];
        }
    }

    f_IsContestDecided = FALSE;
    f_ContestWinner = NO_CONTEST_WINNER;
    g_LongestDrivePot = g_GameState.betRates[BET_LONGEST_DRIVE];
    g_NearestPinPot = g_GameState.betRates[BET_NEAREST_PIN];
    if (g_GameState.betRates[BET_MATCH_POT] != UNSET_BET_RATE) {
        g_HolePot = g_GameState.betRates[BET_MATCH_POT];
    }

    if (g_GameState.betRates[BET_SKINS] != UNSET_BET_RATE) {
        g_HolePot = g_GameState.betRates[BET_SKINS];
    }

    g_GameState.carriedPots[CARRIED_POT_LONGEST_DRIVE] = g_LongestDrivePot;
    g_GameState.carriedPots[CARRIED_POT_NEAREST_PIN] = g_NearestPinPot;
    g_GameState.carriedPots[CARRIED_POT_HOLE] = g_HolePot;
}

void ResumeBets(void) {
    s32 player;
    s32 bet;

    for (player = 0; player != PLAYER_COUNT_MAX; player++) {
        for (bet = 0; bet != BET_COUNT; bet++) {
            g_BetTallies[player][bet] = g_Players[player].betTallies[bet];
        }
    }

    f_IsContestDecided = FALSE;
    f_ContestWinner = NO_CONTEST_WINNER;
    g_LongestDrivePot = g_GameState.carriedPots[CARRIED_POT_LONGEST_DRIVE];
    g_NearestPinPot = g_GameState.carriedPots[CARRIED_POT_NEAREST_PIN];
    g_HolePot = g_GameState.carriedPots[CARRIED_POT_HOLE];
}

s32 GetContestWinner(void) {
    return f_ContestWinner;
}

void SetContestWinner(s32 playerIndex) {
    f_ContestWinner = playerIndex;
}

void SettleLongestDrive(void) {
    s32 longest;
    s32 player;
    s32 distance;

    if (g_GameState.betRates[BET_LONGEST_DRIVE] > 0) {
        longest = -1;
        f_ContestWinner = NO_CONTEST_WINNER;
        for (player = 0; player != g_GameState.playerCount; player++) {
            if (g_Players[player].driveDistance != UNCOUNTED_DISTANCE) {
                distance = g_Players[player].driveDistance * DRIVE_COMPARISON_SCALE;
                if (longest < distance) {
                    f_ContestWinner = player;
                    longest = distance;
                }
                else if (distance == longest) {
                    f_ContestWinner = NO_CONTEST_WINNER;
                }
            }
        }

        if (f_ContestWinner == NO_CONTEST_WINNER) {
            g_LongestDrivePot += g_GameState.betRates[BET_LONGEST_DRIVE];
        }
        else {
            g_BetTallies[f_ContestWinner][BET_LONGEST_DRIVE] +=
                g_LongestDrivePot * (g_GameState.playerCount - 1);
            for (player = 0; player != g_GameState.playerCount; player++) {
                if (player != f_ContestWinner) {
                    g_BetTallies[player][BET_LONGEST_DRIVE] -= g_LongestDrivePot;
                }
            }

            g_LongestDrivePot = g_GameState.betRates[BET_LONGEST_DRIVE];
        }
    }
}

#define FARTHER_THAN_ANY_DISTANCE 0x3FFFFFFF

void SettleNearestPin(void) {
    s32 nearest;
    s32 player;
    s32 distance;

    if (g_GameState.betRates[BET_NEAREST_PIN] > 0) {
        nearest = FARTHER_THAN_ANY_DISTANCE;
        f_ContestWinner = NO_CONTEST_WINNER;
        for (player = 0; player != g_GameState.playerCount; player++) {
            distance = g_Players[player].pinDistance;
            if (distance != UNCOUNTED_DISTANCE) {
                if (distance < nearest) {
                    f_ContestWinner = player;
                    nearest = distance;
                }
                else if (distance == nearest) {
                    f_ContestWinner = NO_CONTEST_WINNER;
                }
            }
        }

        if (f_ContestWinner == NO_CONTEST_WINNER) {
            g_NearestPinPot += g_GameState.betRates[BET_NEAREST_PIN];
        }
        else {
            g_BetTallies[f_ContestWinner][BET_NEAREST_PIN] +=
                g_NearestPinPot * (g_GameState.playerCount - 1);
            for (player = 0; player != g_GameState.playerCount; player++) {
                if (player != f_ContestWinner) {
                    g_BetTallies[player][BET_NEAREST_PIN] -= g_NearestPinPot;
                }
            }

            g_NearestPinPot = g_GameState.betRates[BET_NEAREST_PIN];
        }
    }
}

void ClearContestWinner(void) {
    f_ContestWinner = NO_CONTEST_WINNER;
}

void UpdateContest(Boolean isHoleOver) {
    s32 holeIndex = g_GameState.holeIndex;
    s32 player;
    s32 measuredCount;

    if (!f_IsContestDecided) {
        if (IsMarkedParFive(g_GameState.courseIndex, holeIndex)) {
            if (isHoleOver == TRUE) {
                for (player = 0; player != g_GameState.playerCount; player++) {
                    if (g_Players[player].driveDistance == UNMEASURED_DISTANCE) {
                        g_Players[player].driveDistance = UNCOUNTED_DISTANCE;
                    }
                }
            }

            measuredCount = 0;
            for (player = 0; player != g_GameState.playerCount; player++) {
                measuredCount += g_Players[player].driveDistance != UNMEASURED_DISTANCE;
            }

            if (measuredCount == g_GameState.playerCount) {
                SettleLongestDrive();
                f_IsContestDecided = TRUE;
            }
        }
        else if (IsParThree(g_GameState.courseIndex, holeIndex)) {
            if (isHoleOver == TRUE) {
                for (player = 0; player != g_GameState.playerCount; player++) {
                    if (g_Players[player].pinDistance == UNMEASURED_DISTANCE) {
                        g_Players[player].pinDistance = UNCOUNTED_DISTANCE;
                    }
                }
            }

            measuredCount = 0;
            for (player = 0; player != g_GameState.playerCount; player++) {
                measuredCount += g_Players[player].pinDistance != UNMEASURED_DISTANCE;
            }

            if (measuredCount == g_GameState.playerCount) {
                SettleNearestPin();
                f_IsContestDecided = TRUE;
            }
        }
        else {
            f_ContestWinner = NO_CONTEST_WINNER;
        }
    }
}

void RecordCourseRound(void) {
    CourseStats *record = &GetSaveData()->records.courseStats[g_GameState.courseIndex];
    RoundStats *stats = &g_RoundStats;
    CourseRecord *course = &g_CourseRecords[g_GameState.courseIndex];
    Boolean isAtRoundLimit;
    s32 hole;
    s64 score;
    s64 total;
    s32 sum;
    s32 shotsToGreen;

    if (record->roundCount <= 0) {
        record->roundCount = 0;
        record->bestScore = NO_BEST_SCORE;
        record->strokeTotal = 0;
        record->longestDrive = 0;
        record->driveAverage = 0;
        record->averagedDriveCount = 0;
        record->measuredDriveTotal = 0;
        record->puttTotal = 0;
        record->parOrBetterCount = 0;
        record->parOnCount = 0;
        record->sandSaveCount = 0;
        record->missedSandSaveCount = 0;
    }

    isAtRoundLimit = FALSE;
    if (record->roundCount < ROUND_COUNT_LIMIT) {
        record->roundCount++;
    }
    else {
        isAtRoundLimit = TRUE;
    }

    g_Players[0].holeStatus = HOLE_STATUS_PLAYING;
    score = GetNetScoreToPar(COURSE_HOLE_COUNT, 0);
    if (score < record->bestScore) {
        record->bestScore = score;
    }

    if (record->bestScore < LOWEST_SCORE_TO_PAR) {
        record->bestScore = LOWEST_SCORE_TO_PAR;
    }

    if (score < record->characterBestScores[g_Players[0].characterIndex]) {
        record->characterBestScores[g_Players[0].characterIndex] = score;
        record->hasCharacterBestScore[g_Players[0].characterIndex] = TRUE;
        if (record->characterBestScores[g_Players[0].characterIndex] < LOWEST_SCORE_TO_PAR) {
            record->characterBestScores[g_Players[0].characterIndex] = LOWEST_SCORE_TO_PAR;
        }
    }

    total = 0;
    for (hole = 0; hole != COURSE_HOLE_COUNT; hole++) {
        total += g_Players[0].holes[hole].strokeCount;
    }

    total += record->strokeTotal;
    if (isAtRoundLimit == TRUE) {
        total = total * ROUND_COUNT_LIMIT / (ROUND_COUNT_LIMIT + 1);
    }

    if (total > RECORD_TOTAL_LIMIT) {
        total = RECORD_TOTAL_LIMIT;
    }

    record->strokeTotal = total;

    if (record->longestDrive < stats->longestDrive) {
        record->longestDrive = stats->longestDrive;
    }

    if (record->averagedDriveCount == 0) {
        if (stats->averagedDriveCount == 0) {
            record->driveAverage = 0;
        }
        else if (stats->averagedDriveCount > 0) {
            record->driveAverage = stats->averagedDriveTotal / stats->averagedDriveCount;
            record->averagedDriveCount = 1;
        }
        else {
            record->driveAverage = 0;
        }
    }
    else if (stats->averagedDriveCount > 0) {
        record->driveAverage =
            ((record->driveAverage * record->averagedDriveCount) + stats->averagedDriveTotal) /
            (record->averagedDriveCount + stats->averagedDriveCount);
        record->averagedDriveCount += stats->averagedDriveCount;
    }

    sum = record->measuredDriveTotal + stats->measuredDriveCount;
    if (sum > RECORD_TOTAL_LIMIT) {
        record->measuredDriveTotal = RECORD_TOTAL_LIMIT;
    }
    else {
        record->measuredDriveTotal = sum;
    }

    if (isAtRoundLimit == TRUE) {
        record->measuredDriveTotal =
            record->measuredDriveTotal * ROUND_COUNT_LIMIT / (ROUND_COUNT_LIMIT + 1);
    }

    total = 0;
    for (hole = 0; hole != COURSE_HOLE_COUNT; hole++) {
        if (g_Players[0].holes[hole].puttCount != UNCOUNTED_PUTTS) {
            total += g_Players[0].holes[hole].puttCount;
        }
    }

    total += record->puttTotal;
    if (isAtRoundLimit == TRUE) {
        total = total * ROUND_COUNT_LIMIT / (ROUND_COUNT_LIMIT + 1);
    }

    if (total > RECORD_TOTAL_LIMIT) {
        total = RECORD_TOTAL_LIMIT;
    }

    record->puttTotal = total;

    for (hole = 0; hole != COURSE_HOLE_COUNT; hole++) {
        if (g_Players[0].holes[hole].strokeCount <= course->holes[hole].par) {
            if (record->parOrBetterCount < RECORD_TOTAL_LIMIT) {
                record->parOrBetterCount++;
            }

            if (isAtRoundLimit == TRUE) {
                record->parOrBetterCount =
                    record->parOrBetterCount * ROUND_COUNT_LIMIT / (ROUND_COUNT_LIMIT + 1);
            }
        }
    }

    for (hole = 0; hole != COURSE_HOLE_COUNT; hole++) {
        shotsToGreen = g_Players[0].holes[hole].strokeCount - g_Players[0].holes[hole].puttCount;
        if (course->holes[hole].par - shotsToGreen >= PAR_ON_SPARE_STROKES) {
            if (record->parOnCount < RECORD_TOTAL_LIMIT) {
                record->parOnCount++;
            }

            if (isAtRoundLimit == TRUE) {
                record->parOnCount =
                    record->parOnCount * ROUND_COUNT_LIMIT / (ROUND_COUNT_LIMIT + 1);
            }
        }
    }

    sum = record->sandSaveCount + stats->sandSaveCount;
    if (sum > RECORD_TOTAL_LIMIT) {
        record->sandSaveCount = RECORD_TOTAL_LIMIT;
    }
    else {
        record->sandSaveCount = sum;
    }

    sum = record->missedSandSaveCount + stats->missedSandSaveCount;
    if (sum > RECORD_TOTAL_LIMIT) {
        record->missedSandSaveCount = RECORD_TOTAL_LIMIT;
    }
    else {
        record->missedSandSaveCount = sum;
    }

    WriteSaveData();
}

void StartRoundStats(void) {
    SaveData *save;

    save = GetSaveData();
    g_RoundStats.sandSaveCount = 0;
    g_RoundStats.missedSandSaveCount = 0;
    g_RoundStats.measuredDriveCount = 0;
    g_RoundStats.averagedDriveCount = 0;

    g_RoundStats.longestDrive = (save->records.courseStats + g_GameState.courseIndex)->longestDrive;
    g_RoundStats.averagedDriveTotal = 0;
    g_RoundStats.isDriveAveraged = FALSE;
}

void RestartRoundStats(void) {
    SaveData *save;

    save = GetSaveData();
    g_RoundStats.sandSaveCount = 0;
    g_RoundStats.missedSandSaveCount = 0;
    g_RoundStats.measuredDriveCount = 0;
    g_RoundStats.averagedDriveCount = 0;

    g_RoundStats.longestDrive = (save->records.courseStats + g_GameState.courseIndex)->longestDrive;
    g_RoundStats.isDriveAveraged = FALSE;
}

void RecordHoleStats(void) {
    RoundStats *stats;

    GetSaveData();
    stats = &g_RoundStats;
    if (g_Players[0].hasBeenInBunker) {
        if (g_Players[0].strokeCount <= g_HoleSetup.par) {
            stats->sandSaveCount++;
        }
        else {
            stats->missedSandSaveCount++;
        }
    }

    if (g_Players[0].lastDriveDistance >= 0) {
        stats->measuredDriveCount++;
        if (g_Players[0].lastDriveDistance > stats->longestDrive) {
            stats->longestDrive = g_Players[0].lastDriveDistance;
        }

        if (stats->isDriveAveraged == TRUE) {
            stats->averagedDriveCount++;
            stats->averagedDriveTotal += g_Players[0].lastDriveDistance;
        }
    }
}

s32 RecordDrive(void) {
    RoundStats *stats = &g_RoundStats;
    Club *club = GetSelectedClub();

    g_Players[0].lastDriveDistance = g_Players[0].driveDistance;
    if (club->type == CLUB_TYPE_DRIVER && g_Players[0].driveDistance > 0) {
        stats->isDriveAveraged = TRUE;
    }
    else {
        stats->isDriveAveraged = FALSE;
    }

    return g_Players[0].lastDriveDistance > stats->longestDrive;
}

void SettleHoleBets(void) {
    s32 playerIndex;
    s32 j;
    s32 lastHole = g_GameState.holeIndex - 1;
    s32 teamScore;
    s32 rivalTeamScore;
    s32 bet;
    s32 best;
    s32 count;
    s32 score;

    s8 *lasVegasRate;
    s16 *lasVegasTally;

    void SettleNassau(s32 startHole, s32 endHole) {
        s32 totals[PLAYER_COUNT_MAX];

        for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
            totals[playerIndex] = 0;
            for (j = startHole; j != endHole; j++) {
                totals[playerIndex] += g_Players[playerIndex].holes[j].strokeCount +
                    GetHandicapOffset(GetCourseRecordIndex(), j, g_Players[playerIndex].handicap);
            }
        }

        for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
            for (j = 0; j != g_GameState.playerCount; j++) {
                if (playerIndex != j) {
                    if (totals[playerIndex] < totals[j]) {
                        g_BetTallies[playerIndex][BET_NASSAU] += g_GameState.betRates[BET_NASSAU];
                    }
                    else if (totals[j] < totals[playerIndex]) {
                        g_BetTallies[playerIndex][BET_NASSAU] -= g_GameState.betRates[BET_NASSAU];
                    }
                }
            }
        }
    }

    s32 scores[PLAYER_COUNT_MAX];
    s32 unused[8] UNUSED;

    if (g_GameState.betRates[BET_STROKES] > 0) {
        for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
            for (j = 0; j != g_GameState.playerCount; j++) {
                s32 rivalIndex = j;
                if (playerIndex != rivalIndex) {
                    score = g_Players[playerIndex].strokeCount +
                        GetHandicapOffset(
                            GetCourseRecordIndex(),
                            lastHole,
                            g_Players[playerIndex].handicap
                        );
                    g_BetTallies[playerIndex][BET_STROKES] += (g_Players[j].strokeCount +
                                                                  GetHandicapOffset(
                                                                      GetCourseRecordIndex(),
                                                                      lastHole,
                                                                      g_Players[j].handicap
                                                                  ) -
                                                                  score) *
                        g_GameState.betRates[BET_STROKES];
                }
            }
        }
    }

    lasVegasRate = &g_GameState.betRates[BET_LAS_VEGAS];
    if (*lasVegasRate > 0) {
        lasVegasTally = &g_BetTallies[0][BET_LAS_VEGAS];
        teamScore = g_Players[0].strokeCount + g_Players[1].strokeCount +
            GetHandicapOffset(GetCourseRecordIndex(), lastHole, g_Players[0].handicap) +
            GetHandicapOffset(GetCourseRecordIndex(), lastHole, g_Players[1].handicap);
        rivalTeamScore = g_Players[2].strokeCount + g_Players[3].strokeCount +
            GetHandicapOffset(GetCourseRecordIndex(), lastHole, g_Players[2].handicap) +
            GetHandicapOffset(GetCourseRecordIndex(), lastHole, g_Players[3].handicap);
        *lasVegasTally += (rivalTeamScore - teamScore) * *lasVegasRate;
        g_BetTallies[1][BET_LAS_VEGAS] += (rivalTeamScore - teamScore) * *lasVegasRate;
        g_BetTallies[2][BET_LAS_VEGAS] += (teamScore - rivalTeamScore) * *lasVegasRate;
        g_BetTallies[3][BET_LAS_VEGAS] += (teamScore - rivalTeamScore) * *lasVegasRate;
    }

    if (g_GameState.betRates[BET_NASSAU] > 0) {
        if (g_GameState.holeIndex == FRONT_NINE_END &&
            !(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
            SettleNassau(0, FRONT_NINE_END);
        }

        if (g_GameState.holeIndex == BACK_NINE_END) {
            if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
                SettleNassau(FRONT_NINE_END, BACK_NINE_END);
            }

            if (!(g_GameState.holeSelection &
                    (HOLE_SELECTION_BACK_NINE | HOLE_SELECTION_FRONT_NINE))) {
                SettleNassau(0, BACK_NINE_END);
            }
        }
    }

    if (g_GameState.betRates[BET_SKINS] != UNSET_BET_RATE ||
        g_GameState.betRates[BET_MATCH_POT] != UNSET_BET_RATE) {
        bet = BET_MATCH_POT;
        if (g_GameState.betRates[BET_SKINS] != UNSET_BET_RATE) {
            bet = BET_SKINS;
        }
        else {
            count = 0;
        }

        switch (g_GameState.gameMode) {
            case GAME_MODE_SKINS_MATCH:
            case GAME_MODE_CLUB_SLOTS:
                count = 0;
                for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                    count +=
                        g_Players[playerIndex].holes[g_GameState.holeIndex - 1].strokeCount == 0;
                }

                if (count != g_GameState.playerCount) {
                    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                        if (g_Players[playerIndex].holes[g_GameState.holeIndex - 1].strokeCount !=
                            0) {
                            g_BetTallies[playerIndex][bet] +=
                                g_HolePot * (g_GameState.playerCount - 1);
                        }
                        else {
                            g_BetTallies[playerIndex][bet] -= g_HolePot;
                        }
                    }

                    g_HolePot = g_GameState.betRates[bet];
                }
                else {
                    g_HolePot += g_GameState.betRates[bet];
                }

                break;
            case GAME_MODE_MATCH_GAME:
                if (g_Players[0].holes[g_GameState.holeIndex - 1].strokeCount <
                    g_Players[1].holes[g_GameState.holeIndex - 1].strokeCount) {
                    g_BetTallies[0][bet] += g_HolePot;
                    g_BetTallies[1][bet] -= g_HolePot;
                    g_HolePot = g_GameState.betRates[bet];
                }
                else if (g_Players[1].holes[g_GameState.holeIndex - 1].strokeCount <
                    g_Players[0].holes[g_GameState.holeIndex - 1].strokeCount) {
                    g_BetTallies[0][bet] -= g_HolePot;
                    g_BetTallies[1][bet] += g_HolePot;
                    g_HolePot = g_GameState.betRates[bet];
                }
                else {
                    g_HolePot += g_GameState.betRates[bet];
                }

                break;
            case GAME_MODE_STROKE:
            case GAME_MODE_UNLISTED_STROKE:
            case GAME_MODE_UNRECORDED_STROKE: {
                for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                    scores[playerIndex] = g_Players[playerIndex].strokeCount +
                        GetHandicapOffset(
                            g_GameState.courseIndex,
                            g_GameState.holeIndex - 1,
                            g_Players[playerIndex].handicap
                        );
                }

                best = WORST_HOLE_SCORE;
                for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                    if (scores[playerIndex] < best) {
                        best = scores[playerIndex];
                    }
                }

                count = 0;
                for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                    count += best == scores[playerIndex];
                }

                if (count == 1) {
                    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                        if (best == scores[playerIndex]) {
                            g_BetTallies[playerIndex][bet] +=
                                g_HolePot * (g_GameState.playerCount - 1);
                        }
                        else {
                            g_BetTallies[playerIndex][bet] -= g_HolePot;
                        }
                    }

                    g_HolePot = g_GameState.betRates[bet];
                }
                else {
                    g_HolePot += g_GameState.betRates[bet];
                }

                break;
            }
        }
    }

    g_GameState.carriedPots[CARRIED_POT_LONGEST_DRIVE] = g_LongestDrivePot;
    g_GameState.carriedPots[CARRIED_POT_NEAREST_PIN] = g_NearestPinPot;
    g_GameState.carriedPots[CARRIED_POT_HOLE] = g_HolePot;
    for (playerIndex = 0; playerIndex != PLAYER_COUNT_MAX; playerIndex++) {
        for (j = 0; j != BET_COUNT; j++) {
            g_Players[playerIndex].betTallies[j] = g_BetTallies[playerIndex][j];
        }
    }

    f_IsContestDecided = FALSE;
}
