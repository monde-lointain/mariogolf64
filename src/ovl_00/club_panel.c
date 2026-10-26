#include "common.h"
#include "asset.h"
#include "club.h"
#include "club_panel.h"
#include "controller.h"
#include "current_shot.h"
#include "debug_text.h"
#include "game_flags.h"
#include "heap.h"
#include "round_hud.h"
#include "scene.h"
#include "variables.h"

/*
 * The panel that shows the selected club and the shot mode. Switching the
 * shot mode animates the panel over a few frames, by loading one image per
 * frame into whichever of two buffers is not being shown.
 */

/* Shows the debug value, which the second controller's D-pad changes */
#define GAME_FLAG_CLUB_PANEL_DEBUG 0x6E
#define DEBUG_CONTROLLER 1
#define DEBUG_VALUE_COLUMN 20
#define DEBUG_VALUE_ROW 5

/* sClubPanelState values */
#define CLUB_PANEL_FREED -1
#define CLUB_PANEL_LOADED 1

/* The frames of a shot mode change; the last frame shows the new mode */
#define SHOT_MODE_CHANGE_FRAME_COUNT 4
#define SHOT_MODE_CHANGE_LAST_FRAME (SHOT_MODE_CHANGE_FRAME_COUNT - 1)

/* Where the two panel buffers are among D_801B5CD0's images */
#define CLUB_PANEL_BUFFER_SLOT 12

#define CLUB_PANEL_IMAGE_ASSET 0x4FF
#define CLUB_PANEL_FRAME_ASSET 0x56C
#define SHOT_MODE_IMAGE_SIZE 0x808

/* The panel's images, in each shot mode */
#define PANEL_ASSET_POWER 0x56C
#define PANEL_ASSET_POWER_UNK3 0x56D /* In GAME_MODE_TRAINING */
#define PANEL_ASSET_NORMAL 0x56E
#define PANEL_ASSET_APPROACH 0x56F
#define PANEL_ASSET_PUTT_LONG 0x570
#define PANEL_ASSET_PUTT_MIDDLE 0x571
#define PANEL_ASSET_PUTT_SHORT 0x572

/* The shot mode's name */
#define SHOT_MODE_ASSET_POWER 0x52F
#define SHOT_MODE_ASSET_NORMAL 0x52D
#define SHOT_MODE_ASSET_APPROACH 0x52E

/* Image buffers shared with the menus */
extern u8 *D_801B5CD0[];
/* The panel image buffer that is shown */
extern u8 *D_801321FC;

/* The shot mode last shown in the name, and the one that the panel last finished changing to */
extern u8 sShownShotMode;
extern u8 sPanelShotMode;
extern u8 sPanelIsPutter;
/* The frame of the shot mode change, or 0 while it is not changing */
extern u8 sShotModeChangeFrame;
extern s32 sClubPanelUnk24;
/* Which of the two panel buffers is loaded next */
extern s32 sPanelBufferIndex;
extern s32 sClubPanelState;
extern s32 sClubPanelDebugValue;
/* The frames of each change of shot mode, by the mode and the one changed from */
extern u16 sApproachToNormalFrames[SHOT_MODE_CHANGE_FRAME_COUNT];
extern u16 sPowerToNormalFrames[SHOT_MODE_CHANGE_FRAME_COUNT];
extern u16 sNormalToApproachFrames[SHOT_MODE_CHANGE_FRAME_COUNT];
extern u16 sPowerToApproachFrames[SHOT_MODE_CHANGE_FRAME_COUNT];
extern u16 sToPowerFrames[SHOT_MODE_CHANGE_FRAME_COUNT];
extern u8 *sClubPanelImage;
extern u8 *sShotModeImage;

void UpdateClubPanelDebugValue(void) {
    if (IsGameFlagSet(GAME_FLAG_CLUB_PANEL_DEBUG)) {
        if (g_ControllerStates[DEBUG_CONTROLLER].pressedButtons & U_JPAD) {
            sClubPanelDebugValue--;
        }
        if (g_ControllerStates[DEBUG_CONTROLLER].pressedButtons & D_JPAD) {
            sClubPanelDebugValue++;
        }
        sprintf(g_PrintBuffer, "%3d", sClubPanelDebugValue);
        PrintDebugText(g_PrintBuffer, DEBUG_VALUE_COLUMN, DEBUG_VALUE_ROW);
    }
}

void LoadClubPanelImages(void) {
    EncodedAsset asset;

    sClubPanelImage = AllocMemory(OpenEncodedAsset(CLUB_PANEL_IMAGE_ASSET, &asset));
    ReadEncodedAsset(sClubPanelImage, &asset);
    D_801B5CD0[CLUB_PANEL_BUFFER_SLOT] = AllocMemory(OpenEncodedAsset(CLUB_PANEL_FRAME_ASSET, &asset));
    ReadEncodedAsset(D_801B5CD0[CLUB_PANEL_BUFFER_SLOT], &asset);
    D_801B5CD0[CLUB_PANEL_BUFFER_SLOT + 1] = AllocMemory(OpenEncodedAsset(CLUB_PANEL_FRAME_ASSET, &asset));
    ReadEncodedAsset(D_801B5CD0[CLUB_PANEL_BUFFER_SLOT + 1], &asset);
    sShotModeImage = AllocMemory(SHOT_MODE_IMAGE_SIZE);
    sShownShotMode = 0xFF;
    sPanelShotMode = 0xFF;
    sPanelIsPutter = 0xFF;
    sClubPanelState = CLUB_PANEL_LOADED;
}

void FreeClubPanelImages(void) {
    sClubPanelState = CLUB_PANEL_FREED;
    FreeMemory(&sClubPanelImage);
    FreeMemory(&sShotModeImage);
    FreeMemory(&D_801B5CD0[CLUB_PANEL_BUFFER_SLOT]);
    FreeMemory(&D_801B5CD0[CLUB_PANEL_BUFFER_SLOT + 1]);
}

void ResetShotModeChange(void) {
    sClubPanelUnk24 = 0;
    sShotModeChangeFrame = 0;
}

void UpdateClubPanelImages(void) {
    EncodedAsset panelAsset;
    EncodedAsset nameAsset;
    EncodedAsset frameAsset;
    s32 isPutter;
    u16 assetId;

    isPutter = GetSelectedClub()->surfaceMask & CLUB_SURFACE_GREEN;
    if (sClubPanelState == CLUB_PANEL_FREED) {
        return;
    }
    if (sPanelIsPutter != isPutter) {
        sPanelIsPutter = isPutter;
        sPanelBufferIndex ^= 1;
        if (isPutter) {
            switch ((u32)g_CurrentShot.shotMode) {
                case PUTT_MODE_MIDDLE:
                    OpenEncodedAsset(PANEL_ASSET_PUTT_MIDDLE, &panelAsset);
                    break;
                case PUTT_MODE_LONG:
                    OpenEncodedAsset(PANEL_ASSET_PUTT_LONG, &panelAsset);
                    break;
                case PUTT_MODE_SHORT:
                    OpenEncodedAsset(PANEL_ASSET_PUTT_SHORT, &panelAsset);
                    break;
            }
        } else {
            switch ((u32)g_CurrentShot.shotMode) {
                case SHOT_MODE_NORMAL:
                    OpenEncodedAsset(PANEL_ASSET_NORMAL, &panelAsset);
                    break;
                case SHOT_MODE_POWER:
                    if (g_GameMode == GAME_MODE_TRAINING) {
                        OpenEncodedAsset(PANEL_ASSET_POWER_UNK3, &panelAsset);
                    } else {
                        OpenEncodedAsset(PANEL_ASSET_POWER, &panelAsset);
                    }
                    break;
                case SHOT_MODE_APPROACH:
                    OpenEncodedAsset(PANEL_ASSET_APPROACH, &panelAsset);
                    break;
            }
        }
        ReadEncodedAsset(D_801B5CD0[CLUB_PANEL_BUFFER_SLOT + sPanelBufferIndex], &panelAsset);
        D_801321FC = D_801B5CD0[CLUB_PANEL_BUFFER_SLOT + sPanelBufferIndex];
    }
    if (sShownShotMode != g_CurrentShot.shotMode) {
        sShownShotMode = g_CurrentShot.shotMode;
        if (g_CurrentShot.shotMode != SHOT_MODE_POWER) {
            if (g_CurrentShot.shotMode == SHOT_MODE_NORMAL) {
                assetId = SHOT_MODE_ASSET_NORMAL;
            } else {
                assetId = SHOT_MODE_ASSET_APPROACH;
            }
        } else {
            assetId = SHOT_MODE_ASSET_POWER;
        }

        OpenEncodedAsset(assetId, &nameAsset);
        ReadEncodedAsset(sShotModeImage, &nameAsset);
    }
    if (sPanelShotMode != g_CurrentShot.shotMode) {
        sPanelBufferIndex ^= 1;
        if (sShotModeChangeFrame == SHOT_MODE_CHANGE_LAST_FRAME) {
            if (isPutter) {
                switch ((u32)g_CurrentShot.shotMode) {
                    case PUTT_MODE_MIDDLE:
                        OpenEncodedAsset(PANEL_ASSET_PUTT_MIDDLE, &frameAsset);
                        break;
                    case PUTT_MODE_LONG:
                        OpenEncodedAsset(PANEL_ASSET_PUTT_LONG, &frameAsset);
                        break;
                    case PUTT_MODE_SHORT:
                        OpenEncodedAsset(PANEL_ASSET_PUTT_SHORT, &frameAsset);
                        break;
                }
            } else {
                switch ((u32)g_CurrentShot.shotMode) {
                    case SHOT_MODE_NORMAL:
                        OpenEncodedAsset(PANEL_ASSET_NORMAL, &frameAsset);
                        break;
                    case SHOT_MODE_POWER:
                        if (g_GameMode == GAME_MODE_TRAINING) {
                            OpenEncodedAsset(PANEL_ASSET_POWER_UNK3, &frameAsset);
                        } else {
                            OpenEncodedAsset(PANEL_ASSET_POWER, &frameAsset);
                        }
                        break;
                    case SHOT_MODE_APPROACH:
                        OpenEncodedAsset(PANEL_ASSET_APPROACH, &frameAsset);
                        break;
                }
            }
        } else {
            switch (g_CurrentShot.shotMode) {
                case SHOT_MODE_NORMAL:
                    if (sPanelShotMode == SHOT_MODE_POWER) {
                        assetId = sPowerToNormalFrames[sShotModeChangeFrame];
                    } else {
                        assetId = sApproachToNormalFrames[sShotModeChangeFrame];
                    }
                    break;
                case SHOT_MODE_POWER:
                    assetId = sToPowerFrames[sShotModeChangeFrame];
                    break;
                default:
                    if (sPanelShotMode == SHOT_MODE_POWER) {
                        assetId = sPowerToApproachFrames[sShotModeChangeFrame];
                    } else {
                        assetId = sNormalToApproachFrames[sShotModeChangeFrame];
                    }
                    break;
            }
            OpenEncodedAsset(assetId, &frameAsset);
        }
        ReadEncodedAsset(D_801B5CD0[CLUB_PANEL_BUFFER_SLOT + sPanelBufferIndex], &frameAsset);
        sShotModeChangeFrame++;
        D_801321FC = D_801B5CD0[CLUB_PANEL_BUFFER_SLOT + sPanelBufferIndex];
        if (sShotModeChangeFrame == SHOT_MODE_CHANGE_FRAME_COUNT) {
            sShotModeChangeFrame = 0;
            sPanelShotMode = g_CurrentShot.shotMode;
        }
    }
}

s32 IsShotModeChangeDone(void) {
    return sShotModeChangeFrame == 0;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/club_panel", DrawClubPanel);

void DrawClubPanelIfShown(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    if (g_RoundHud.unkC != 0) {
        UpdateClubPanelDebugValue();
        DrawClubPanel(&gfx);
        *displayListHead = gfx;
    }
}
