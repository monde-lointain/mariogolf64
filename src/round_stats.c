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
ScorecardState g_ScorecardState = SCORECARD_CLOSED;

static s32 f_ContestWinner = NO_CONTEST_WINNER;
static void *f_UnusedPositionRoutines[6] UNUSED = {NULL, NULL, guPosition, NULL, NULL, NULL};

s32 GetNetScoreToPar(s32 endHole, s32 playerIndex) {
    s32 score = 0;
    CourseRecord *course = &g_CourseRecords[GetCourseRecordIndex()];
    s32 holeIndex;

    if (g_GameState.holeIndex == endHole) {
        endHole += g_Players[playerIndex].holeStatus != HOLE_STATUS_PLAYING;
    }

    for (holeIndex = GetFirstHoleIndex(); holeIndex < endHole; holeIndex++) {
        score += g_Players[playerIndex].holes[holeIndex].strokeCount -
            course->holes[holeIndex].par +
            GetHandicapOffset(g_GameState.courseIndex, holeIndex, g_Players[playerIndex].handicap);
    }

    return score;
}

void TallySkins(s32 endHole) {
    s32 playerIndex;
    s32 hole;
    s32 winner;
    s32 skins;

    for (hole = 0; hole != g_GameState.playerCount; hole++) {
        g_RoundHud.skinsTotals[hole] = 0;
    }

    winner = FALSE;
    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        if (g_Players[playerIndex].holeStatus == HOLE_STATUS_PLAYING) {
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
        for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
            if (g_Players[playerIndex].holes[hole].strokeCount != 0) {
                winner = playerIndex;
                g_Players[playerIndex].holes[hole].strokeCount = skins;
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
    s32 playerIndex;
    s32 bet;

    for (playerIndex = 0; playerIndex != PLAYER_COUNT_MAX; playerIndex++) {
        for (bet = 0; bet != BET_COUNT; bet++) {
            g_BetTallies[playerIndex][bet] = 0;
        }
    }

    for (playerIndex = 0; playerIndex != PLAYER_COUNT_MAX; playerIndex++) {
        for (bet = 0; bet != BET_COUNT; bet++) {
            g_Players[playerIndex].betTallies[bet] = g_BetTallies[playerIndex][bet];
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
    s32 playerIndex;
    s32 bet;

    for (playerIndex = 0; playerIndex != PLAYER_COUNT_MAX; playerIndex++) {
        for (bet = 0; bet != BET_COUNT; bet++) {
            g_BetTallies[playerIndex][bet] = g_Players[playerIndex].betTallies[bet];
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
    s32 playerIndex;
    s32 distance;

    if (g_GameState.betRates[BET_LONGEST_DRIVE] > 0) {
        longest = -1;
        f_ContestWinner = NO_CONTEST_WINNER;
        for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
            if (g_Players[playerIndex].driveDistance != UNCOUNTED_DISTANCE) {
                distance = g_Players[playerIndex].driveDistance * DRIVE_COMPARISON_SCALE;
                if (longest < distance) {
                    f_ContestWinner = playerIndex;
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
            for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                if (playerIndex != f_ContestWinner) {
                    g_BetTallies[playerIndex][BET_LONGEST_DRIVE] -= g_LongestDrivePot;
                }
            }

            g_LongestDrivePot = g_GameState.betRates[BET_LONGEST_DRIVE];
        }
    }
}

#define FARTHER_THAN_ANY_DISTANCE 0x3FFFFFFF

void SettleNearestPin(void) {
    s32 nearest;
    s32 playerIndex;
    s32 distance;

    if (g_GameState.betRates[BET_NEAREST_PIN] > 0) {
        nearest = FARTHER_THAN_ANY_DISTANCE;
        f_ContestWinner = NO_CONTEST_WINNER;
        for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
            distance = g_Players[playerIndex].pinDistance;
            if (distance != UNCOUNTED_DISTANCE) {
                if (distance < nearest) {
                    f_ContestWinner = playerIndex;
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
            for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                if (playerIndex != f_ContestWinner) {
                    g_BetTallies[playerIndex][BET_NEAREST_PIN] -= g_NearestPinPot;
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
    s32 playerIndex;
    s32 measuredCount;

    if (!f_IsContestDecided) {
        if (IsMarkedParFive(g_GameState.courseIndex, holeIndex)) {
            if (isHoleOver == TRUE) {
                for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                    if (g_Players[playerIndex].driveDistance == UNMEASURED_DISTANCE) {
                        g_Players[playerIndex].driveDistance = UNCOUNTED_DISTANCE;
                    }
                }
            }

            measuredCount = 0;
            for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                measuredCount += g_Players[playerIndex].driveDistance != UNMEASURED_DISTANCE;
            }

            if (measuredCount == g_GameState.playerCount) {
                SettleLongestDrive();
                f_IsContestDecided = TRUE;
            }
        }
        else if (IsParThree(g_GameState.courseIndex, holeIndex)) {
            if (isHoleOver == TRUE) {
                for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                    if (g_Players[playerIndex].pinDistance == UNMEASURED_DISTANCE) {
                        g_Players[playerIndex].pinDistance = UNCOUNTED_DISTANCE;
                    }
                }
            }

            measuredCount = 0;
            for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                measuredCount += g_Players[playerIndex].pinDistance != UNMEASURED_DISTANCE;
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
    CourseStats *courseStats = &GetSaveData()->records.courseStats[g_GameState.courseIndex];
    RoundStats *stats = &g_RoundStats;
    CourseRecord *course = &g_CourseRecords[g_GameState.courseIndex];
    Boolean isAtRoundLimit;
    s32 holeIndex;
    s64 score;
    s64 total;
    s32 measuredDriveTotal;
    s32 shotsToGreen;

    if (courseStats->roundCount <= 0) {
        courseStats->roundCount = 0;
        courseStats->bestScore = NO_BEST_SCORE;
        courseStats->strokeTotal = 0;
        courseStats->longestDrive = 0;
        courseStats->driveAverage = 0;
        courseStats->averagedDriveCount = 0;
        courseStats->measuredDriveTotal = 0;
        courseStats->puttTotal = 0;
        courseStats->parOrBetterCount = 0;
        courseStats->parOnCount = 0;
        courseStats->sandSaveCount = 0;
        courseStats->missedSandSaveCount = 0;
    }

    isAtRoundLimit = FALSE;
    if (courseStats->roundCount < ROUND_COUNT_LIMIT) {
        courseStats->roundCount++;
    }
    else {
        isAtRoundLimit = TRUE;
    }

    g_Players[0].holeStatus = HOLE_STATUS_PLAYING;
    score = GetNetScoreToPar(COURSE_HOLE_COUNT, 0);
    if (score < courseStats->bestScore) {
        courseStats->bestScore = score;
    }

    if (courseStats->bestScore < LOWEST_SCORE_TO_PAR) {
        courseStats->bestScore = LOWEST_SCORE_TO_PAR;
    }

    if (score < courseStats->characterBestScores[g_Players[0].characterIndex]) {
        courseStats->characterBestScores[g_Players[0].characterIndex] = score;
        courseStats->hasCharacterBestScore[g_Players[0].characterIndex] = TRUE;
        if (courseStats->characterBestScores[g_Players[0].characterIndex] < LOWEST_SCORE_TO_PAR) {
            courseStats->characterBestScores[g_Players[0].characterIndex] = LOWEST_SCORE_TO_PAR;
        }
    }

    total = 0;
    for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
        total += g_Players[0].holes[holeIndex].strokeCount;
    }

    total += courseStats->strokeTotal;
    if (isAtRoundLimit == TRUE) {
        total = total * ROUND_COUNT_LIMIT / (ROUND_COUNT_LIMIT + 1);
    }

    if (total > RECORD_TOTAL_LIMIT) {
        total = RECORD_TOTAL_LIMIT;
    }

    courseStats->strokeTotal = total;

    if (courseStats->longestDrive < stats->longestDrive) {
        courseStats->longestDrive = stats->longestDrive;
    }

    if (courseStats->averagedDriveCount == 0) {
        if (stats->averagedDriveCount == 0) {
            courseStats->driveAverage = 0;
        }
        else if (stats->averagedDriveCount > 0) {
            courseStats->driveAverage = stats->averagedDriveTotal / stats->averagedDriveCount;
            courseStats->averagedDriveCount = 1;
        }
        else {
            courseStats->driveAverage = 0;
        }
    }
    else if (stats->averagedDriveCount > 0) {
        courseStats->driveAverage = ((courseStats->driveAverage * courseStats->averagedDriveCount) +
                                        stats->averagedDriveTotal) /
            (courseStats->averagedDriveCount + stats->averagedDriveCount);
        courseStats->averagedDriveCount += stats->averagedDriveCount;
    }

    measuredDriveTotal = courseStats->measuredDriveTotal + stats->measuredDriveCount;
    if (measuredDriveTotal > RECORD_TOTAL_LIMIT) {
        courseStats->measuredDriveTotal = RECORD_TOTAL_LIMIT;
    }
    else {
        courseStats->measuredDriveTotal = measuredDriveTotal;
    }

    if (isAtRoundLimit == TRUE) {
        courseStats->measuredDriveTotal =
            courseStats->measuredDriveTotal * ROUND_COUNT_LIMIT / (ROUND_COUNT_LIMIT + 1);
    }

    total = 0;
    for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
        if (g_Players[0].holes[holeIndex].puttCount != UNCOUNTED_PUTTS) {
            total += g_Players[0].holes[holeIndex].puttCount;
        }
    }

    total += courseStats->puttTotal;
    if (isAtRoundLimit == TRUE) {
        total = total * ROUND_COUNT_LIMIT / (ROUND_COUNT_LIMIT + 1);
    }

    if (total > RECORD_TOTAL_LIMIT) {
        total = RECORD_TOTAL_LIMIT;
    }

    courseStats->puttTotal = total;

    for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
        if (g_Players[0].holes[holeIndex].strokeCount <= course->holes[holeIndex].par) {
            if (courseStats->parOrBetterCount < RECORD_TOTAL_LIMIT) {
                courseStats->parOrBetterCount++;
            }

            if (isAtRoundLimit == TRUE) {
                courseStats->parOrBetterCount =
                    courseStats->parOrBetterCount * ROUND_COUNT_LIMIT / (ROUND_COUNT_LIMIT + 1);
            }
        }
    }

    for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
        shotsToGreen =
            g_Players[0].holes[holeIndex].strokeCount - g_Players[0].holes[holeIndex].puttCount;
        if (course->holes[holeIndex].par - shotsToGreen >= PAR_ON_SPARE_STROKES) {
            if (courseStats->parOnCount < RECORD_TOTAL_LIMIT) {
                courseStats->parOnCount++;
            }

            if (isAtRoundLimit == TRUE) {
                courseStats->parOnCount =
                    courseStats->parOnCount * ROUND_COUNT_LIMIT / (ROUND_COUNT_LIMIT + 1);
            }
        }
    }

    measuredDriveTotal = courseStats->sandSaveCount + stats->sandSaveCount;
    if (measuredDriveTotal > RECORD_TOTAL_LIMIT) {
        courseStats->sandSaveCount = RECORD_TOTAL_LIMIT;
    }
    else {
        courseStats->sandSaveCount = measuredDriveTotal;
    }

    measuredDriveTotal = courseStats->missedSandSaveCount + stats->missedSandSaveCount;
    if (measuredDriveTotal > RECORD_TOTAL_LIMIT) {
        courseStats->missedSandSaveCount = RECORD_TOTAL_LIMIT;
    }
    else {
        courseStats->missedSandSaveCount = measuredDriveTotal;
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
    s32 bestScore;
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

                bestScore = WORST_HOLE_SCORE;
                for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                    if (scores[playerIndex] < bestScore) {
                        bestScore = scores[playerIndex];
                    }
                }

                count = 0;
                for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                    count += bestScore == scores[playerIndex];
                }

                if (count == 1) {
                    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                        if (bestScore == scores[playerIndex]) {
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
