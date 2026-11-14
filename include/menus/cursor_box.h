#ifndef CURSOR_BOX_H
#define CURSOR_BOX_H

#include <ultra64.h>

#include "system/asset.h"

void DrawCursorBox(Gfx **displayListHead, s32 left, s32 top);
void LoadCursorBox(ImageAsset *image);

#endif
