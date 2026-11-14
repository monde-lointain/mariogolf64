/*
 * The courses' statistics over every round played, and the figures
 * that the menus show from them
 */

#ifndef COURSE_STATS_H
#define COURSE_STATS_H

#include <ultra64.h>

#include "characters/character.h"
#include "course/course.h"
#include "system/c_language.h"

// Indices of CourseStats.hasPlace
#define THIRD_PLACE_INDEX 0
#define SECOND_PLACE_INDEX 1
#define FIRST_PLACE_INDEX 2
#define TOURNAMENT_PLACE_COUNT 3

// A course's statistics over every round played on it
typedef struct CourseStats {
    s32 roundCount;
    s32 bestScore; // The lowest net score to par over a round
    s32 strokeTotal;
    s32 longestDrive;
    s32 driveAverage;
    s32 averagedDriveCount;
    s32 measuredDriveTotal; // The sum of RoundStats.measuredDriveCount
    s32 puttTotal;
    s32 parOrBetterCount; // Holes played in par or fewer strokes
    s32 parOnCount; // Holes whose green was reached with two strokes to spare
    s32 sandSaveCount; // The sum of RoundStats.sandSaveCount
    s32 missedSandSaveCount; // The sum of RoundStats.missedSandSaveCount
    // In a tournament on the course
    Boolean hasPlace[TOURNAMENT_PLACE_COUNT];
    s16 characterBestScores[CHARACTER_COUNT]; // bestScore, by character
    UBoolean8 hasCharacterBestScore[CHARACTER_COUNT];
    u8 unusedAfterBestScores[0x74 - 0x72];
} CourseStats;

// The statistics that the menus show, worked out from CourseStats: for
// each course, then for all of them
typedef struct CourseStatFigures {
    f32 roundCount; // A float, as the averages divide by it
    f32 bestScore;
    f32 scoreAverage;
    f32 bestDrive;
    f32 driveAverage;
    // Only ever cleared, for a course with no rounds
    f32 unusedDriveFigure;
    f32 fairwayPercent;
    f32 puttAverage;
    f32 parSavePercent;
    f32 parOnPercent;
    f32 sandSavePercent;
    Boolean hasPlace[TOURNAMENT_PLACE_COUNT];
} CourseStatFigures;

extern CourseStatFigures g_CourseStatFigures[COURSE_COUNT + 1];

#define HOLES_PER_ROUND 18.0f

void ComputeRoundStatistics(void);

#endif
