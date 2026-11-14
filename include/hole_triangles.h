#ifndef HOLE_TRIANGLES_H
#define HOLE_TRIANGLES_H

#include <ultra64.h>

#include "camera.h"
#include "math/geometry.h"

#define HOLE_TRIANGLE_COUNT 160

enum BoundEnd {
    BOUND_LOWEST,
    BOUND_HIGHEST
};

#define BOUND_END_COUNT 2

enum HoleTriangleKind {
    HOLE_TRIANGLE_KIND_FLAT,
    HOLE_TRIANGLE_KIND_RING,
    HOLE_TRIANGLE_KIND_UPRIGHT,
    HOLE_TRIANGLE_KIND_SHADED,
    HOLE_TRIANGLE_KIND_COUNT
};

typedef u32 HoleTriangleKind;
typedef s8 HoleTriangleKind8;

#define RING_CENTER 0
#define RING_FACING 1
#define RING_SCALE 2

typedef struct HoleTriangle {
    HoleTriangleKind kind;
    Vtx vertices[TRIANGLE_VERTEX_COUNT];
    s16 bounds[AXIS_COUNT][BOUND_END_COUNT];
    f32 ignoredCenter[AXIS_COUNT];
    s32 unusedEditorValues[3];
} HoleTriangle;

void DimNextTranslucentTriangles(void);
void DrawHoleTriangles(Gfx **displayListHead, CameraMatrices *matrices, Camera *camera);
void DrawRings(Gfx **displayListHead, CameraMatrices *matrices, Camera *camera);
void InitRings(void);

#endif
