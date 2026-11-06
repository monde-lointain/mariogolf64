#ifndef MENU_ARROWS_H
#define MENU_ARROWS_H

#include <ultra64.h>

/* The arrows that the menu screens draw beside their choices */

void DrawBlinkingSideArrows(Gfx** displayListHead, void* image, s32* blinkFrame,
    s32* blinkTimer, s32 width, s32 centerX, s32 centerY);
void DrawGreenArrow(Gfx** displayListHead, void* image, s32 blinkFrame,
    s32 direction, s32 centerX, s32 centerY);
void DrawPageArrows(Gfx** displayListHead);
void DrawCenteredPageArrows(Gfx** displayListHead, s32 gap, s32 centerY);
void DrawUpDownArrows(Gfx** displayListHead, s32 centerY);

#endif
