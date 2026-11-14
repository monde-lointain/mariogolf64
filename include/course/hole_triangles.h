/*
 * Single triangles that the hole editor places on a hole, apart from its
 * ground. Their kinds choose how they are drawn; the rings of Ring Shot are
 * one of the kinds, with the triangle's corners holding the ring's place.
 */

#ifndef HOLE_TRIANGLES_H
#define HOLE_TRIANGLES_H

#include <ultra64.h>

#include "camera/camera.h"
#include "math/geometry.h"

#define HOLE_TRIANGLE_COUNT 160

// The ends of each axis's range in HoleTriangle.bounds
enum BoundEnd {
    BOUND_LOWEST,
    BOUND_HIGHEST
};

#define BOUND_END_COUNT 2

// HoleTriangle.kind's values
// Blended texture, mapped across x and y + z
enum HoleTriangleKind {
    HOLE_TRIANGLE_KIND_FLAT,
    // Not a triangle; see BuildRingMatrices
    HOLE_TRIANGLE_KIND_RING,
    // Blended texture tinted blue, mapped across x + z and y
    HOLE_TRIANGLE_KIND_UPRIGHT,
    // Untextured, more opaque the more its edge runs along z
    HOLE_TRIANGLE_KIND_SHADED,
    HOLE_TRIANGLE_KIND_COUNT
};

typedef u32 HoleTriangleKind;
typedef s8 HoleTriangleKind8;
// FindRingHit's use of a ring's corners
#define RING_CENTER 0
#define RING_FACING 1
// Only its x is used
#define RING_SCALE 2

typedef struct HoleTriangle {
    HoleTriangleKind kind;
    Vtx vertices[TRIANGLE_VERTEX_COUNT];
    // Each axis's lowest and highest corner coordinate
    s16 bounds[AXIS_COUNT][BOUND_END_COUNT];
    // For a ring, the mean of its corners; nothing reads it
    f32 ignoredCenter[AXIS_COUNT];
    // Cleared by UpdateRingEditor and read by nothing
    s32 unusedEditorValues[3];
} HoleTriangle;

void DimNextTranslucentTriangles(void);
void DrawHoleTriangles(Gfx **displayListHead, CameraMatrices *matrices, Camera *camera);
void DrawRings(Gfx **displayListHead, CameraMatrices *matrices, Camera *camera);
void InitRings(void);

#endif
