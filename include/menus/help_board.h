/*
 * The menus' help board and its topics.
 */

#ifndef HELP_BOARD_H
#define HELP_BOARD_H

#include <ultra64.h>

// LoadHelpBoardImage's images
enum HelpBoardImage {
    HELP_BOARD_IMAGE_HAND = 1,
    HELP_BOARD_IMAGE_ARROWS,
    HELP_BOARD_IMAGE_CHALKBOARD
};

typedef s32 HelpBoardImage;
void LoadHelpBoardImage(void *buffer, HelpBoardImage image);
void UpdateHelpBoard(void);
void DrawHelpBoard(Gfx **displayListHead);

#endif
