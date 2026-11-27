/*
 * Records the player's progress in the save: wins and losses against the
 * characters, birdie badges and tournament places.
 */

#include "save/awards.h"

#include <ultra64.h>

#include "course/course.h"
#include "game/scene.h"
#include "round/game_state.h"
#include "round/round_stats.h"
#include "round/scoring.h"
#include "round/tournament.h"
#include "save/course_stats.h"
#include "save/save.h"
#include "shot/particle_effects.h"
#include "system/c_language.h"

// Counts in the save stop here
#define MATCHUP_RESULT_COUNT_MAX 99

// Any place past third, which no flag records
#define PLACE_UNPLACED 4

// Counts a win or a loss in matchups[row][column], and saves. A win also sets
// WON_UNSAVED_FLAG unless g_GameState.saveCount is set, and prints "Win Com
// Chara".
void RecordComputerMatchupResult(s32 rowCharacter, s32 columnCharacter, Boolean isWin) {
    Awards *awards;
    u8 winCount;
    s32 winBits;
    s32 tally;

    awards = &GetSaveData()->records.awards;
    if (isWin) {
        winBits = awards->matchups[rowCharacter][columnCharacter].winBits;
        winCount = winBits & WIN_COUNT_MASK;
        if (winCount < MATCHUP_RESULT_COUNT_MAX) {
            winCount++;
        }

        tally = winCount | (winBits & WON_UNSAVED_FLAG);
        winBits = tally;
        if (g_GameState.saveCount == 0) {
            winBits |= WON_UNSAVED_FLAG;
        }

        awards->matchups[rowCharacter][columnCharacter].winBits = winBits;
        osSyncPrintf("Win Com Chara[%d]\n", winBits);
    }
    else {
        tally = awards->matchups[rowCharacter][columnCharacter].lossCount + 1;
        awards->matchups[rowCharacter][columnCharacter].lossCount = tally;
        if (awards->matchups[rowCharacter][columnCharacter].lossCount > MATCHUP_RESULT_COUNT_MAX) {
            awards->matchups[rowCharacter][columnCharacter].lossCount = MATCHUP_RESULT_COUNT_MAX;
        }
    }

    TransferSaveData(GetSaveData(), TRUE);
}

// Adds a birdie badge on the current hole of a tournament, and saves
void AddBirdieBadge(void) {
    Awards *awards;

    awards = &GetSaveData()->records.awards;
    if (g_GameState.gameMode == GAME_MODE_TOURNAMENT) {
        awards->birdieBadgeCounts[g_GameState.courseIndex][g_GameState.holeIndex]++;
        if (awards->birdieBadgeCounts[g_GameState.courseIndex][g_GameState.holeIndex] >
            MATCHUP_RESULT_COUNT_MAX) {
            awards->birdieBadgeCounts[g_GameState.courseIndex][g_GameState.holeIndex] =
                MATCHUP_RESULT_COUNT_MAX;
        }

        TransferSaveData(GetSaveData(), TRUE);
    }
}

// Passes how many holes have a birdie badge to ShowBirdieBadgeCount, if this is
// a tournament and the current hole has none yet
void ReportBirdieBadgeCount(void) {
    Awards *awards;
    s32 badgeCount;
    CourseIndex courseIndex;
    s32 holeIndex;

    awards = &GetSaveData()->records.awards;
    if (g_GameState.gameMode == GAME_MODE_TOURNAMENT &&
        awards->birdieBadgeCounts[g_GameState.courseIndex][g_GameState.holeIndex] == 0) {
        badgeCount = 0;
        for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
            for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
                badgeCount += awards->birdieBadgeCounts[courseIndex][holeIndex] != FALSE;
            }
        }

        ShowBirdieBadgeCount(badgeCount);
    }
}

// Flags the course's statistics with the player's place in the tournament, if
// third or better, and saves
void RecordTournamentPlace(void) {
    CourseStats *courseStats;
    Boolean *earnedPlace;
    Boolean hasPlace;
    s32 place;
    s32 index;
    TournamentStanding *standing;

    courseStats = &GetSaveData()->records.courseStats[g_GameState.courseIndex];
    hasPlace = FALSE;
    if (g_GameState.gameMode == GAME_MODE_TOURNAMENT) {
        RankTournament(
            GetLastHoleIndex() + 1,
            0,
            GetNetScoreToPar(GetLastHoleIndex() + 1, 0),
            g_GameState.roundSeed
        );
        place = PLACE_UNPLACED;
        for (index = 0; index != TOURNAMENT_CONTESTANT_COUNT; index++) {
            standing = &g_TournamentStandings[index];
            if (standing->contestantId == PLAYER_CONTESTANT) {
                place = standing->place & PLACE_MASK;
                break;
            }
        }

        switch (place) {
            case 1:
                earnedPlace = &courseStats->hasPlace[FIRST_PLACE_INDEX];
                hasPlace = TRUE;
                break;
            case 2:
                earnedPlace = &courseStats->hasPlace[SECOND_PLACE_INDEX];
                hasPlace = TRUE;
                break;
            case 3:
                earnedPlace = &courseStats->hasPlace[THIRD_PLACE_INDEX];
                hasPlace = TRUE;
                break;
            default:
                // Not written, since hasPlace stays FALSE
                earnedPlace = &courseStats->hasPlace[THIRD_PLACE_INDEX];
                break;
        }

        if (hasPlace) {
            *earnedPlace = TRUE;
        }

        TransferSaveData(GetSaveData(), TRUE);
    }
}
