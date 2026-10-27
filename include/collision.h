#ifndef COLLISION_H
#define COLLISION_H

#include "common.h"

#include <ultra64.h>

#include "ball_simulation.h"
#include "functions.h"

/* What FindPinHit returns */
#define PIN_HIT_NONE 0
#define PIN_HIT_FLAGSTICK 1
#define PIN_HIT_FLAG 2

s32 FindMiniGolfGroundHit(BallSimulation* simulation, s32 startX, s32 startY,
    s32 startZ, s32 endX, s32 endY, s32 endZ, PlaneTriangle* hit,
    f32* fraction);
s32 FindBillboardHit(s32 startX, s32 startY, s32 startZ, s32 endX, s32 endY,
    s32 endZ, s32* cell, f32* fraction);
void MoveBallToCupEdge(BallSimulation* simulation);
s32 FindPinHit(s32 startX, s32 startY, s32 startZ, s32 endX, s32 endY, s32 endZ,
    CoursePoint velocity, f32* fraction, s32 unkB0, BallSimulation* simulation);
s32 FindWallHit(s32 startX, s32 startY, s32 startZ, s32 endX, s32 endY,
    s32 endZ, PlaneTriangle* hit, f32* fraction);
s32 PushBallOutOfWalls(BallSimulation* simulation, s32 checksHeight);
s32 FindRingHit(s32 startX, s32 startY, s32 startZ, s32 endX, s32 endY,
    s32 endZ, PlaneTriangle* hitRing, f32* fraction);

#endif
