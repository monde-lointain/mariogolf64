#ifndef MENU_ARROWS_H
#define MENU_ARROWS_H

#include <ultra64.h>

#include "asset.h"

void DrawBlinkingSideArrows(
    Gfx **displayListHead,
    ImageAsset *image,
    s32 *blinkFrame,
    s32 *blinkTimer,
    s32 width,
    s32 centerX,
    s32 centerY
);
void DrawGreenArrow(
    Gfx **displayListHead,
    ImageAsset *image,
    s32 blinkFrame,
    MenuArrow direction,
    s32 centerX,
    s32 centerY
);
void DrawPageArrows(Gfx **displayListHead);
void DrawCenteredPageArrows(Gfx **displayListHead, s32 gap, s32 centerY);
void DrawUpDownArrows(Gfx **displayListHead, s32 centerY);

#endif
