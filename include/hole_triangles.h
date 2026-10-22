#ifndef HOLE_TRIANGLES_H
#define HOLE_TRIANGLES_H

#include <ultra64.h>
#include "camera.h"

void DimNextTranslucentTriangles(void);
void DrawHoleTriangles(Gfx **displayListHead, CameraMatrices *matrices, Camera *camera);
void DrawRings(Gfx **displayListHead, CameraMatrices *matrices, Camera *camera);
void InitRings(void);

#endif
