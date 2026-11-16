/*
 * Facts about the courses and their holes: where their records are kept, and
 * which holes are par threes or are marked as par fives.
 */

#include "course/course.h"

#include <ultra64.h>

#include "course/course_records.h"
#include "game/scene.h"
#include "round/game_state.h"
#include "system/c_language.h"
#include "system/main.h"

// The radius used for holes not listed in GetHoleRadius
#define DEFAULT_HOLE_RADIUS 200
#define MINI_GOLF_HOLE_RADIUS 30

char g_TournamentNames[TOURNAMENT_NAME_COUNT][TOURNAMENT_NAME_SIZE] = {
    TOAD_HIGHLANDS_TOURNAMENT_NAME,
    KOOPA_PARK_TOURNAMENT_NAME,
    SHY_GUY_DESERT_TOURNAMENT_NAME,
    YOSHIS_ISLAND_TOURNAMENT_NAME,
    BOO_VALLEY_TOURNAMENT_NAME,
    MARIOS_STAR_TOURNAMENT_NAME,
    "Tournament Total",
};
// Each course's name, then its holes in the order they are played
CourseRecord g_CourseRecords[COURSE_RECORD_COUNT] = {
    {TOAD_HIGHLANDS_NAME, {TOAD_HIGHLANDS_HOLE_RECORDS}},
    {KOOPA_PARK_NAME, {KOOPA_PARK_HOLE_RECORDS}},
    {SHY_GUY_DESERT_NAME, {SHY_GUY_DESERT_HOLE_RECORDS}},
    {YOSHIS_ISLAND_NAME, {YOSHIS_ISLAND_HOLE_RECORDS}},
    {BOO_VALLEY_NAME, {BOO_VALLEY_HOLE_RECORDS}},
    {MARIOS_STAR_NAME, {MARIOS_STAR_HOLE_RECORDS}},
    {LUIGIS_GARDEN_NAME, {LUIGIS_GARDEN_HOLE_RECORDS}},
    {PEACHS_CASTLE_NAME, {PEACHS_CASTLE_HOLE_RECORDS}},
    {DRIVING_RANGE_NAME, {DRIVING_RANGE_HOLE_RECORDS}},
};
static RingShotHole f_RingShotHoles[RING_SHOT_COURSE_COUNT][RING_SHOT_HOLE_COUNT] = {
    {{0, 0}, {0, 16}, {0, 15}, {0, 13}, {0, 17}, {0, 6}},
    {{1, 1}, {1, 13}, {1, 11}, {1, 4}, {1, 2}, {1, 17}},
    {{2, 13}, {2, 2}, {2, 12}, {2, 6}, {2, 4}, {2, 16}},
    {{3, 14}, {3, 4}, {3, 7}, {3, 11}, {3, 16}, {3, 6}},
    {{4, 0}, {4, 10}, {4, 15}, {4, 14}, {4, 17}, {4, 1}},
    {{5, 14}, {5, 10}, {5, 8}, {5, 5}, {5, 16}, {5, 15}},
};

// The one caller scales the sine and cosine of an angle by this, so it is
// taken to be a radius
s32 GetHoleRadius(CourseRecordIndex courseRecordIndex, s32 holeIndex) {
    switch (courseRecordIndex) {
        case COURSE_RECORD_TOAD_HIGHLANDS:
            if (holeIndex == 10) {
                return 400;
            }

            if (holeIndex == 16) {
                return 400;
            }

            break;
        case COURSE_RECORD_KOOPA_PARK:
            if (holeIndex == 1) {
                return 400;
            }

            if (holeIndex == 5) {
                return 600;
            }

            if (holeIndex == 7) {
                return 150;
            }

            if (holeIndex == 16) {
                return 300;
            }

            break;
        case COURSE_RECORD_SHY_GUY_DESERT:
            if (holeIndex == 2) {
                return 300;
            }

            if (holeIndex == 12) {
                return 600;
            }

            if (holeIndex == 16) {
                return 550;
            }

            break;
        case COURSE_RECORD_YOSHIS_ISLAND:
            if (holeIndex == 2) {
                return 300;
            }

            if (holeIndex == 4) {
                return 1000;
            }

            if (holeIndex == 9) {
                return 700;
            }

            if (holeIndex == 10) {
                return 400;
            }

            if (holeIndex == 11) {
                return 450;
            }

            if (holeIndex == 12) {
                return 400;
            }

            if (holeIndex == 13) {
                return 600;
            }

            if (holeIndex == 16) {
                return 900;
            }

            break;
        case COURSE_RECORD_BOO_VALLEY:
            if (holeIndex == 10) {
                return 400;
            }

            if (holeIndex == 15) {
                return 300;
            }

            break;
        case COURSE_RECORD_MARIOS_STAR:
            if (holeIndex == 15) {
                return 400;
            }

            if (holeIndex == 16) {
                return 700;
            }

            if (holeIndex == 17) {
                return 400;
            }

            break;
        case COURSE_RECORD_LUIGIS_GARDEN:
        case COURSE_RECORD_PEACHS_CASTLE:
            return MINI_GOLF_HOLE_RADIUS;
    }

    return DEFAULT_HOLE_RADIUS;
}

CourseRecordIndex GetCourseRecordIndex(void) {
    if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
        return f_RingShotHoles[g_GameState.courseIndex][g_GameState.holeIndex].courseRecordIndex;
    }

    if (g_GameState.isMiniGolf) {
        return (((g_GameState.courseIndex - MINI_GOLF_COURSE_INDEX) /
                    MINI_GOLF_COURSES_PER_RECORD) &
                   1) |
            COURSE_RECORD_LUIGIS_GARDEN;
    }

    if (g_GameState.courseIndex >= DRIVING_RANGE_COURSE_INDEX) {
        return COURSE_RECORD_DRIVING_RANGE;
    }

    return g_GameState.courseIndex;
}

s32 GetHoleRecordIndex(void) {
    if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
        return f_RingShotHoles[g_GameState.courseIndex][g_GameState.holeIndex].holeIndex;
    }

    if (g_GameState.courseIndex >= DRIVING_RANGE_COURSE_INDEX) {
        return 0;
    }

    return g_GameState.holeIndex;
}

// GetHoleRecordIndex as if outside Ring Shot
s32 GetHoleIndexInRecord(void) {
    if (g_GameState.courseIndex >= DRIVING_RANGE_COURSE_INDEX) {
        return 0;
    }

    return g_GameState.holeIndex;
}

// With g_IsDebugEnabled set, the first hole counts too
Boolean IsMarkedParFive(CourseRecordIndex courseRecordIndex, s32 holeIndex) {
    if (g_GameState.isMiniGolf) {
        return FALSE;
    }

    if (g_IsDebugEnabled && holeIndex == 0) {
        return TRUE;
    }

    return g_CourseRecords[courseRecordIndex].holes[holeIndex].flags & HOLE_FLAG_MARKED_PAR_FIVE;
}

Boolean IsParThree(CourseRecordIndex courseRecordIndex, s32 holeIndex) {
    if (g_GameState.isMiniGolf) {
        return FALSE;
    }

    return g_CourseRecords[courseRecordIndex].holes[holeIndex].flags & HOLE_FLAG_PAR_THREE;
}
