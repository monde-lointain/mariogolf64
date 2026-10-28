/*
 * The panel that shows the selected club and the shot mode. Switching the
 * shot mode animates the panel over a few frames, by loading one image per
 * frame into whichever of two buffers is not being shown.
 */
#include "common.h"

#include "club_panel.h"

#include "asset.h"
#include "club.h"
#include "controller.h"
#include "current_shot.h"
#include "debug_text.h"
#include "game_flags.h"
#include "heap.h"
#include "round_hud.h"
#include "scene.h"
#include "variables.h"

/* Shows the debug value, which the second controller's D-pad changes */
#define GAME_FLAG_CLUB_PANEL_DEBUG 0x6E
#define DEBUG_CONTROLLER 1
#define DEBUG_VALUE_COLUMN 20
#define DEBUG_VALUE_ROW 5

/* f_ClubPanelState values */
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
extern u8* D_801B5CD0[];

/* The shot mode last shown in the name, and the one that the panel last
 * finished changing to */
extern u8 f_ShownShotMode;
extern u8 f_PanelShotMode;
extern u8 f_PanelIsPutter;
/* The frame of the shot mode change, or 0 while it is not changing */
extern u8 f_ShotModeChangeFrame;
extern s32 f_ClubPanelUnk24;
/* Which of the two panel buffers is loaded next */
extern s32 f_PanelBufferIndex;
extern s32 f_ClubPanelState;
extern s32 f_ClubPanelDebugValue;
/* The frames of each change of shot mode, by the mode and the one changed from
 */
extern u16 f_ApproachToNormalFrames[SHOT_MODE_CHANGE_FRAME_COUNT];
extern u16 f_PowerToNormalFrames[SHOT_MODE_CHANGE_FRAME_COUNT];
extern u16 f_NormalToApproachFrames[SHOT_MODE_CHANGE_FRAME_COUNT];
extern u16 f_PowerToApproachFrames[SHOT_MODE_CHANGE_FRAME_COUNT];
extern u16 f_ToPowerFrames[SHOT_MODE_CHANGE_FRAME_COUNT];
extern u8* f_ClubPanelImage;
extern u8* f_ShotModeImage;

void UpdateClubPanelDebugValue(void) {
  if (IsGameFlagSet(GAME_FLAG_CLUB_PANEL_DEBUG)) {
    if (g_ControllerStates[DEBUG_CONTROLLER].pressedButtons & U_JPAD) {
      f_ClubPanelDebugValue--;
    }

    if (g_ControllerStates[DEBUG_CONTROLLER].pressedButtons & D_JPAD) {
      f_ClubPanelDebugValue++;
    }

    sprintf(g_PrintBuffer, "%3d", f_ClubPanelDebugValue);
    PrintDebugText(g_PrintBuffer, DEBUG_VALUE_COLUMN, DEBUG_VALUE_ROW);
  }
}

void LoadClubPanelImages(void) {
  EncodedAsset asset;

  f_ClubPanelImage =
      AllocMemory(OpenEncodedAsset(CLUB_PANEL_IMAGE_ASSET, &asset));
  ReadEncodedAsset(f_ClubPanelImage, &asset);
  D_801B5CD0[CLUB_PANEL_BUFFER_SLOT] =
      AllocMemory(OpenEncodedAsset(CLUB_PANEL_FRAME_ASSET, &asset));
  ReadEncodedAsset(D_801B5CD0[CLUB_PANEL_BUFFER_SLOT], &asset);
  D_801B5CD0[CLUB_PANEL_BUFFER_SLOT + 1] =
      AllocMemory(OpenEncodedAsset(CLUB_PANEL_FRAME_ASSET, &asset));
  ReadEncodedAsset(D_801B5CD0[CLUB_PANEL_BUFFER_SLOT + 1], &asset);
  f_ShotModeImage = AllocMemory(SHOT_MODE_IMAGE_SIZE);
  f_ShownShotMode = 0xFF;
  f_PanelShotMode = 0xFF;
  f_PanelIsPutter = 0xFF;
  f_ClubPanelState = CLUB_PANEL_LOADED;
}

void FreeClubPanelImages(void) {
  f_ClubPanelState = CLUB_PANEL_FREED;
  FreeMemory(&f_ClubPanelImage);
  FreeMemory(&f_ShotModeImage);
  FreeMemory(&D_801B5CD0[CLUB_PANEL_BUFFER_SLOT]);
  FreeMemory(&D_801B5CD0[CLUB_PANEL_BUFFER_SLOT + 1]);
}

void ResetShotModeChange(void) {
  f_ClubPanelUnk24 = 0;
  f_ShotModeChangeFrame = 0;
}

void UpdateClubPanelImages(void) {
  EncodedAsset panelAsset;
  EncodedAsset nameAsset;
  EncodedAsset frameAsset;
  s32 isPutter;
  u16 assetId;

  isPutter = GetSelectedClub()->surfaceMask & CLUB_SURFACE_GREEN;
  if (f_ClubPanelState == CLUB_PANEL_FREED) {
    return;
  }

  if (f_PanelIsPutter != isPutter) {
    f_PanelIsPutter = isPutter;
    f_PanelBufferIndex ^= 1;
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
          if (g_GameState.gameMode == GAME_MODE_TRAINING) {
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

    ReadEncodedAsset(
        D_801B5CD0[CLUB_PANEL_BUFFER_SLOT + f_PanelBufferIndex], &panelAsset);
    g_ClubPanelImage = D_801B5CD0[CLUB_PANEL_BUFFER_SLOT + f_PanelBufferIndex];
  }

  if (f_ShownShotMode != g_CurrentShot.shotMode) {
    f_ShownShotMode = g_CurrentShot.shotMode;
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
    ReadEncodedAsset(f_ShotModeImage, &nameAsset);
  }

  if (f_PanelShotMode != g_CurrentShot.shotMode) {
    f_PanelBufferIndex ^= 1;
    if (f_ShotModeChangeFrame == SHOT_MODE_CHANGE_LAST_FRAME) {
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
            if (g_GameState.gameMode == GAME_MODE_TRAINING) {
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
          if (f_PanelShotMode == SHOT_MODE_POWER) {
            assetId = f_PowerToNormalFrames[f_ShotModeChangeFrame];
          } else {
            assetId = f_ApproachToNormalFrames[f_ShotModeChangeFrame];
          }

          break;
        case SHOT_MODE_POWER:
          assetId = f_ToPowerFrames[f_ShotModeChangeFrame];
          break;
        default:
          if (f_PanelShotMode == SHOT_MODE_POWER) {
            assetId = f_PowerToApproachFrames[f_ShotModeChangeFrame];
          } else {
            assetId = f_NormalToApproachFrames[f_ShotModeChangeFrame];
          }

          break;
      }

      OpenEncodedAsset(assetId, &frameAsset);
    }

    ReadEncodedAsset(
        D_801B5CD0[CLUB_PANEL_BUFFER_SLOT + f_PanelBufferIndex], &frameAsset);
    f_ShotModeChangeFrame++;
    g_ClubPanelImage = D_801B5CD0[CLUB_PANEL_BUFFER_SLOT + f_PanelBufferIndex];
    if (f_ShotModeChangeFrame == SHOT_MODE_CHANGE_FRAME_COUNT) {
      f_ShotModeChangeFrame = 0;
      f_PanelShotMode = g_CurrentShot.shotMode;
    }
  }
}

s32 IsShotModeChangeDone(void) {
  return f_ShotModeChangeFrame == 0;
}

#ifdef NON_MATCHING
/*
 * The power gauge runs from GAUGE_EMPTY_X leftwards, so a fuller gauge
 * reaches further left. Its ticks are GAUGE_TICK_SPACING apart, with
 * medium ticks every GAUGE_TICKS_PER_GROUP ticks and long ones at each end.
 */
#define GAUGE_FULL_X 56
#define GAUGE_LENGTH 140
#define GAUGE_EMPTY_X (GAUGE_FULL_X + GAUGE_LENGTH)
#define GAUGE_MAX_VALUE 30
#define GAUGE_TOP 210
#define GAUGE_BOTTOM 218
#define GAUGE_TICK_SPACING 7
#define GAUGE_TICK_WIDTH 4
#define GAUGE_GROUP_COUNT 5
#define GAUGE_TICKS_PER_GROUP 5
#define GAUGE_EXTRA_TICK_COUNT 2
#define GAUGE_END_TICK_COUNT 2
#define GAUGE_MEDIUM_TICK_COUNT 3
#define GAUGE_MEDIUM_TICK_TOP 212
#define GAUGE_MEDIUM_TICK_BOTTOM 216
/* The markers and the long ticks reach past the gauge */
#define GAUGE_MARKER_TOP 208
#define GAUGE_MARKER_BOTTOM 220
#define GAUGE_MARKER_WIDTH 4
/* The fill's gradient image */
#define GAUGE_FILL_IMAGE_WIDTH 64
#define GAUGE_FILL_IMAGE_HEIGHT 16
#define GAUGE_TICK_IMAGE_WIDTH 16
#define GAUGE_TICK_IMAGE_HEIGHT 12

/* g_Players[].unkC values that hide the gauge's markers */
#define PLAYER_STATE_UNK2 2
#define PLAYER_STATE_UNK4 4
#define PLAYER_STATE_UNK5 5

/* The bar that shows the lie's span below the gauge */
#define SPAN_BAR_X 38.0f
#define SPAN_BAR_Y -103.0f
#define SPAN_BAR_Z -100.0f
#define SPAN_BAR_SCALE_PER_SPAN 0.032857146f
#define SPAN_BAR_THICKNESS 0.1f
#define SPAN_BAR_VERTEX_COUNT 8

/* The markers pulse at a twelfth of a turn per frame */
#define MARKER_PULSE_STEP (M_PI / 6)
#define MARKER_PULSE_LEVEL 100.0

extern Vtx f_SpanBarVertices[SPAN_BAR_VERTEX_COUNT];
extern Gfx f_SpanBarDisplayList[];
extern f32 f_MarkerPulseAngle;
extern Mtx f_SpanBarTranslation;
extern Mtx f_SpanBarScale;
extern s32 f_ShotPanelOffsetX;

s32 func_8005F224(void);

/* Draws the power gauge, its markers, and the bar for the lie's span */
void DrawClubPanel(Gfx** displayListHead) {
  s32 span;
  Gfx* gfx;
  s32 isPutter;
  s32 value;
  s32 playerState;
  s32 group;
  s32 tick;
  s32 firstTick;
  s32 x;
  s32 fillWidth;

  s32 markerOffset;
  s32 pulse;
  s32 alpha;
  s32 absValue;

  gfx = *displayListHead;
  isPutter = GetSelectedClub()->surfaceMask & CLUB_SURFACE_GREEN;
  playerState = g_Players[g_CurrentShot.playerIndex].unkC;
  GetLieSpan(
      GetClub(
          g_Players[g_CurrentShot.playerIndex].clubs[g_CurrentShot.clubSlot]),
      g_CurrentShot.surface, &span, g_CurrentShot.shotMode);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gDPPipeSync(gfx++);
  gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 191);
  gDPPipeSync(gfx++);
  if (g_RoundHud.flags & HUD_FLAG_TRANSLUCENT_GAUGE) {
    alpha = 127;
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  } else {
    alpha = 255;
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  }

  switch (g_CurrentShot.shotMode) {
    case SHOT_MODE_NORMAL:
      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 32, alpha);
      break;
    case SHOT_MODE_POWER:
      gDPSetPrimColor(gfx++, 0, 0, 255, 32, 32, alpha);
      break;
    default:
      gDPSetPrimColor(gfx++, 0, 0, 64, 127, 255, alpha);
      break;
  }

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPSetCombineLERP(gfx++, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, PRIMITIVE,
      PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, PRIMITIVE);
  gDPLoadTextureBlock_4b(gfx++,
      ((u32)f_ClubPanelImage + IMAGE_HEADER_SIZE) & ~7, G_IM_FMT_I,
      GAUGE_TICK_IMAGE_WIDTH, GAUGE_TICK_IMAGE_HEIGHT, 0, G_TX_WRAP, G_TX_WRAP,
      2, 0, 0, 0);
  for (group = 0, firstTick = 0; group != GAUGE_GROUP_COUNT;
      group++, firstTick += GAUGE_TICKS_PER_GROUP) {
    for (tick = 1; tick != GAUGE_TICKS_PER_GROUP; tick++) {
      x = (firstTick + tick) * GAUGE_TICK_SPACING;
      gSPTextureRectangle(gfx++, (GAUGE_FULL_X + x) << 2,
          (GAUGE_TOP + f_ClubPanelDebugValue + f_ShotPanelOffsetX) << 2,
          (GAUGE_FULL_X + GAUGE_TICK_WIDTH + x) << 2,
          (GAUGE_BOTTOM + f_ClubPanelDebugValue + f_ShotPanelOffsetX) << 2,
          G_TX_RENDERTILE, 0, 0, 0x400, 0x600);
    }
  }

  for (tick = 0; tick != GAUGE_EXTRA_TICK_COUNT; tick++) {
    x = (firstTick + tick) * GAUGE_TICK_SPACING;
    gSPTextureRectangle(gfx++, (GAUGE_FULL_X + x) << 2,
        (GAUGE_TOP + f_ClubPanelDebugValue + f_ShotPanelOffsetX) << 2,
        (GAUGE_FULL_X + GAUGE_TICK_WIDTH + x) << 2,
        (GAUGE_BOTTOM + f_ClubPanelDebugValue + f_ShotPanelOffsetX) << 2,
        G_TX_RENDERTILE, 0, 0, 0x400, 0x600);
  }

  for (tick = 0; tick < GAUGE_END_TICK_COUNT; tick++) {
    gSPTextureRectangle(gfx++, (GAUGE_FULL_X + tick * GAUGE_LENGTH) << 2,
        (GAUGE_MARKER_TOP + f_ClubPanelDebugValue + f_ShotPanelOffsetX) << 2,
        (GAUGE_FULL_X + tick * GAUGE_LENGTH + GAUGE_TICK_WIDTH) << 2,
        (GAUGE_MARKER_BOTTOM + f_ClubPanelDebugValue + f_ShotPanelOffsetX) << 2,
        G_TX_RENDERTILE, 0, 0, 0x400, 0x400);
  }

  for (tick = 1; tick <= GAUGE_MEDIUM_TICK_COUNT; tick++) {
    gSPTextureRectangle(gfx++,
        (GAUGE_FULL_X + tick * GAUGE_TICKS_PER_GROUP * GAUGE_TICK_SPACING) << 2,
        (GAUGE_MEDIUM_TICK_TOP + f_ClubPanelDebugValue + f_ShotPanelOffsetX)
            << 2,
        (GAUGE_FULL_X + tick * GAUGE_TICKS_PER_GROUP * GAUGE_TICK_SPACING +
            GAUGE_TICK_WIDTH)
            << 2,
        (GAUGE_MEDIUM_TICK_BOTTOM + f_ClubPanelDebugValue + f_ShotPanelOffsetX)
            << 2,
        G_TX_RENDERTILE, 0, 0, 0x400, 0xC00);
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPSetPrimColor(gfx++, 0, 0, 255, 255, 0, 255);
  gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gDPPipeSync(gfx++);
  fillWidth = GAUGE_FILL_IMAGE_WIDTH;
  value = func_8005F224();
  gDPLoadTextureBlock(gfx++, ((u32)f_ShotModeImage + IMAGE_HEADER_SIZE) & ~7,
      G_IM_FMT_RGBA, G_IM_SIZ_16b, fillWidth, GAUGE_FILL_IMAGE_HEIGHT, 0,
      G_TX_WRAP, G_TX_WRAP, 0, 0, 0, 0);
  if (playerState != PLAYER_STATE_UNK2 && playerState == PLAYER_STATE_UNK4) {
    f_ClubPanelUnk24 = value;
  }

  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  if (playerState != PLAYER_STATE_UNK2) {
    gSPTextureRectangle(gfx++,
        (GAUGE_EMPTY_X - f_ClubPanelUnk24 * GAUGE_LENGTH / GAUGE_MAX_VALUE)
            << 2,
        (GAUGE_TOP + f_ClubPanelDebugValue + f_ShotPanelOffsetX) << 2,
        (GAUGE_EMPTY_X + GAUGE_MARKER_WIDTH) << 2,
        (GAUGE_BOTTOM + f_ClubPanelDebugValue + f_ShotPanelOffsetX) << 2,
        G_TX_RENDERTILE,
        ((((GAUGE_MAX_VALUE * 5) - (f_ClubPanelUnk24 * 5)) << 6) /
            (GAUGE_MAX_VALUE * 5))
            << 5,
        0, 0x1B4, 0x800);
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPLoadTextureBlock_4b(gfx++,
      ((u32)f_ClubPanelImage + IMAGE_HEADER_SIZE) & ~7, G_IM_FMT_I,
      GAUGE_TICK_IMAGE_WIDTH, GAUGE_TICK_IMAGE_HEIGHT, 0, G_TX_WRAP, G_TX_WRAP,
      2, 0, 0, 0);
  gDPSetCombineLERP(gfx++, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, PRIMITIVE,
      PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, PRIMITIVE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gDPSetPrimColor(gfx++, 0, 0, 191, 191, 191, 255);
  gDPPipeSync(gfx++);
  gSPTextureRectangle(gfx++, GAUGE_EMPTY_X << 2,
      (GAUGE_MARKER_TOP + f_ClubPanelDebugValue + f_ShotPanelOffsetX) << 2,
      (GAUGE_EMPTY_X + GAUGE_MARKER_WIDTH) << 2,
      (GAUGE_MARKER_BOTTOM + f_ClubPanelDebugValue + f_ShotPanelOffsetX) << 2,
      G_TX_RENDERTILE, 0, 0, 0x400, 0x400);
  if (isPutter) {
    markerOffset = f_ClubPanelUnk24 * GAUGE_LENGTH / GAUGE_MAX_VALUE;
  } else {
    markerOffset = value * GAUGE_LENGTH / GAUGE_MAX_VALUE;
  }

  f_MarkerPulseAngle += MARKER_PULSE_STEP;
  pulse = (sinf(f_MarkerPulseAngle) * MARKER_PULSE_LEVEL) + MARKER_PULSE_LEVEL;
  gDPPipeSync(gfx++);
  gDPSetPrimColor(gfx++, 0, 0, 191, 191, 191, 255);
  if (playerState == PLAYER_STATE_UNK2 || playerState == PLAYER_STATE_UNK4 ||
      playerState == PLAYER_STATE_UNK5 || isPutter) {
    gDPSetEnvColor(gfx++, 0, 0, 0, 255);
  } else if (value == 0) {
    gDPSetEnvColor(gfx++, pulse, pulse, pulse, 255);
  } else {
    absValue = value;
    if (absValue < 0) {
      absValue = -absValue;
    }

    if (span / 2 < absValue) {
      gDPSetPrimColor(gfx++, 0, 0, 127, 127, 127, 255);
      gDPSetEnvColor(gfx++, 0, 0, 0, 255);
    } else {
      gDPSetEnvColor(gfx++, 0, 0, 0, 255);
    }
  }

  gDPPipeSync(gfx++);
  gDPSetCombineLERP(gfx++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0,
      0, 0, 1, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, 0, 1);
  gSPTextureRectangle(gfx++, (GAUGE_EMPTY_X - markerOffset) << 2,
      (GAUGE_MARKER_TOP + f_ClubPanelDebugValue + f_ShotPanelOffsetX) << 2,
      (GAUGE_EMPTY_X + GAUGE_MARKER_WIDTH - markerOffset) << 2,
      (GAUGE_MARKER_BOTTOM + f_ClubPanelDebugValue + f_ShotPanelOffsetX) << 2,
      G_TX_RENDERTILE, 0, 0, 0x400, 0x400);
  gDPPipeSync(gfx++);
  if (playerState != PLAYER_STATE_UNK2 && playerState != PLAYER_STATE_UNK4 &&
      !isPutter) {
    gDPSetPrimColor(gfx++, 0, 0, 191, 191, 191, 255);
    markerOffset = f_ClubPanelUnk24 * GAUGE_LENGTH / GAUGE_MAX_VALUE;
    if (f_ClubPanelUnk24 == GAUGE_MAX_VALUE) {
      gDPSetEnvColor(gfx++, pulse, pulse, pulse, 255);
    } else {
      gDPSetEnvColor(gfx++, 0, 0, 0, 255);
    }

    gDPPipeSync(gfx++);
    gSPTextureRectangle(gfx++, (GAUGE_EMPTY_X - markerOffset) << 2,
        (GAUGE_MARKER_TOP + f_ClubPanelDebugValue + f_ShotPanelOffsetX) << 2,
        (GAUGE_EMPTY_X + GAUGE_MARKER_WIDTH - markerOffset) << 2,
        (GAUGE_MARKER_BOTTOM + f_ClubPanelDebugValue + f_ShotPanelOffsetX) << 2,
        G_TX_RENDERTILE, 0, 0, 0x400, 0x400);
    gDPPipeSync(gfx++);
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  if (span != 0) {
    guTranslate(&f_SpanBarTranslation, SPAN_BAR_X,
        SPAN_BAR_Y - f_ShotPanelOffsetX - f_ClubPanelDebugValue, SPAN_BAR_Z);
    guScale(&f_SpanBarScale, span * SPAN_BAR_SCALE_PER_SPAN, SPAN_BAR_THICKNESS,
        SPAN_BAR_THICKNESS);
    gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_SpanBarTranslation),
        G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_SpanBarScale),
        G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
    gSPVertex(gfx++, f_SpanBarVertices, SPAN_BAR_VERTEX_COUNT, 0);
    gSPDisplayList(gfx++, f_SpanBarDisplayList);
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/club_panel", DrawClubPanel);
#endif

void DrawClubPanelIfShown(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

  if (g_RoundHud.unkC != 0) {
    UpdateClubPanelDebugValue();
    DrawClubPanel(&gfx);
    *displayListHead = gfx;
  }
}
