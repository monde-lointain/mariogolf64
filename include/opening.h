#ifndef OPENING_H
#define OPENING_H

#include <ultra64.h>

#include "camera/camera.h"
#include "system/c_language.h"

void LoadOpening(void);
void FreeOpening(void);
void UpdateOpening(void);

void DrawOpening(CameraMatrices *matrices, Gfx **displayListHead);
void DrawOpeningLogo(Gfx **displayListHead, Boolean isDrawn);

#endif
