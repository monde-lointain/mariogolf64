#ifndef TITLE_SCREEN_H
#define TITLE_SCREEN_H

#include <ultra64.h>

/* The title screen */

void LoadTitleScreen(void);
void UnloadTitleScreen(void);
void UpdateTitleScreen(void);

void DrawTitleScreen(Gfx** displayListHead);

#endif
