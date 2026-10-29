#ifndef HOLE_OBJECTS_H
#define HOLE_OBJECTS_H

#include <ultra64.h>

#include "math_util.h"

void LoadHoleCreatureImages(void);
void FreeHoleCreatureImages(void);
Radians GetCameraYaw(void);
Boolean IsMonkeyShown(void);
void StartMonkeyDrop(Radians cameraYaw);
void ClearHoleCreatures(void);
void StartCheepCheepLeap(s32 arg0);
void DrawHoleCreatures(Gfx** displayListHead);

#endif
