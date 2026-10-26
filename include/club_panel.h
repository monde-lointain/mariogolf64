#ifndef CLUB_PANEL_H
#define CLUB_PANEL_H

#include <ultra64.h>

void UpdateClubPanelDebugValue(void);
void LoadClubPanelImages(void);
void FreeClubPanelImages(void);
void ResetShotModeChange(void);
void UpdateClubPanelImages(void);
s32 IsShotModeChangeDone(void);
void DrawClubPanel(Gfx **displayListHead);
void DrawClubPanelIfShown(Gfx **displayListHead);

#endif
