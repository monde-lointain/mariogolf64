#ifndef HOLE_OBJECTS_H
#define HOLE_OBJECTS_H

#include <ultra64.h>

void LoadHoleCreatureImages(void);
void FreeHoleCreatureImages(void);
f32 GetCameraYaw(void);
Boolean IsMonkeyShown(void);
void StartMonkeyDrop(f32 cameraYaw);
void ClearHoleCreatures(void);
void StartCheepCheepLeap(s32 arg0);
void DrawHoleCreatures(Gfx** displayListHead);

#endif
