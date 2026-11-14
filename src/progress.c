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

#define SAVED_COUNT_MAX 99

#define PLACE_UNPLACED 4

void RecordComCharacterResult(s32 row, s32 column, Boolean isWin) {
    Awards *awards;
    u8 count;
    s32 value;
    s32 tally;

    awards = &GetSaveData()->records.awards;
    if (isWin) {
        value = awards->matchups[row][column].winBits;
        count = value & WIN_COUNT_MASK;
        if (count < SAVED_COUNT_MAX) {
            count++;
        }

        tally = count | (value & WON_UNSAVED_FLAG);
        value = tally;
        if (g_GameState.saveCount == 0) {
            value |= WON_UNSAVED_FLAG;
        }

        awards->matchups[row][column].winBits = value;
        osSyncPrintf("Win Com Chara[%d]\n", value);
    }
    else {
        tally = awards->matchups[row][column].lossCount + 1;
        awards->matchups[row][column].lossCount = tally;
        if (awards->matchups[row][column].lossCount > SAVED_COUNT_MAX) {
            awards->matchups[row][column].lossCount = SAVED_COUNT_MAX;
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
            SAVED_COUNT_MAX) {
            awards->birdieBadgeCounts[g_GameState.courseIndex][g_GameState.holeIndex] =
                SAVED_COUNT_MAX;
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
