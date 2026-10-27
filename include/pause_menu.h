#ifndef PAUSE_MENU_H
#define PAUSE_MENU_H

#include <ultra64.h>

/* Where the panels of the lie and of the wind slide to beside the menu */
extern f32 g_LiePanelPosition[2];
extern f32 g_WindPanelPosition[2];

void DrawPauseArrows(Gfx** displayListHead, s32 x, s32 y, s32 gap);

#endif
