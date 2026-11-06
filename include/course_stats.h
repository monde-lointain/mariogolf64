#ifndef COURSE_STATS_H
#define COURSE_STATS_H

#include <ultra64.h>

#include "character.h"
#include "course.h"
#include "language.h"

/*
 * The courses' statistics over every round played, and the figures
 * that the menus show from them
 */

/* Indices of CourseStats.hasPlace */
#define THIRD_PLACE_INDEX 0
#define SECOND_PLACE_INDEX 1
#define FIRST_PLACE_INDEX 2
#define TOURNAMENT_PLACE_COUNT 3

/* A course's statistics over every round played on it */
typedef struct CourseStats {
  /* 0x00 */ s32 roundCount;
  /* 0x04 */ s32 bestScore; /* The lowest net score to par over a round */
  /* 0x08 */ s32 strokeTotal;
  /* 0x0C */ s32 longestDrive;
  /* 0x10 */ s32 averageDrive;
  /* 0x14 */ s32 averagedDriveCount;
  /* 0x18 */ s32
      measuredDriveTotal; /* The sum of RoundStats.measuredDriveCount */
  /* 0x1C */ s32 puttTotal;
  /* 0x20 */ s32 parOrBetterCount; /* Holes played in par or fewer strokes */
  /* 0x24 */ s32
      parOnCount; /* Holes whose green was reached with two strokes to spare */
  /* 0x28 */ s32 sandSaveCount; /* The sum of RoundStats.sandSaveCount */
  /* 0x2C */ s32
      missedSandSaveCount; /* The sum of RoundStats.missedSandSaveCount */
  /* In a tournament on the course */
  /* 0x30 */ Boolean hasPlace[TOURNAMENT_PLACE_COUNT];
  /* 0x3C */ s16
      characterBestScores[CHARACTER_COUNT]; /* bestScore, by character */
  /* 0x60 */ u8 hasCharacterBestScore[CHARACTER_COUNT];
  /* 0x72 */ u8 unusedAfterBestScores[0x74 - 0x72];
} CourseStats; /* size = 0x74 */

/*
 * The statistics that the menus show, worked out from CourseStats: for
 * each course, then for all of them
 */
typedef struct CourseStatFigures {
  /* 0x00 */ f32 roundCount;
  /* 0x04 */ f32 bestScore;
  /* 0x08 */ f32 averageScore;
  /* 0x0C */ f32 bestDrive;
  /* 0x10 */ f32 averageDrive;
  /* Only ever cleared, for a course with no rounds */
  /* 0x14 */ f32 unusedDriveFigure;
  /* 0x18 */ f32 fairwayPercent;
  /* 0x1C */ f32 averagePutts;
  /* 0x20 */ f32 parSavePercent;
  /* 0x24 */ f32 parOnPercent;
  /* 0x28 */ f32 sandSavePercent;
  /* 0x2C */ Boolean hasPlace[TOURNAMENT_PLACE_COUNT];
} CourseStatFigures; /* size = 0x38 */

extern CourseStatFigures g_CourseStatFigures[COURSE_COUNT + 1];

#define HOLES_PER_ROUND 18.0f

void ComputeRoundStatistics(void);

#endif
