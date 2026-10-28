#ifndef COURSE_H
#define COURSE_H

#include <ultra64.h>

/*
 * The courses' layouts are kept as course records. The six main courses
 * have one each, as do the two mini-golf courses and the driving range.
 */
#define COURSE_RECORD_COUNT 9
#define COURSE_RECORD_TOAD_HIGHLANDS 0
#define COURSE_RECORD_KOOPA_PARK 1
#define COURSE_RECORD_YOSHIS_ISLAND 3
#define COURSE_RECORD_BOO_VALLEY 4
#define COURSE_RECORD_MARIOS_STAR 5
#define COURSE_RECORD_LUIGIS_GARDEN 6
#define COURSE_RECORD_PEACHS_CASTLE 7
#define COURSE_RECORD_DRIVING_RANGE 8

/*
 * GameState.courseIndex numbers the main courses from 0, then the mini-golf
 * ones, which share a record three at a time, from MINI_GOLF_COURSE_INDEX. From
 * DRIVING_RANGE_COURSE_INDEX on, courses use the driving range's record.
 */
#define MINI_GOLF_COURSE_INDEX 6
#define MINI_GOLF_COURSES_PER_RECORD 3
#define DRIVING_RANGE_COURSE_INDEX 12
#define COURSE_INDEX_COUNT (DRIVING_RANGE_COURSE_INDEX + 1)
/* The main courses keep their records' order */
#define COURSE_SHY_GUY_DESERT 2
#define COURSE_YOSHIS_ISLAND 3
#define COURSE_MARIOS_STAR 5

/* HoleRecord.flags; the main courses mark three or four par 5s and every par 3
 */
#define HOLE_FLAG_MARKED_PAR_FIVE (1 << 0)
#define HOLE_FLAG_PAR_THREE (1 << 1)

/* Each main course's tournament, then the tournaments' total */
#define TOURNAMENT_NAME_COUNT 7
#define TOURNAMENT_NAME_SIZE 25

typedef struct HoleRecord {
  /* 0x0 */ u8 par;
  /* 0x1 */ s8 handicap; /* Ranks a course's holes from 1 to 18 */
  /* 0x2 */ u16 yardages[3]; /* The three are equal in every hole */
  /* 0x8 */ u8 flags;
  /* 0x9 */ u8 unk9;
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

/*
 * g_GameState.courseIndex under a symbol of its own, which the routines
 * that read the field alone need to match
 */
extern s32 g_CourseIndex;
extern CourseRecord g_CourseRecords[COURSE_RECORD_COUNT];
extern char g_TournamentNames[TOURNAMENT_NAME_COUNT][TOURNAMENT_NAME_SIZE];
/* The par of the hole being played */

s32 GetHoleRadius(s32 courseRecordIndex, s32 holeIndex);
/*
 * Takes no parameters, but is left unprototyped because GetHandicapOffset
 * passes it a course index, which is not used
 */
s32 GetCourseRecordIndex();
s32 GetHoleRecordIndex(void);
s32 GetHoleIndexInRecord(void);
s32 IsMarkedParFive(s32 courseRecordIndex, s32 holeIndex);
s32 IsParThree(s32 courseRecordIndex, s32 holeIndex);

#endif
