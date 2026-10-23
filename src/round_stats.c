#include "common.h"
#include "club.h"
#include "course.h"
#include "round_stats.h"
#include "save.h"

#define NO_CONTEST_WINNER -1
#define UNSET_BET_RATE -1

/* Drives are compared at this scale, so those nearly as long tie */
#define DRIVE_COMPARISON_SCALE 7.119878e-05f

/* The pots of the contests and of the hole, which a tie carries over */
extern s32 sLongestDrivePot;
extern s32 sNearestPinPot;
extern s32 sHolePot;
extern s32 sIsContestDecided;

/*
 * The player who won the hole's contest: the longest drive on a marked par
 * 5, or the shot nearest the pin on a par 3
 */
extern s32 sContestWinner;

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006CD50);

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006CE88);

void StartBets(void) {
    s32 player;
    s32 bet;

    for (player = 0; player != MAX_PLAYER_COUNT; player++) {
        for (bet = 0; bet != BET_COUNT; bet++) {
            g_BetTallies[player][bet] = 0;
        }
    }
    for (player = 0; player != MAX_PLAYER_COUNT; player++) {
        for (bet = 0; bet != BET_COUNT; bet++) {
            g_Players[player].betTallies[bet] = g_BetTallies[player][bet];
        }
    }
    sIsContestDecided = FALSE;
    sContestWinner = NO_CONTEST_WINNER;
    sLongestDrivePot = g_GameState.betRates[BET_LONGEST_DRIVE];
    sNearestPinPot = g_GameState.betRates[BET_NEAREST_PIN];
    if (g_GameState.betRates[BET_UNK5] != UNSET_BET_RATE) {
        sHolePot = g_GameState.betRates[BET_UNK5];
    }
    if (g_GameState.betRates[BET_UNK4] != UNSET_BET_RATE) {
        sHolePot = g_GameState.betRates[BET_UNK4];
    }
    g_GameState.carriedPots[CARRIED_POT_LONGEST_DRIVE] = sLongestDrivePot;
    g_GameState.carriedPots[CARRIED_POT_NEAREST_PIN] = sNearestPinPot;
    g_GameState.carriedPots[CARRIED_POT_HOLE] = sHolePot;
}

/* Takes the tallies and pots back from a continued game */
void ResumeBets(void) {
    s32 player;
    s32 bet;

    for (player = 0; player != MAX_PLAYER_COUNT; player++) {
        for (bet = 0; bet != BET_COUNT; bet++) {
            g_BetTallies[player][bet] = g_Players[player].betTallies[bet];
        }
    }
    sIsContestDecided = FALSE;
    sContestWinner = NO_CONTEST_WINNER;
    sLongestDrivePot = g_GameState.carriedPots[CARRIED_POT_LONGEST_DRIVE];
    sNearestPinPot = g_GameState.carriedPots[CARRIED_POT_NEAREST_PIN];
    sHolePot = g_GameState.carriedPots[CARRIED_POT_HOLE];
}

s32 GetContestWinner(void) {
    return sContestWinner;
}

void SetContestWinner(s32 playerIndex) {
    sContestWinner = playerIndex;
}

/* The longest drive wins the pot from each other player */
void SettleLongestDrive(void) {
    s32 longest;
    s32 player;
    s32 distance;

    if (g_GameState.betRates[BET_LONGEST_DRIVE] > 0) {
        longest = -1;
        sContestWinner = NO_CONTEST_WINNER;
        for (player = 0; player != g_GameState.playerCount; player++) {
            if (g_Players[player].driveDistance != UNCOUNTED_DISTANCE) {
                distance = g_Players[player].driveDistance * DRIVE_COMPARISON_SCALE;
                if (longest < distance) {
                    sContestWinner = player;
                    longest = distance;
                } else if (distance == longest) {
                    sContestWinner = NO_CONTEST_WINNER;
                }
            }
        }
        if (sContestWinner == NO_CONTEST_WINNER) {
            sLongestDrivePot += g_GameState.betRates[BET_LONGEST_DRIVE];
        } else {
            g_BetTallies[sContestWinner][BET_LONGEST_DRIVE] += sLongestDrivePot * (g_GameState.playerCount - 1);
            for (player = 0; player != g_GameState.playerCount; player++) {
                if (player != sContestWinner) {
                    g_BetTallies[player][BET_LONGEST_DRIVE] -= sLongestDrivePot;
                }
            }
            sLongestDrivePot = g_GameState.betRates[BET_LONGEST_DRIVE];
        }
    }
}

/* The shot nearest the pin wins the pot from each other player */
void SettleNearestPin(void) {
    s32 nearest;
    s32 player;
    s32 distance;

    if (g_GameState.betRates[BET_NEAREST_PIN] > 0) {
        nearest = 0x3FFFFFFF;
        sContestWinner = NO_CONTEST_WINNER;
        for (player = 0; player != g_GameState.playerCount; player++) {
            distance = g_Players[player].pinDistance;
            if (distance != UNCOUNTED_DISTANCE) {
                if (distance < nearest) {
                    sContestWinner = player;
                    nearest = distance;
                } else if (distance == nearest) {
                    sContestWinner = NO_CONTEST_WINNER;
                }
            }
        }
        if (sContestWinner == NO_CONTEST_WINNER) {
            sNearestPinPot += g_GameState.betRates[BET_NEAREST_PIN];
        } else {
            g_BetTallies[sContestWinner][BET_NEAREST_PIN] += sNearestPinPot * (g_GameState.playerCount - 1);
            for (player = 0; player != g_GameState.playerCount; player++) {
                if (player != sContestWinner) {
                    g_BetTallies[player][BET_NEAREST_PIN] -= sNearestPinPot;
                }
            }
            sNearestPinPot = g_GameState.betRates[BET_NEAREST_PIN];
        }
    }
}

void ClearContestWinner(void) {
    sContestWinner = NO_CONTEST_WINNER;
}

/*
 * Settles the hole's contest once every player's distance is measured.
 * When the hole is over, the distances still unmeasured stop counting
 */
void UpdateContest(s32 isHoleOver) {
    s32 holeIndex = g_GameState.holeIndex;
    s32 player;
    s32 measuredCount;

    if (!sIsContestDecided) {
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
                sIsContestDecided = TRUE;
            }
        } else if (IsParThree(g_GameState.courseIndex, holeIndex)) {
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
                sIsContestDecided = TRUE;
            }
        } else {
            sContestWinner = NO_CONTEST_WINNER;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006D6D0);

void StartRoundStats(void) {
    SaveData *save;

    save = GetSaveData();
    g_RoundStats.unk0 = 0;
    g_RoundStats.unk4 = 0;
    g_RoundStats.measuredDriveCount = 0;
    g_RoundStats.averagedDriveCount = 0;
    /* The pointer arithmetic is there to match */
    g_RoundStats.longestDrive = (save->unkE98 + g_CourseIndex)->longestDrive;
    g_RoundStats.averagedDriveTotal = 0;
    g_RoundStats.isDriveAveraged = FALSE;
}

/* As StartRoundStats, but keeps the total of the averaged drives */
void RestartRoundStats(void) {
    SaveData *save;

    save = GetSaveData();
    g_RoundStats.unk0 = 0;
    g_RoundStats.unk4 = 0;
    g_RoundStats.measuredDriveCount = 0;
    g_RoundStats.averagedDriveCount = 0;
    /* The pointer arithmetic is there to match */
    g_RoundStats.longestDrive = (save->unkE98 + g_CourseIndex)->longestDrive;
    g_RoundStats.isDriveAveraged = FALSE;
}

/* Only the first player's holes count */
void RecordHoleStats(void) {
    RoundStats *stats;

    GetSaveData();
    stats = &g_RoundStats;
    if (g_Players[0].unk9C) {
        if (g_Players[0].unk23 <= g_HolePar) {
            stats->unk0++;
        } else {
            g_RoundStats.unk4++;
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

/* Returns whether the drive is the longest yet */
s32 RecordDrive(void) {
    RoundStats *stats = &g_RoundStats;
    Club *club = GetSelectedClub();

    g_Players[0].lastDriveDistance = g_Players[0].driveDistance;
    if (club->type == CLUB_TYPE_DRIVER && g_Players[0].driveDistance > 0) {
        g_RoundStats.isDriveAveraged = TRUE;
    } else {
        stats->isDriveAveraged = FALSE;
    }
    return g_Players[0].lastDriveDistance > stats->longestDrive;
}

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006DFF0);

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006E210);
