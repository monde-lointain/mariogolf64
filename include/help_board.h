#ifndef HELP_BOARD_H
#define HELP_BOARD_H

#include <ultra64.h>

void LoadHelpBoardImage(void* buffer, s32 image);
void UpdateHelpBoard(void);
void DrawHelpBoard(Gfx** displayListHead);

#endif
