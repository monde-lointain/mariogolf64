#ifndef CURSOR_BOX_H
#define CURSOR_BOX_H

#include <ultra64.h>

/* The cursor box drawn around a point */

void DrawCursorBox(Gfx **displayListHead, s32 left, s32 top);
void LoadCursorBox(void *image);

#endif
