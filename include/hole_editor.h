#ifndef HOLE_EDITOR_H
#define HOLE_EDITOR_H

#include <ultra64.h>

#include "camera.h"
#include "terrain.h"

void CaptureFrame(void);
void CaptureMapImage(void);
void DrawHoleEditorGround(CameraMatrices *matrices, TerrainStyle terrainStyle);
void DrawTargetPole(CameraMatrices *matrices, Gfx **displayListHead);
void FillWaveTexels(void);
void UpdateCellEditor(void);
void UpdateGroundPointEditor(void);
void UpdateHeightEditor(void);
void UpdateHoleFileMenu(void);
void UpdateHolePointEditor(void);
void UpdateRingEditor(void);
void UpdateTreeEditor(void);
void UpdateTriangleEditor(void);
void UpdateWallEditor(void);

#endif
