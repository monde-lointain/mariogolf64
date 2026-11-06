#ifndef HOLE_TRIANGLES_H
#define HOLE_TRIANGLES_H

#include "common.h"

#include <ultra64.h>

#include "camera.h"

/*
 * Single triangles that the hole editor places on a hole, apart from its
 * ground. Their kinds choose how they are drawn; the rings of Ring Shot are
 * one of the kinds, with the triangle's corners holding the ring's place.
 */

#define HOLE_TRIANGLE_COUNT 160
#define HOLE_TRIANGLE_VERTEX_COUNT 3

/* The ends of each axis's range in HoleTriangle.bounds */
enum BoundEnd {
  BOUND_LOWEST,
  BOUND_HIGHEST
};

#define BOUND_END_COUNT 2

/* HoleTriangle.kind's values */
/* Blended texture, mapped across x and y + z */
enum HoleTriangleKind {
  HOLE_TRIANGLE_KIND_FLAT,
  /* Not a triangle; see BuildRingMatrices */
  HOLE_TRIANGLE_KIND_RING,
  /* Blended texture tinted blue, mapped across x + z and y */
  HOLE_TRIANGLE_KIND_UPRIGHT,
  /* Untextured, more opaque the more its edge runs along z */
  HOLE_TRIANGLE_KIND_SHADED
};

typedef struct HoleTriangle {
  /* 0x00 */ u32 kind;
  /* 0x08 */ Vtx vertices[HOLE_TRIANGLE_VERTEX_COUNT];
  /* Each axis's lowest and highest corner coordinate */
  /* 0x38 */ s16 bounds[AXIS_COUNT][BOUND_END_COUNT];
  /* For a ring, the mean of its corners; nothing reads it */
  /* 0x44 */ f32 ignoredCenter[AXIS_COUNT];
  /* Cleared by UpdateRingEditor and read by nothing */
  /* 0x50 */ s32 unusedEditorValues[3];
} HoleTriangle; /* size = 0x60 */

void DimNextTranslucentTriangles(void);
void DrawHoleTriangles(
    Gfx** displayListHead, CameraMatrices* matrices, Camera* camera);
void DrawRings(Gfx** displayListHead, CameraMatrices* matrices, Camera* camera);
void InitRings(void);

#endif
