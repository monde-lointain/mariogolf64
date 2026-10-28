#ifndef HELP_BOARD_H
#define HELP_BOARD_H

#include <ultra64.h>

/* LoadHelpBoardImage's images */
#define HELP_BOARD_IMAGE_HAND 1
#define HELP_BOARD_IMAGE_ARROWS 2
#define HELP_BOARD_IMAGE_CHALKBOARD 3

void LoadHelpBoardImage(void* buffer, s32 image);
void UpdateHelpBoard(void);
void DrawHelpBoard(Gfx** displayListHead);

#endif
