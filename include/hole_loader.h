#ifndef HOLE_LOADER_H
#define HOLE_LOADER_H

#include <ultra64.h>

#include "course.h"
#include "course_space.h"
#include "language.h"

#define CUP_POSITION_COUNT 4
#define ROUTE_POINT_COUNT 8
#define AIM_POINT_COUNT 8

typedef struct HolePoints {
    CoursePoint cupPositions[CUP_POSITION_COUNT];
    CoursePoint teePositions[CUP_POSITION_COUNT];
    CoursePoint aimPoints[AIM_POINT_COUNT];
    CoursePoint routePoints[ROUTE_POINT_COUNT];
} HolePoints;

extern HolePoints g_HolePoints;

#ifdef __IS_KMC__
extern Boolean g_HasLoaderFinished;
#else
extern volatile Boolean g_HasLoaderFinished;
#endif

extern s32 g_HoleTimeFrames[COURSE_HOLE_COUNT];
extern s32 g_HoleTimeMinutes[COURSE_HOLE_COUNT];
extern s32 g_HoleTimeSeconds[COURSE_HOLE_COUNT];
extern s32 g_HoleTimeHundredths[COURSE_HOLE_COUNT];

void ResetLoaderThread(void);
void StartLoaderThread(void);
void StopLoaderThread(void);

s32 LoadLzData(u32 romAddress, void *destination, u32 size);
s32 LoadPatchedLzData(u32 romAddress, void *destination, u32 size);
void ChooseConditionsAndInitHole(void);

#endif
