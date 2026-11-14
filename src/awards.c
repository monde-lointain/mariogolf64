#include "awards.h"

#include <ultra64.h>

#include "course/course.h"
#include "course_stats.h"
#include "game_state.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "shot/particle_effects.h"
#include "system/c_language.h"
#include "tournament.h"

#define MATCHUP_RESULT_COUNT_MAX 99

#define PLACE_UNPLACED 4

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
                earnedPlace = &courseStats->hasPlace[THIRD_PLACE_INDEX];
                break;
        }

        if (hasPlace) {
            *earnedPlace = TRUE;
        }

        TransferSaveData(GetSaveData(), TRUE);
    }
}
