#include "course.h"

#include <ultra64.h>

#include "c_language.h"
#include "game_state.h"
#include "main.h"
#include "scene.h"

#define DEFAULT_HOLE_RADIUS 200
#define MINI_GOLF_HOLE_RADIUS 30

#define HOLE_RECORD(par, handicap, yardage, flags) \
    {(par), (handicap), {(yardage), (yardage), (yardage)}, (flags)}

char g_TournamentNames[TOURNAMENT_NAME_COUNT][TOURNAMENT_NAME_SIZE] = {
    "Toad Tournament",
    "Koopa Cup",
    "Shy Guy International",
    "Yoshi Championship",
    "Boo Clasic",
    "Mario Open",
    "Tournament Total",
};
CourseRecord g_CourseRecords[COURSE_RECORD_COUNT] = {
    {
        "Toad HighLands",
        {
            HOLE_RECORD(4, 11, 363, 0),
            HOLE_RECORD(3, 1, 161, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 5, 343, 0),
            HOLE_RECORD(5, 13, 477, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(4, 15, 355, 0),
            HOLE_RECORD(3, 7, 138, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 3, 387, 0),
            HOLE_RECORD(5, 17, 511, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(4, 9, 409, 0),
            HOLE_RECORD(4, 16, 304, 0),
            HOLE_RECORD(4, 4, 380, 0),
            HOLE_RECORD(5, 12, 466, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(3, 2, 165, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 10, 388, 0),
            HOLE_RECORD(4, 8, 369, 0),
            HOLE_RECORD(5, 18, 448, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(3, 14, 149, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 6, 400, 0),
        },
    },
    {
        "Koopa Park",
        {
            HOLE_RECORD(4, 11, 372, 0),
            HOLE_RECORD(5, 15, 527, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(4, 5, 375, 0),
            HOLE_RECORD(3, 7, 169, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 13, 400, 0),
            HOLE_RECORD(5, 17, 545, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(4, 9, 380, 0),
            HOLE_RECORD(3, 3, 140, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 1, 362, 0),
            HOLE_RECORD(4, 8, 381, 0),
            HOLE_RECORD(4, 16, 366, 0),
            HOLE_RECORD(5, 6, 512, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(3, 14, 174, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 2, 392, 0),
            HOLE_RECORD(4, 10, 436, 0),
            HOLE_RECORD(3, 4, 154, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 12, 360, 0),
            HOLE_RECORD(5, 18, 520, HOLE_FLAG_MARKED_PAR_FIVE),
        },
    },
    {
        "Shy Guy Desert",
        {
            HOLE_RECORD(4, 11, 360, 0),
            HOLE_RECORD(3, 13, 153, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(5, 15, 520, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(4, 1, 390, 0),
            HOLE_RECORD(4, 5, 400, 0),
            HOLE_RECORD(4, 9, 420, 0),
            HOLE_RECORD(3, 7, 170, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 3, 403, 0),
            HOLE_RECORD(5, 17, 550, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(4, 2, 380, 0),
            HOLE_RECORD(4, 8, 412, 0),
            HOLE_RECORD(3, 4, 177, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 12, 370, 0),
            HOLE_RECORD(5, 18, 535, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(3, 6, 164, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 14, 340, 0),
            HOLE_RECORD(5, 10, 570, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(4, 16, 398, 0),
        },
    },
    {
        "Yoshi's Island",
        {
            HOLE_RECORD(4, 9, 350, 0),
            HOLE_RECORD(4, 15, 400, 0),
            HOLE_RECORD(3, 5, 170, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(5, 13, 470, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(4, 17, 382, 0),
            HOLE_RECORD(3, 3, 155, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(5, 1, 544, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(4, 11, 420, 0),
            HOLE_RECORD(4, 7, 365, 0),
            HOLE_RECORD(4, 6, 375, 0),
            HOLE_RECORD(5, 18, 507, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(3, 10, 140, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 16, 415, 0),
            HOLE_RECORD(4, 12, 395, 0),
            HOLE_RECORD(3, 2, 190, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 8, 398, 0),
            HOLE_RECORD(4, 4, 389, 0),
            HOLE_RECORD(5, 14, 530, HOLE_FLAG_MARKED_PAR_FIVE),
        },
    },
    {
        "Boo Valley",
        {
            HOLE_RECORD(4, 17, 373, 0),
            HOLE_RECORD(4, 5, 400, 0),
            HOLE_RECORD(5, 11, 534, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(3, 3, 199, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 7, 335, 0),
            HOLE_RECORD(4, 13, 411, 0),
            HOLE_RECORD(5, 15, 546, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(3, 9, 136, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 1, 402, 0),
            HOLE_RECORD(5, 16, 562, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(4, 4, 357, 0),
            HOLE_RECORD(3, 6, 152, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 14, 396, 0),
            HOLE_RECORD(4, 8, 391, 0),
            HOLE_RECORD(3, 18, 170, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 2, 407, 0),
            HOLE_RECORD(5, 12, 484, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(4, 10, 380, 0),
        },
    },
    {
        "Mario's Star",
        {
            HOLE_RECORD(4, 17, 338, 0),
            HOLE_RECORD(4, 1, 414, 0),
            HOLE_RECORD(5, 11, 510, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(3, 3, 194, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 13, 410, 0),
            HOLE_RECORD(3, 15, 149, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(5, 9, 500, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(4, 7, 346, 0),
            HOLE_RECORD(4, 5, 416, 0),
            HOLE_RECORD(4, 12, 400, 0),
            HOLE_RECORD(5, 8, 515, 0),
            HOLE_RECORD(3, 4, 172, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(4, 2, 400, 0),
            HOLE_RECORD(4, 16, 380, 0),
            HOLE_RECORD(4, 18, 419, 0),
            HOLE_RECORD(3, 14, 195, HOLE_FLAG_PAR_THREE),
            HOLE_RECORD(5, 10, 520, HOLE_FLAG_MARKED_PAR_FIVE),
            HOLE_RECORD(4, 6, 395, 0),
        },
    },
    {
        "Luigi's Garden",
        {
            HOLE_RECORD(3, 17, 363, 0),
            HOLE_RECORD(3, 7, 165, 0),
            HOLE_RECORD(3, 11, 343, 0),
            HOLE_RECORD(3, 9, 477, 0),
            HOLE_RECORD(3, 3, 355, 0),
            HOLE_RECORD(3, 13, 145, 0),
            HOLE_RECORD(3, 15, 387, 0),
            HOLE_RECORD(3, 1, 510, 0),
            HOLE_RECORD(3, 5, 410, 0),
            HOLE_RECORD(3, 12, 338, 0),
            HOLE_RECORD(3, 10, 380, 0),
            HOLE_RECORD(3, 6, 466, 0),
            HOLE_RECORD(3, 14, 170, 0),
            HOLE_RECORD(3, 8, 395, 0),
            HOLE_RECORD(3, 18, 369, 0),
            HOLE_RECORD(3, 4, 448, 0),
            HOLE_RECORD(3, 16, 150, 0),
            HOLE_RECORD(3, 2, 400, 0),
        },
    },
    {
        "Peach's Castle",
        {
            HOLE_RECORD(3, 17, 363, 0),
            HOLE_RECORD(3, 7, 165, 0),
            HOLE_RECORD(3, 11, 343, 0),
            HOLE_RECORD(3, 9, 477, 0),
            HOLE_RECORD(3, 3, 355, 0),
            HOLE_RECORD(3, 13, 145, 0),
            HOLE_RECORD(3, 15, 387, 0),
            HOLE_RECORD(3, 5, 510, 0),
            HOLE_RECORD(3, 1, 410, 0),
            HOLE_RECORD(3, 16, 338, 0),
            HOLE_RECORD(3, 14, 380, 0),
            HOLE_RECORD(3, 8, 466, 0),
            HOLE_RECORD(3, 10, 170, 0),
            HOLE_RECORD(3, 6, 395, 0),
            HOLE_RECORD(3, 18, 369, 0),
            HOLE_RECORD(3, 4, 448, 0),
            HOLE_RECORD(3, 12, 150, 0),
            HOLE_RECORD(3, 2, 400, 0),
        },
    },
    {
        "Driving Range",
        {
            HOLE_RECORD(3, 1, 363, 0),
            HOLE_RECORD(3, 1, 165, 0),
            HOLE_RECORD(3, 1, 343, 0),
            HOLE_RECORD(3, 1, 477, 0),
            HOLE_RECORD(3, 1, 355, 0),
            HOLE_RECORD(3, 1, 145, 0),
            HOLE_RECORD(3, 1, 387, 0),
            HOLE_RECORD(3, 1, 510, 0),
            HOLE_RECORD(3, 1, 410, 0),
            HOLE_RECORD(3, 1, 338, 0),
            HOLE_RECORD(3, 1, 380, 0),
            HOLE_RECORD(3, 1, 466, 0),
            HOLE_RECORD(3, 1, 170, 0),
            HOLE_RECORD(3, 1, 395, 0),
            HOLE_RECORD(3, 1, 369, 0),
            HOLE_RECORD(3, 1, 448, 0),
            HOLE_RECORD(3, 1, 150, 0),
            HOLE_RECORD(3, 1, 400, 0),
        },
    },
};
static RingShotHole f_RingShotHoles[RING_SHOT_COURSE_COUNT][RING_SHOT_HOLE_COUNT] = {
    {{0, 0}, {0, 16}, {0, 15}, {0, 13}, {0, 17}, {0, 6}},
    {{1, 1}, {1, 13}, {1, 11}, {1, 4}, {1, 2}, {1, 17}},
    {{2, 13}, {2, 2}, {2, 12}, {2, 6}, {2, 4}, {2, 16}},
    {{3, 14}, {3, 4}, {3, 7}, {3, 11}, {3, 16}, {3, 6}},
    {{4, 0}, {4, 10}, {4, 15}, {4, 14}, {4, 17}, {4, 1}},
    {{5, 14}, {5, 10}, {5, 8}, {5, 5}, {5, 16}, {5, 15}},
};

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

s32 GetHoleIndexInRecord(void) {
    if (g_GameState.courseIndex >= DRIVING_RANGE_COURSE_INDEX) {
        return 0;
    }

    return g_GameState.holeIndex;
}

Boolean IsMarkedParFive(s32 courseRecordIndex, s32 holeIndex) {
    if (g_GameState.isMiniGolf) {
        return FALSE;
    }

    if (g_IsDebugEnabled && holeIndex == 0) {
        return TRUE;
    }

    return g_CourseRecords[courseRecordIndex].holes[holeIndex].flags & HOLE_FLAG_MARKED_PAR_FIVE;
}

Boolean IsParThree(s32 courseRecordIndex, s32 holeIndex) {
    if (g_GameState.isMiniGolf) {
        return FALSE;
    }

    return g_CourseRecords[courseRecordIndex].holes[holeIndex].flags & HOLE_FLAG_PAR_THREE;
}
