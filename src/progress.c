#include "common.h"
#include "course.h"
#include "save.h"
#include "scoring.h"
#include "scene.h"
#include "tournament.h"

/* Counts in the save stop here */
#define MAX_SAVED_COUNT 99

/* Any place past third, which no flag records */
#define PLACE_UNPLACED 4

extern s8 D_801B60C5;
extern s32 D_801B60B4;

s32 func_8006CD50(s32, s32);
void func_8007FE44(s32 badgeCount);

/*
 * Counts a win or a loss in unkAF0[row][column], and saves. A win also sets
 * the top bit unless D_801B60C5 is set, and prints "Win Com Chara".
 */
#ifdef NON_MATCHING
void RecordComCharacterResult(s32 row, s32 column, s32 isWin) {
    Unk990 *unk990;
    u8 count;
    u8 value;
    s32 newValue;

    unk990 = &GetSaveData()->unk990;
    if (isWin) {
        value = unk990->unkAF0[row][column].unk1;
        count = value & WIN_COUNT_MASK;
        if (count < MAX_SAVED_COUNT) {
            count = (value & WIN_COUNT_MASK) + 1;
        }
        value = count | (value & UNKAF0_FLAG);
        if (!D_801B60C5) {
            value |= UNKAF0_FLAG;
        }
        newValue = value; /* Passed on unmasked, to match */
        unk990->unkAF0[row][column].unk1 = newValue;
        osSyncPrintf("Win Com Chara[%d]\n", newValue);
    } else {
        unk990->unkAF0[row][column].unk0++;
        if (unk990->unkAF0[row][column].unk0 > MAX_SAVED_COUNT) {
            unk990->unkAF0[row][column].unk0 = MAX_SAVED_COUNT;
        }
    }
    TransferSaveData(GetSaveData(), TRUE);
}
#else
INCLUDE_ASM("asm/nonmatchings/progress", RecordComCharacterResult);
#endif

/* Adds a birdie badge on the current hole of a tournament, and saves */
void AddBirdieBadge(void) {
    Unk990 *unk990;

    unk990 = &GetSaveData()->unk990;
    if (g_GameMode == GAME_MODE_TOURNAMENT) {
        unk990->birdieBadgeCounts[g_CourseIndex][g_HoleIndex]++;
        if (unk990->birdieBadgeCounts[g_CourseIndex][g_HoleIndex] > MAX_SAVED_COUNT) {
            unk990->birdieBadgeCounts[g_CourseIndex][g_HoleIndex] = MAX_SAVED_COUNT;
        }
        TransferSaveData(GetSaveData(), TRUE);
    }
}

/*
 * Passes how many holes have a birdie badge to func_8007FE44, if this is a
 * tournament and the current hole has none yet
 */
void ReportBirdieBadgeCount(void) {
    Unk990 *unk990;
    s32 badgeCount;
    s32 courseIndex;
    s32 holeIndex;

    unk990 = &GetSaveData()->unk990;
    if (g_GameMode == GAME_MODE_TOURNAMENT && unk990->birdieBadgeCounts[g_CourseIndex][g_HoleIndex] == 0) {
        badgeCount = 0;
        for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
            for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
                badgeCount += unk990->birdieBadgeCounts[courseIndex][holeIndex] != FALSE;
            }
        }
        func_8007FE44(badgeCount);
    }
}

/* Flags the course's statistics with the player's place in the tournament, if third or better, and saves */
#ifdef NON_MATCHING
void RecordTournamentPlace(void) {
    UnkE98 *stats;
    s32 *placeFlag;
    s32 hasPlace;
    s32 place;
    s32 index;
    TournamentStanding *standing;

    stats = &GetSaveData()->unkE98[g_CourseIndex];
    hasPlace = FALSE;
    if (g_GameMode == GAME_MODE_TOURNAMENT) {
        RankTournament(GetLastHoleIndex() + 1, 0, func_8006CD50(GetLastHoleIndex() + 1, 0), D_801B60B4);
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
                placeFlag = &stats->hasFirstPlace;
                hasPlace = TRUE;
                break;
            case 2:
                placeFlag = &stats->hasSecondPlace;
                hasPlace = TRUE;
                break;
            case 3:
                placeFlag = &stats->hasThirdPlace;
                hasPlace = TRUE;
                break;
        }
        if (hasPlace) {
            *placeFlag = TRUE;
        }
        TransferSaveData(GetSaveData(), TRUE);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/progress", RecordTournamentPlace);
#endif
