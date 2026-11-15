/*
 * Loading holes on the loader thread and setting up their state.
 */

#ifndef HOLE_LOADER_H
#define HOLE_LOADER_H

#include <ultra64.h>

#include "course/course.h"
#include "course/course_space.h"
#include "system/c_language.h"

#define CUP_POSITION_COUNT 4
// Points that the computer golfer may route its shots through
#define ROUTE_POINT_COUNT 8
// Points along the hole that shots aim for, the hole editor's "Meson" points
#define AIM_POINT_COUNT 8
#define PLAYED_TEE 0
#define DEFAULT_CUP 0
#define TEE_SHOT_AIM_POINT 0
#define SECOND_SHOT_AIM_POINT 1

typedef struct HolePoints {
    CoursePoint cupPositions[CUP_POSITION_COUNT]; // Zero where a hole has fewer
    CoursePoint teePositions[CUP_POSITION_COUNT]; // Only the first is read as the tee
    // A par 4 or 5's tee shot faces the first; zero where unset
    CoursePoint aimPoints[AIM_POINT_COUNT];
    CoursePoint routePoints[ROUTE_POINT_COUNT]; // For the computer golfer; zero where unused
} HolePoints;

extern HolePoints g_HolePoints;

// Cleared while the loader thread loads a hole; set once it is done or stopped
#if defined(__IS_KMC__) && !defined(READS_LOADER_FLAG_VOLATILE)
// hole_loader.c defines the flag volatile, but the files that poll it saw it
// without the qualifier, which their loops' code depends on. game.c defines
// READS_LOADER_FLAG_VOLATILE to see the qualifier.
extern Boolean g_HasLoaderFinished;
#else
extern volatile Boolean g_HasLoaderFinished;
#endif

// Each hole's time in Speed Golf. A negative minute count shows as dashes.
extern s32 g_HoleTimeFrames[COURSE_HOLE_COUNT]; // Zero for a hole not yet timed
extern s32 g_HoleTimeMinutes[COURSE_HOLE_COUNT];
extern s32 g_HoleTimeSeconds[COURSE_HOLE_COUNT];
extern s32 g_HoleTimeHundredths[COURSE_HOLE_COUNT];

// The loader thread's steps, which load the current hole and prepare its
// scene. Both need the hole builder's overlay loaded.
void LoadHole(void);
void PrepareHoleScene(void);

void ResetLoaderThread(void);
void StartLoaderThread(void);
void StopLoaderThread(void);

// Decompress the size bytes at romAddress into destination, returning the
// decompressed size
s32 LoadLzData(u32 romAddress, void *destination, u32 size);
s32 LoadPatchedLzData(u32 romAddress, void *destination, u32 size);
void ChooseConditionsAndInitHole(void);

#endif
