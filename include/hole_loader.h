#ifndef HOLE_LOADER_H
#define HOLE_LOADER_H

#include <ultra64.h>

#include "course.h"
#include "course_space.h"
#include "language.h"

#define CUP_POSITION_COUNT 4
/* Points that the computer golfer may route its shots through */
#define ROUTE_POINT_COUNT 8
/* Points along the hole that shots aim for, the hole editor's "Meson" points */
#define AIM_POINT_COUNT 8

typedef struct HolePoints {
    /* 0x00 */ CoursePoint cupPositions[CUP_POSITION_COUNT]; /* Zero where a hole has fewer */
    /* 0x30 */ CoursePoint teePositions[CUP_POSITION_COUNT]; /* Only the first is read as the tee */
    /* 0x60 */ CoursePoint aimPoints[AIM_POINT_COUNT]; /* A par 4 or 5's tee shot
                                                       faces the first; zero
                                                       where unset */
    /* 0xC0 */ CoursePoint routePoints[ROUTE_POINT_COUNT]; /* For the computer golfer; zero where
                                                              unused */
} HolePoints; /* size = 0x120 */

extern HolePoints g_HolePoints;

/* Cleared while the loader thread loads a hole; set once it is done or stopped
 */
#ifdef __IS_KMC__
/* hole_loader.c defines the flag volatile, but the files that poll it saw it
 * without the qualifier, which their loops' code depends on. game.c declares
 * it itself, with the qualifier. */
extern Boolean g_HasLoaderFinished;
#else
extern volatile Boolean g_HasLoaderFinished;
#endif

/* Each hole's time in Speed Golf. A negative minute count shows as dashes. */
extern s32 g_HoleTimeFrames[COURSE_HOLE_COUNT]; /* Zero for a hole not yet timed */
extern s32 g_HoleTimeMinutes[COURSE_HOLE_COUNT];
extern s32 g_HoleTimeSeconds[COURSE_HOLE_COUNT];
extern s32 g_HoleTimeHundredths[COURSE_HOLE_COUNT];

void ResetLoaderThread(void);
void StartLoaderThread(void);
void StopLoaderThread(void);

/* Decompress the size bytes at romAddress into destination, returning the
 * decompressed size */
s32 LoadLzData(u32 romAddress, void *destination, u32 size);
s32 LoadPatchedLzData(u32 romAddress, void *destination, u32 size);
void ChooseConditionsAndInitHole(void);

#endif
