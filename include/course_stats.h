#ifndef COURSE_STATS_H
#define COURSE_STATS_H

#include <ultra64.h>

#include "characters/character.h"
#include "course.h"
#include "system/c_language.h"

#define THIRD_PLACE_INDEX 0
#define SECOND_PLACE_INDEX 1
#define FIRST_PLACE_INDEX 2
#define TOURNAMENT_PLACE_COUNT 3

typedef struct CourseStats {
    s32 roundCount;
    s32 bestScore;
    s32 strokeTotal;
    s32 longestDrive;
    s32 driveAverage;
    s32 averagedDriveCount;
    s32 measuredDriveTotal;
    s32 puttTotal;
    s32 parOrBetterCount;
    s32 parOnCount;
    s32 sandSaveCount;
    s32 missedSandSaveCount;
    Boolean hasPlace[TOURNAMENT_PLACE_COUNT];
    s16 characterBestScores[CHARACTER_COUNT];
    UBoolean8 hasCharacterBestScore[CHARACTER_COUNT];
    u8 unusedAfterBestScores[0x74 - 0x72];
} CourseStats;

typedef struct CourseStatFigures {
    f32 roundCount;
    f32 bestScore;
    f32 scoreAverage;
    f32 bestDrive;
    f32 driveAverage;
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
