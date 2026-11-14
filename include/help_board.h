#ifndef HELP_BOARD_H
#define HELP_BOARD_H

#include <ultra64.h>

enum HelpBoardImage {
    HELP_BOARD_IMAGE_HAND = 1,
    HELP_BOARD_IMAGE_ARROWS,
    HELP_BOARD_IMAGE_CHALKBOARD
};

void LoadHelpBoardImage(void *buffer, s32 image);
void UpdateHelpBoard(void);
void DrawHelpBoard(Gfx **displayListHead);

#endif
