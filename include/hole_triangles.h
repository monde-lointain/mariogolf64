#ifndef HOLE_TRIANGLES_H
#define HOLE_TRIANGLES_H

#include <ultra64.h>

#include "camera.h"
#include "geometry.h"

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
    HOLE_TRIANGLE_KIND_SHADED
};

typedef s8 HoleTriangleKind;

typedef struct HoleTriangle {
    u32 kind;
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
