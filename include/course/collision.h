/*
 * What the moving ball hits: the ground, the billboards, the pin, the hole
 * triangles, the walls and the rings.
 */

#ifndef COLLISION_H
#define COLLISION_H

#include <ultra64.h>

#include "course/course_space.h"
#include "course/terrain.h"
#include "shot/ball_simulation.h"
#include "system/c_language.h"

// What FindPinHit returns
enum PinHit {
    PIN_HIT_NONE,
    PIN_HIT_FLAGSTICK,
    PIN_HIT_FLAG
};

typedef s32 PinHit;

// What a billboard's hit mask marks at a point, which decides how a ball
// bounces off it and the sound it makes
enum BillboardCell {
    BILLBOARD_CELL_EMPTY,
    BILLBOARD_CELL_TRUNK, // A tree's trunk
    BILLBOARD_CELL_FOLIAGE, // A tree's foliage, which only slows the ball
    BILLBOARD_CELL_IGNORED, // The ragged edge of foliage, treated as empty
    // A solid object, such as a cactus, Shy Guy, mushroom, fruit or Bob-omb
    BILLBOARD_CELL_SOLID,
    BILLBOARD_CELL_SHELL // A Koopa or Spiny shell
};

typedef s32 BillboardCell;

Boolean FindMiniGolfGroundHit(
    BallSimulation *simulation,
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    PlaneTriangle *hit,
    f32 *fraction
);
s32 FindBillboardHit(
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    BillboardCell *cell,
    f32 *fraction
);

// The original's ball steppers call FindBillboardHit with a ninth
// argument, the simulation, which FindBillboardHit does not read. The cast
// goes through a function of no arguments, which GCC takes as meant.
// Calling through it is undefined, so otherwise the simulation is left out.
#ifndef AVOID_UB
typedef s32 (*BillboardHitFinder)(
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    BillboardCell *cell,
    f32 *fraction,
    BallSimulation *simulation
);
#    define FindSimulationBillboardHit( \
        startX, \
        startY, \
        startZ, \
        endX, \
        endY, \
        endZ, \
        cell, \
        fraction, \
        simulation \
    ) \
        ((BillboardHitFinder)(void (*)( \
            void \
        ))FindBillboardHit)(startX, startY, startZ, endX, endY, endZ, cell, fraction, simulation)
#else
#    define FindSimulationBillboardHit( \
        startX, \
        startY, \
        startZ, \
        endX, \
        endY, \
        endZ, \
        cell, \
        fraction, \
        simulation \
    ) \
        FindBillboardHit(startX, startY, startZ, endX, endY, endZ, cell, fraction)
#endif

void MoveBallToCupEdge(BallSimulation *simulation);
PinHit FindPinHit(
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    CoursePoint velocity,
    f32 *fraction,
    Boolean hasHitFlag,
    BallSimulation *simulation
);
Boolean FindWallHit(
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    PlaneTriangle *hit,
    f32 *fraction
);
s32 PushBallOutOfWalls(BallSimulation *simulation, Boolean checksHeight);
s32 FindRingHit(
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    PlaneTriangle *hitRing,
    f32 *fraction
);
// Finds the nearest hole triangle that a path hits. It sets the triangle,
// whose tag then holds the triangle's kind, and the fraction along
// the path, and returns whether one is hit.
Boolean FindHoleTriangleHit(
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    PlaneTriangle *hit,
    f32 *fraction
);
// Finds the nearest ground triangle that a path hits, and sets the triangle
// and the fraction along the path. It returns whether one is hit.
Boolean FindGroundHit(
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    PlaneTriangle *hit,
    f32 *fraction
);

#endif
