/*
 * Loads and unloads the overlays that hold the game's modules.
 */
#include "overlay_manager.h"

#include <lib/libnusys/nusys.h>
#include <ultra64.h>

#include "boot_logos.h"
#include "camera_editor.h"
#include "compiler.h"
#include "game_flags.h"
#include "golf_balls.h"
#include "language.h"
#include "main.h"
#include "menu.h"
#include "opening.h"
#include "round_hud.h"
#include "segment_symbols.h"
#include "staff_roll.h"
#include "title_screen.h"
#include "transmit_menu.h"

/* A frame long enough ago that no module conflicts with an overlay */
#define RDP_REQUEST_FRAME_NEVER 0x80000000

/* Modules whose display lists the RDP may still be reading are in use for
   this many frames after they make a request */
#define RDP_REQUEST_FRAMES_IN_USE 2

/* Size of the RDRAM that every console has, without the Expansion Pak */
#define BASE_RDRAM_SIZE 0x400000

#define OVERLAY_ENTRY(name, moduleIds, onLoad, onUnload) \
  {FALSE, moduleIds, (u32)name##_ROM_START, (u32)name##_ROM_END, name##_VRAM, \
      name##_TEXT_END, name##_BSS_START, name##_BSS_END, onLoad, onUnload}

s8 g_IsModuleAlive[MODULE_COUNT];
s32 g_ModuleRdpRequestFrames[MODULE_COUNT];

static s8 f_UnusedModuleIds[] UNUSED = {0x00, MODULE_ID_END};
static s8 f_ModuleIdsOfOverlays00To02[] = {
    MODULE_ROUND_RESULTS,
    MODULE_WIND_BOO,
    MODULE_GOLF_BALLS,
    MODULE_PLAY,
    MODULE_HOLE_OBJECTS,
    MODULE_ROUND_EVENTS,
    MODULE_SHOT_EFFECTS,
    MODULE_PAUSE_MENU,
    MODULE_SHOT_LABELS,
    MODULE_UNUSED_PLAY,
    MODULE_CLUB_PANEL,
    MODULE_ID_END,
};
static s8 f_ModuleIdsOfOverlay03[] = {MODULE_MENU, MODULE_ID_END};
static s8 f_ModuleIdsOfOverlay04[] = {
    MODULE_HOLE_BUILDER, MODULE_UNUSED_HOLE_BUILDER, MODULE_ID_END};
static s8 f_ModuleIdsOfOverlay05[] = {MODULE_COMPUTER_GOLFER, MODULE_ID_END};
static s8 f_ModuleIdsOfOverlay06[] = {MODULE_SHOT_SIMULATION, MODULE_ID_END};
static s8 f_ModuleIdsOfOverlay07[] = {MODULE_OPENING, MODULE_UNUSED_OPENING_A,
    MODULE_UNUSED_OPENING_B, MODULE_ID_END};
static s8 f_ModuleIdsOfOverlay08[] = {
    MODULE_TITLE_SCREEN, MODULE_UNUSED_TITLE_SCREEN, MODULE_ID_END};
static s8 f_ModuleIdsOfOverlay09[] = {MODULE_BOOT_LOGOS, MODULE_ID_END};
static s8 f_ModuleIdsOfOverlay10[] = {MODULE_CONFIRM_WAIT, MODULE_ID_END};
static s8 f_ModuleIdsOfOverlay11[] = {
    MODULE_SCORECARD, MODULE_UNUSED_SCORECARD, MODULE_ID_END};
static s8 f_ModuleIdsOfOverlay12[] = {MODULE_CLUB_SLOTS, MODULE_ID_END};
static s8 f_ModuleIdsOfOverlay13[] = {MODULE_COURSE_SELECT, MODULE_ID_END};
static s8 f_ModuleIdsOfOverlay14[] = {MODULE_SAVE_MENU, MODULE_ID_END};
static s8 f_ModuleIdsOfOverlay15[] = {MODULE_TRANSMIT, MODULE_UNUSED_TRANSMIT_A,
    MODULE_UNUSED_TRANSMIT_B, MODULE_ID_END};
static s8 f_ModuleIdsOfOverlay16[] = {MODULE_STAFF_ROLL, MODULE_ID_END};

static Overlay f_Overlays[OVERLAY_COUNT] = {
    OVERLAY_ENTRY(ovl_00, f_ModuleIdsOfOverlays00To02, NULL, NULL),
    OVERLAY_ENTRY(ovl_00, f_ModuleIdsOfOverlays00To02, NULL, NULL),
    OVERLAY_ENTRY(
        ovl_00, f_ModuleIdsOfOverlays00To02, OnLoadOverlay2, FreeScenery),
    OVERLAY_ENTRY(
        ovl_01, f_ModuleIdsOfOverlay03, LoadMenuResources, FreeMenuResources),
    OVERLAY_ENTRY(ovl_02, f_ModuleIdsOfOverlay04, NULL, NULL),
    OVERLAY_ENTRY(ovl_03, f_ModuleIdsOfOverlay05, NULL, NULL),
    OVERLAY_ENTRY(ovl_04, f_ModuleIdsOfOverlay06, NULL, NULL),
    OVERLAY_ENTRY(ovl_05, f_ModuleIdsOfOverlay07, OnLoadOverlay7, FreeOpening),
    OVERLAY_ENTRY(
        ovl_06, f_ModuleIdsOfOverlay08, LoadTitleScreen, UnloadTitleScreen),
    OVERLAY_ENTRY(ovl_07, f_ModuleIdsOfOverlay09, LoadBootLogos, NULL),
    OVERLAY_ENTRY(ovl_08, f_ModuleIdsOfOverlay10, NULL, NULL),
    OVERLAY_ENTRY(ovl_09, f_ModuleIdsOfOverlay11, NULL, NULL),
    OVERLAY_ENTRY(ovl_10, f_ModuleIdsOfOverlay12, NULL, NULL),
    OVERLAY_ENTRY(ovl_11, f_ModuleIdsOfOverlay13, NULL, NULL),
    OVERLAY_ENTRY(ovl_12, f_ModuleIdsOfOverlay14, NULL, NULL),
    OVERLAY_ENTRY(
        ovl_13, f_ModuleIdsOfOverlay15, LoadTransmitMenu, FreeTransmitMenu),
    OVERLAY_ENTRY(
        ovl_14, f_ModuleIdsOfOverlay16, LoadStaffRoll, UnloadStaffRoll),
};

/* The index of the overlay that holds each module, or -1 for none */
static s8 f_OverlayIndexOfModule[MODULE_COUNT];

void OnLoadOverlay2(void) {
  InitScenery();
  LoadGolfBallImages();
}

void OnLoadOverlay7(void) {
  ResetCameraEditor();
  LoadOpening();
}

s32 GetOverlayOfModule(s32 moduleId) {
  return f_OverlayIndexOfModule[moduleId] | OVERLAY_ID_BASE;
}

void InitOverlayManager(void) {
  s32 i;
  s8* moduleIds;
  s32 moduleIndex;

  for (i = 0; i < MODULE_COUNT; i++) {
    f_OverlayIndexOfModule[i] = -1;
    g_IsModuleAlive[i] = FALSE;
    g_ModuleRdpRequestFrames[i] = RDP_REQUEST_FRAME_NEVER;
  }

  /*
   * The unsigned compare is there to match: a signed one gives 2 words
   * differing, a u32 index 4, and != 10
   */
  for (i = 0; (u32)i < OVERLAY_COUNT; i++) {
    moduleIds = f_Overlays[i].moduleIds;
    moduleIndex = 0;
    while (moduleIds[moduleIndex] != MODULE_ID_END) {
      if (f_OverlayIndexOfModule[moduleIds[moduleIndex]] != -1 &&
          !IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
        osSyncPrintf("OverlayManager\n");
      }

      f_OverlayIndexOfModule[moduleIds[moduleIndex++]] = i;
    }
  }

  for (i = 0; i < MODULE_COUNT; i++) {
    if (f_OverlayIndexOfModule[i] == -1) {
      g_IsModuleAlive[i] = TRUE;
    }
  }
}

s32 GetOverlayIndex(s32 overlayId) {
  if (overlayId < OVERLAY_ID_BASE ||
      (u32)overlayId > OVERLAY_ID_BASE + OVERLAY_COUNT - 1) {
    if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
      osSyncPrintf("Illegal ModuleSetNo.\n");
    }

    return 0;
  }

  return overlayId & 0xFF;
}

Boolean CanLoadOverlay(s32 overlayId) {
  s32 overlayIndex = GetOverlayIndex(overlayId);
  u32 otherOverlayIndex;
  s32 moduleIndex;
  s8* moduleIds;

  if (f_Overlays[overlayIndex].isLoaded) {
    if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
      osSyncPrintf("Already Loaded\n");
    }

    return TRUE;
  }

  for (otherOverlayIndex = 0; otherOverlayIndex < OVERLAY_COUNT;
      otherOverlayIndex++) {
    if (otherOverlayIndex == (u32)overlayIndex) {
      continue;
    }

    if ((u32)f_Overlays[otherOverlayIndex].vramStart >
        (u32)f_Overlays[overlayIndex].bssEnd) {
      continue;
    }

    if ((u32)f_Overlays[otherOverlayIndex].bssEnd <
        (u32)f_Overlays[overlayIndex].vramStart) {
      continue;
    }

    if (f_Overlays[otherOverlayIndex].isLoaded) {
      if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
        osSyncPrintf("Conflicting %d m.s. and other moduleset[%x] is alive\n",
            overlayIndex, otherOverlayIndex);
      }

      return FALSE;
    }

    moduleIds = f_Overlays[otherOverlayIndex].moduleIds;
    moduleIndex = 0;
    while (moduleIds[moduleIndex] != MODULE_ID_END) {
      if ((u32)(g_FrameCount -
              g_ModuleRdpRequestFrames[moduleIds[moduleIndex]]) <
          RDP_REQUEST_FRAMES_IN_USE) {
        if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
          osSyncPrintf(
              "Conflicting %d m.s. and other module[%x-%x]'s RDP request\n",
              overlayIndex, otherOverlayIndex, moduleIndex);
        }

        return FALSE;
      }

      moduleIndex++;
    }
  }

  return TRUE;
}

s32 LoadOverlay(s32 overlayId) {
  Overlay* overlay;
  s8* moduleIds;
  s32 moduleIndex;

  if (CanLoadOverlay(overlayId)) {
    overlayId = GetOverlayIndex(overlayId);
    if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
      osSyncPrintf("Load ModuleSet %x (absolute No.)\n", overlayId);
    }

    overlay = &f_Overlays[overlayId];
    moduleIds = overlay->moduleIds;
    osWritebackDCacheAll();
    nuPiReadRom(overlay->romStart, overlay->vramStart,
        overlay->romEnd - overlay->romStart);
    bzero(overlay->bssStart, overlay->bssEnd - overlay->bssStart);
    osWritebackDCache(overlay->bssStart, overlay->bssEnd - overlay->bssStart);
    osInvalICache(overlay->vramStart, overlay->textEnd - overlay->vramStart);
    if (overlay->onLoad != NULL) {
      overlay->onLoad();
    }

    moduleIndex = 0;
    while (moduleIds[moduleIndex] != MODULE_ID_END) {
      g_IsModuleAlive[moduleIds[moduleIndex++]] = TRUE;
    }

    overlay->isLoaded = TRUE;
    if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
      osSyncPrintf("Complete Load ModuleSet %x (absolute No.)\n", overlayId);
    }

    return TRUE;
  } else {
    if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
      osSyncPrintf("Can't Load ModuleSet %x (logical No.)\n", overlayId);
    }

    return FALSE;
  }
}

void UnloadOverlay(s32 overlayId) {
  Overlay* overlay;
  s32 moduleIndex;
  s8* moduleIds;

  overlayId = GetOverlayIndex(overlayId);
  overlay = &f_Overlays[overlayId];
  if (overlay->isLoaded) {
    if (overlay->onUnload != NULL) {
      overlay->onUnload();
    }

    moduleIds = overlay->moduleIds;
    overlay->isLoaded = FALSE;
    if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
      osSyncPrintf("Moduleset %x Disposed\n", overlayId);
    }

    moduleIndex = 0;
    while (moduleIds[moduleIndex] != MODULE_ID_END) {
      g_IsModuleAlive[moduleIds[moduleIndex++]] = FALSE;
    }

    osWritebackDCacheAll();

    /* Clear the code so that stale calls into it fail early */
    if (g_IsDebugEnabled &&
        osVirtualToPhysical(overlay->vramStart) < BASE_RDRAM_SIZE) {
      bzero(overlay->vramStart, overlay->textEnd - overlay->vramStart);
      osWritebackDCache(
          overlay->vramStart, overlay->textEnd - overlay->vramStart);
      osInvalICache(overlay->vramStart, overlay->textEnd - overlay->vramStart);
    }
  }
}

/* The module number is missing from the format's arguments in the original */
s32 ReportModuleNotAlive(s32 moduleId UNUSED) {
  if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
    osSyncPrintf("Module %d not alive !!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  }

  if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
    osSyncPrintf("Module %d not alive !!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  }

  return 0;
}
