#ifndef COLLISION_H
#define COLLISION_H

#include <ultra64.h>

#include "ball_simulation.h"
#include "course_space.h"
#include "language.h"
#include "terrain.h"

enum PinHit {
    PIN_HIT_NONE,
    PIN_HIT_FLAGSTICK,
    PIN_HIT_FLAG
};

typedef s32 PinHit;

enum BillboardCell {
    BILLBOARD_CELL_EMPTY,
    BILLBOARD_CELL_TRUNK,
    BILLBOARD_CELL_FOLIAGE,
    BILLBOARD_CELL_IGNORED,
    BILLBOARD_CELL_SOLID,
    BILLBOARD_CELL_SHELL
};

typedef s32 BillboardCell;

s32 FindMiniGolfGroundHit(
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
s32 FindWallHit(
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    PlaneTriangle *hit,
    f32 *fraction
);
s32 PushBallOutOfWalls(BallSimulation *simulation, s32 checksHeight);
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
s32 FindHoleTriangleHit(
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    PlaneTriangle *hit,
    f32 *fraction
);
s32 FindGroundHit(
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
