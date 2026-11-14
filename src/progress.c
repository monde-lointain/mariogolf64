#include "progress.h"

#include <ultra64.h>

#include "course.h"
#include "course_stats.h"
#include "effects.h"
#include "game_state.h"
#include "language.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "tournament.h"

#define MATCHUP_RESULT_COUNT_MAX 99

#define PLACE_UNPLACED 4

void RecordComCharacterResult(s32 row, s32 column, Boolean isWin) {
    Awards *awards;
    u8 winCount;
    s32 winBits;
    // The new win bits or loss count. Giving each its own variable
    // breaks the match.
    s32 tally;

    awards = &GetSaveData()->records.awards;
    if (isWin) {
        winBits = awards->matchups[row][column].winBits;
        winCount = winBits & WIN_COUNT_MASK;
        if (winCount < MATCHUP_RESULT_COUNT_MAX) {
            winCount++;
        }

        tally = winCount | (winBits & WON_UNSAVED_FLAG);
        winBits = tally;
        if (g_GameState.saveCount == 0) {
            winBits |= WON_UNSAVED_FLAG;
        }

        awards->matchups[row][column].winBits = winBits;
        osSyncPrintf("Win Com Chara[%d]\n", winBits);
    }
    else {
        tally = awards->matchups[row][column].lossCount + 1;
        awards->matchups[row][column].lossCount = tally;
        if (awards->matchups[row][column].lossCount > MATCHUP_RESULT_COUNT_MAX) {
            awards->matchups[row][column].lossCount = MATCHUP_RESULT_COUNT_MAX;
        }
    }

    TransferSaveData(GetSaveData(), TRUE);
}

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

void RecordTournamentPlace(void) {
    CourseStats *stats;
    s32 *placeEntry;
    Boolean hasPlace;
    s32 place;
    s32 index;
    TournamentStanding *standing;

    stats = &GetSaveData()->records.courseStats[g_GameState.courseIndex];
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
                placeEntry = &stats->hasPlace[FIRST_PLACE_INDEX];
                hasPlace = TRUE;
                break;
            case 2:
                placeEntry = &stats->hasPlace[SECOND_PLACE_INDEX];
                hasPlace = TRUE;
                break;
            case 3:
                placeEntry = &stats->hasPlace[THIRD_PLACE_INDEX];
                hasPlace = TRUE;
                break;
            default:
                placeEntry = &stats->hasPlace[THIRD_PLACE_INDEX];
                break;
        }

        if (hasPlace) {
            *placeEntry = TRUE;
        }

        TransferSaveData(GetSaveData(), TRUE);
    }
}
