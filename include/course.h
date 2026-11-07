#ifndef COURSE_H
#define COURSE_H

#include <ultra64.h>

#include "language.h"

#define COURSE_COUNT 6
#define COURSE_HOLE_COUNT 18
#define HOLES_PER_NINE 9
#define RING_SHOT_COURSE_COUNT 6
#define RING_SHOT_HOLE_COUNT 6

/*
 * The courses' layouts are kept as course records. The six main courses
 * have one each, as do the two mini-golf courses and the driving range.
 */
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

/* An enum CourseRecordIndex value */
typedef s32 CourseRecordIndex;

/*
 * GameState.courseIndex numbers the main courses from 0, then the mini-golf
 * ones, which share a record three at a time, from MINI_GOLF_COURSE_INDEX. From
 * DRIVING_RANGE_COURSE_INDEX on, courses use the driving range's record.
 */
#define MINI_GOLF_COURSE_INDEX 6
#define MINI_GOLF_COURSES_PER_RECORD 3
#define DRIVING_RANGE_COURSE_INDEX 12
#define COURSE_INDEX_COUNT (DRIVING_RANGE_COURSE_INDEX + 1)

/* A course as GameState.courseIndex numbers it; the main courses are its
 * enum Course values */
typedef s32 CourseIndex;

/* The main courses, which keep their records' order */
enum Course {
    COURSE_TOAD_HIGHLANDS,
    COURSE_KOOPA_PARK,
    COURSE_SHY_GUY_DESERT,
    COURSE_YOSHIS_ISLAND,
    COURSE_BOO_VALLEY,
    COURSE_MARIOS_STAR
};

/* The colors of each course's name, which gradients from the first to the
 * second */
extern s32 g_CourseNameEnvColors[];
extern s32 g_CourseNamePrimColors[];

/* HoleRecord.flags; the main courses mark three or four par 5s and every par 3
 */
#define HOLE_FLAG_MARKED_PAR_FIVE (1 << 0)
#define HOLE_FLAG_PAR_THREE (1 << 1)

/* The pars a hole can have */
#define PAR_THREE 3
#define PAR_FOUR 4
#define PAR_FIVE 5

/* Each main course's tournament, then the tournaments' total */
#define TOURNAMENT_NAME_COUNT 7
#define TOURNAMENT_NAME_SIZE 25

typedef struct HoleRecord {
    /* 0x0 */ s8 par;
    /* 0x1 */ s8 handicap; /* Ranks a course's holes from 1 to 18 */
    /* 0x2 */ u16 yardages[3]; /* The three are equal in every hole */
    /* 0x8 */ u8 flags;
} HoleRecord; /* size = 0xA */

typedef struct CourseRecord {
    /* 0x00 */ char name[20];
    /* 0x14 */ HoleRecord holes[18];
} CourseRecord; /* size = 0xC8 */

/* A Ring Shot hole is one of the holes of a course record */
typedef struct RingShotHole {
    /* 0x0 */ u8 courseRecordIndex;
    /* 0x1 */ u8 holeIndex;
} RingShotHole; /* size = 0x2 */

extern CourseRecord g_CourseRecords[COURSE_RECORD_COUNT];
extern char g_TournamentNames[TOURNAMENT_NAME_COUNT][TOURNAMENT_NAME_SIZE];
/* The par of the hole being played */

s32 GetHoleRadius(CourseRecordIndex courseRecordIndex, s32 holeIndex);
/*
 * Takes no parameters, but is left unprototyped because GetHandicapOffset
 * passes it a course index, which is not used
 */
CourseRecordIndex GetCourseRecordIndex();
s32 GetHoleRecordIndex(void);
s32 GetHoleIndexInRecord(void);
Boolean IsMarkedParFive(s32 courseRecordIndex, s32 holeIndex);
Boolean IsParThree(s32 courseRecordIndex, s32 holeIndex);

#endif
