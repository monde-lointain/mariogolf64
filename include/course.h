#ifndef COURSE_H
#define COURSE_H

#include <ultra64.h>

#include "c_language.h"

#define COURSE_COUNT 6
#define COURSE_HOLE_COUNT 18
#define HOLES_PER_NINE 9
#define RING_SHOT_COURSE_COUNT 6
#define RING_SHOT_HOLE_COUNT 6
#define RING_SHOT_STARS_PER_OPENED_COURSE 4

enum CourseRecordIndex {
    COURSE_RECORD_TOAD_HIGHLANDS,
    COURSE_RECORD_KOOPA_PARK,
    COURSE_RECORD_SHY_GUY_DESERT,
    COURSE_RECORD_YOSHIS_ISLAND,
    COURSE_RECORD_BOO_VALLEY,
    COURSE_RECORD_MARIOS_STAR,
    COURSE_RECORD_LUIGIS_GARDEN,
    COURSE_RECORD_PEACHS_CASTLE,
    COURSE_RECORD_DRIVING_RANGE,
    COURSE_RECORD_COUNT
};

typedef s32 CourseRecordIndex;

enum Course {
    COURSE_TOAD_HIGHLANDS,
    COURSE_KOOPA_PARK,
    COURSE_SHY_GUY_DESERT,
    COURSE_YOSHIS_ISLAND,
    COURSE_BOO_VALLEY,
    COURSE_MARIOS_STAR,
    COURSE_LUIGIS_GARDEN_FIRST,
    COURSE_LUIGIS_GARDEN_SECOND,
    COURSE_LUIGIS_GARDEN_THIRD,
    COURSE_PEACHS_CASTLE_FIRST,
    COURSE_PEACHS_CASTLE_SECOND,
    COURSE_PEACHS_CASTLE_THIRD,
    COURSE_DRIVING_RANGE
};

#define MINI_GOLF_COURSE_INDEX COURSE_LUIGIS_GARDEN_FIRST
#define MINI_GOLF_COURSES_PER_RECORD 3
#define DRIVING_RANGE_COURSE_INDEX COURSE_DRIVING_RANGE
#define COURSE_INDEX_COUNT (DRIVING_RANGE_COURSE_INDEX + 1)

typedef s32 CourseIndex;
typedef s8 CourseIndex8;

extern s32 g_CourseNameEnvColors[];
extern s32 g_CourseNamePrimColors[];

#define HOLE_FLAG_MARKED_PAR_FIVE (1 << 0)
#define HOLE_FLAG_PAR_THREE (1 << 1)
typedef u8 UHoleFlags8;

#define PAR_THREE 3
#define PAR_FOUR 4
#define PAR_FIVE 5

#define TOURNAMENT_NAME_COUNT 7
#define TOURNAMENT_NAME_SIZE 25

#define HOLE_YARDAGE_COUNT 3
#define SHOWN_YARDAGE 0

typedef struct HoleRecord {
    s8 par;
    s8 handicap;
    u16 yardages[HOLE_YARDAGE_COUNT];
    UHoleFlags8 flags;
} HoleRecord;

typedef struct CourseRecord {
    char name[20];
    HoleRecord holes[18];
} CourseRecord;

typedef struct RingShotHole {
    u8 courseRecordIndex;
    u8 holeIndex;
} RingShotHole;

extern CourseRecord g_CourseRecords[COURSE_RECORD_COUNT];
extern char g_TournamentNames[TOURNAMENT_NAME_COUNT][TOURNAMENT_NAME_SIZE];

s32 GetHoleRadius(CourseRecordIndex courseRecordIndex, s32 holeIndex);
CourseRecordIndex GetCourseRecordIndex();
s32 GetHoleRecordIndex(void);
s32 GetHoleIndexInRecord(void);
Boolean IsMarkedParFive(s32 courseRecordIndex, s32 holeIndex);
Boolean IsParThree(s32 courseRecordIndex, s32 holeIndex);

#endif
