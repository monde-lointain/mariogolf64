/*
 * Facts about the courses and their holes: where their records are kept, the
 * holes' sizes, and their pars.
 */

#ifndef COURSE_H
#define COURSE_H

#include <ultra64.h>

#include "system/c_language.h"

#define COURSE_COUNT 6
#define COURSE_HOLE_COUNT 18
#define HOLES_PER_NINE 9
#define RING_SHOT_COURSE_COUNT 6
#define RING_SHOT_HOLE_COUNT 6
// Each Ring Shot course after the first opens after this many more stars
#define RING_SHOT_STARS_PER_OPENED_COURSE 4

// The courses' layouts are kept as course records. The six main courses
// have one each, as do the two mini-golf courses and the driving range.
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

// An enum CourseRecordIndex value
typedef s32 CourseRecordIndex;
typedef u8 UCourseRecordIndex8;

// The main courses, which keep their records' order
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

// GameState.courseIndex numbers the main courses from 0, then the mini-golf ones, which share a
// record three at a time, from MINI_GOLF_COURSE_INDEX. From DRIVING_RANGE_COURSE_INDEX on, courses
// use the driving range's record.
#define MINI_GOLF_COURSE_INDEX COURSE_LUIGIS_GARDEN_FIRST
#define MINI_GOLF_COURSES_PER_RECORD 3
#define DRIVING_RANGE_COURSE_INDEX COURSE_DRIVING_RANGE
#define COURSE_INDEX_COUNT (DRIVING_RANGE_COURSE_INDEX + 1)
// A course as GameState.courseIndex numbers it; the main courses are its enum Course values
typedef s32 CourseIndex;
typedef s8 CourseIndex8;
typedef s16 CourseIndex16;

// The colors of each course's name, which gradients from the first to the
// second
extern s32 g_CourseNameEnvColors[];
extern s32 g_CourseNamePrimColors[];

// HoleRecord.flags; the main courses mark three or four par 5s and every par 3
#define HOLE_FLAG_MARKED_PAR_FIVE (1 << 0)
#define HOLE_FLAG_PAR_THREE (1 << 1)
typedef u8 UHoleFlags8;

// The pars a hole can have
#define PAR_THREE 3
#define PAR_FOUR 4
#define PAR_FIVE 5

// Each main course's tournament, then the tournaments' total
#define TOURNAMENT_NAME_COUNT 7
#define TOURNAMENT_NAME_SIZE 25
#define HOLE_YARDAGE_COUNT 3
#define SHOWN_YARDAGE 0

typedef struct HoleRecord {
    s8 par;
    s8 handicap; // Ranks a course's holes from 1 to 18
    u16 yardages[HOLE_YARDAGE_COUNT]; // The three are equal in every hole
    UHoleFlags8 flags;
} HoleRecord;

typedef struct CourseRecord {
    char name[20];
    HoleRecord holes[18];
} CourseRecord;

// A Ring Shot hole is one of the holes of a course record
typedef struct RingShotHole {
    UCourseRecordIndex8 courseRecordIndex;
    u8 holeIndex;
} RingShotHole;

extern CourseRecord g_CourseRecords[COURSE_RECORD_COUNT];
extern char g_TournamentNames[TOURNAMENT_NAME_COUNT][TOURNAMENT_NAME_SIZE];

s32 GetHoleRadius(CourseRecordIndex courseRecordIndex, s32 holeIndex);
// Takes no parameters, but is left unprototyped because GetHandicapOffset
// passes it a course index, which is not used
CourseRecordIndex GetCourseRecordIndex();
s32 GetHoleRecordIndex(void);
s32 GetHoleIndexInRecord(void);
Boolean IsMarkedParFive(CourseRecordIndex courseRecordIndex, s32 holeIndex);
Boolean IsParThree(CourseRecordIndex courseRecordIndex, s32 holeIndex);

#endif
