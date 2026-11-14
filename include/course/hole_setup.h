/*
 * Where the hole's tee and cup are, its conditions and its par, as the loader
 * sets them up.
 */

#ifndef HOLE_SETUP_H
#define HOLE_SETUP_H

#include <ultra64.h>

#include "course/course_space.h"
#include "course/hole_conditions.h"

// The hole being played: where its cup and tee are and how it plays. The records keep a copy, and a
// saved game keeps one to resume the hole.
typedef struct HoleSetup {
    CoursePoint cupPosition;
    CoursePoint teePosition; // Where each player starts the hole
    HoleConditions conditions;
    s8 par;
    s8 nextCupIndex;
    u8 unusedAfterNextCup[0x34 - 0x2E];
} HoleSetup;

extern HoleSetup g_HoleSetup;

#endif
