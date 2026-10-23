#ifndef HOLE_TRIANGLES_H
#define HOLE_TRIANGLES_H

#include <ultra64.h>
#include "camera.h"

/*
 * Single triangles that the hole editor places on a hole, apart from its
 * ground. Their kinds choose how they are drawn; the rings of Ring Shot are
 * one of the kinds, with the triangle's corners holding the ring's place.
 */

#define HOLE_TRIANGLE_COUNT 160
#define HOLE_TRIANGLE_VERTEX_COUNT 3
#define HOLE_TRIANGLE_OFFSET 0x5A520 /* In g_WorkArea.bytes */

/* HoleTriangle.kind's values */
#define HOLE_TRIANGLE_KIND_FLAT 0      /* Blended texture, mapped across x and y + z */
#define HOLE_TRIANGLE_KIND_RING 1      /* Not a triangle; see BuildRingMatrices */
#define HOLE_TRIANGLE_KIND_UPRIGHT 2   /* Blended texture tinted blue, mapped across x + z and y */
#define HOLE_TRIANGLE_KIND_SHADED 3    /* Untextured, more opaque the more its edge runs along z */

typedef struct HoleTriangle {
    /* 0x00 */ u32 kind;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ Vtx vertices[HOLE_TRIANGLE_VERTEX_COUNT];
    /* 0x38 */ s16 bounds[3][2]; /* Each axis's lowest and highest corner coordinate */
    /* 0x44 */ f32 center[3]; /* For a ring, the mean of its corners */
    /* 0x50 */ u8 unk50[0x60 - 0x50];
} HoleTriangle; /* size = 0x60 */

void DimNextTranslucentTriangles(void);
void DrawHoleTriangles(Gfx **displayListHead, CameraMatrices *matrices, Camera *camera);
void DrawRings(Gfx **displayListHead, CameraMatrices *matrices, Camera *camera);
void InitRings(void);

#endif
