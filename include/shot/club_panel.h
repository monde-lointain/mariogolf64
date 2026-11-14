/*
 * The panel that shows the selected club and the shot mode.
 */

#ifndef CLUB_PANEL_H
#define CLUB_PANEL_H

#include <ultra64.h>

#include "system/asset.h"
#include "system/c_language.h"

// The panel image buffer that is shown
extern ImageAsset *g_ClubPanelImage;

void UpdateClubPanelDebugValue(void);
void LoadClubPanelImages(void);
void FreeClubPanelImages(void);
void ResetShotModeChange(void);
void UpdateClubPanelImages(void);
Boolean IsShotModeChangeDone(void);
void DrawClubPanel(Gfx **displayListHead);
void DrawClubPanelIfShown(Gfx **displayListHead);

#endif
