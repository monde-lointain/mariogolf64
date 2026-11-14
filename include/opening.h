#ifndef OPENING_H
#define OPENING_H

#include <ultra64.h>

#include "c_language.h"
#include "camera.h"

void LoadOpening(void);
void FreeOpening(void);
void UpdateOpening(void);

void DrawOpening(CameraMatrices *matrices, Gfx **displayListHead);
void DrawOpeningLogo(Gfx **displayListHead, Boolean isDrawn);

#endif
