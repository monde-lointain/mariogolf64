#ifndef HOLE_CREATURES_H
#define HOLE_CREATURES_H

#include <ultra64.h>

#include "math_util.h"
#include "system/c_language.h"

void LoadHoleCreatureImages(void);
void FreeHoleCreatureImages(void);
Radians GetCameraYaw(void);
Boolean IsMonkeyShown(void);
void StartMonkeyDrop(Radians cameraYaw);
void ClearHoleCreatures(void);
void StartCheepCheepLeap(s32 startValue);
void DrawHoleCreatures(Gfx **displayListHead);

#endif
