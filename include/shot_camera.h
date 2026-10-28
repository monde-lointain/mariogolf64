#ifndef SHOT_CAMERA_H
#define SHOT_CAMERA_H

#include <ultra64.h>

/* The frame the hole's fly-over has reached, or -1 once it ends */
extern s32 g_HoleFlyoverFrame;
/* While set, characters are drawn for every camera but the inset */
extern s32 D_800BE698;
/* Set while the camera shows the cup up close, at twice the distance from it */
extern s32 g_IsCupViewShown;

#endif
