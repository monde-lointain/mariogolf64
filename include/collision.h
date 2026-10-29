#ifndef COLLISION_H
#define COLLISION_H

#include "common.h"

#include <ultra64.h>

#include "ball_simulation.h"
#include "functions.h"

/* What FindPinHit returns */
enum PinHit {
  PIN_HIT_NONE,
  PIN_HIT_FLAGSTICK,
  PIN_HIT_FLAG
};

/*
 * What a billboard's hit mask marks at a point, which decides how a ball
 * bounces off it and the sound it makes
 */
enum BillboardCell {
  BILLBOARD_CELL_EMPTY,
  BILLBOARD_CELL_TRUNK, /* A tree's trunk */
  BILLBOARD_CELL_FOLIAGE, /* A tree's foliage, which only slows the ball */
  BILLBOARD_CELL_IGNORED, /* The ragged edge of foliage, treated as empty */
  /* A solid object, such as a cactus, Shy Guy, mushroom, fruit or Bob-omb */
  BILLBOARD_CELL_SOLID,
  BILLBOARD_CELL_SHELL /* A Koopa or Spiny shell */
};

typedef s32 BillboardCell;

s32 FindMiniGolfGroundHit(BallSimulation* simulation, s32 startX, s32 startY,
    s32 startZ, s32 endX, s32 endY, s32 endZ, PlaneTriangle* hit,
    f32* fraction);
s32 FindBillboardHit(s32 startX, s32 startY, s32 startZ, s32 endX, s32 endY,
    s32 endZ, BillboardCell* cell, f32* fraction);
void MoveBallToCupEdge(BallSimulation* simulation);
s32 FindPinHit(s32 startX, s32 startY, s32 startZ, s32 endX, s32 endY, s32 endZ,
    CoursePoint velocity, f32* fraction, s32 hasHitFlag,
    BallSimulation* simulation);
s32 FindWallHit(s32 startX, s32 startY, s32 startZ, s32 endX, s32 endY,
    s32 endZ, PlaneTriangle* hit, f32* fraction);
s32 PushBallOutOfWalls(BallSimulation* simulation, s32 checksHeight);
s32 FindRingHit(s32 startX, s32 startY, s32 startZ, s32 endX, s32 endY,
    s32 endZ, PlaneTriangle* hitRing, f32* fraction);

#endif
