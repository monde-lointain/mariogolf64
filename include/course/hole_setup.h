#ifndef HOLE_SETUP_H
#define HOLE_SETUP_H

#include <ultra64.h>

#include "course/course_space.h"
#include "course/hole_conditions.h"

typedef struct HoleSetup {
    CoursePoint cupPosition;
    CoursePoint teePosition;
    HoleConditions conditions;
    s8 par;
    s8 nextCupIndex;
    u8 unusedAfterNextCup[0x34 - 0x2E];
} HoleSetup;

extern HoleSetup g_HoleSetup;

#endif
