#ifndef HOLE_DISPLAY_H
#define HOLE_DISPLAY_H

#include <ultra64.h>

/* SetMenuBackdrop's backdrops, and the styles that DrawGame draws them in */
#define MENU_BACKDROP_NONE (-1)
#define MENU_BACKDROP_UNK0 0
#define MENU_BACKDROP_UNK3 3
#define MENU_BACKDROP_STYLE_NONE 0
#define MENU_BACKDROP_STYLE_PLAIN 1
#define MENU_BACKDROP_STYLE_UNK2 2

extern s32 g_MenuBackdropStyle;

void SetUpTitleHole(void);
void UsePrimitiveDepth(s32 isPrimitiveDepthSet);
void SetMenuBackdrop(s32 backdrop);
void StartHudAnimation(s32 animation);
s32 IsHudAnimationDone(void);

#endif
