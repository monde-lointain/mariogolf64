#include "overlay_manager.h"

#include <nusys.h>
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

#define RDP_REQUEST_FRAME_NEVER 0x80000000

#define RDP_REQUEST_FRAMES_IN_USE 2

#define BASE_RDRAM_SIZE 0x400000

#define OVERLAY_ENTRY(name, moduleIds, onLoad, onUnload) \
    {FALSE, \
        moduleIds, \
        (u32)name##_ROM_START, \
        (u32)name##_ROM_END, \
        name##_VRAM, \
        name##_TEXT_END, \
        name##_BSS_START, \
        name##_BSS_END, \
        onLoad, \
        onUnload}

s8 g_IsModuleAlive[MODULE_COUNT];
s32 g_ModuleRdpRequestFrames[MODULE_COUNT];

static ModuleId8 f_UnusedModuleIds[] UNUSED = {0x00, MODULE_ID_END};
static ModuleId8 f_PlayModuleIds[] = {
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
static ModuleId8 f_MenuModuleIds[] = {MODULE_MENU, MODULE_ID_END};
static ModuleId8 f_HoleBuilderModuleIds[] =
    {MODULE_HOLE_BUILDER, MODULE_UNUSED_HOLE_BUILDER, MODULE_ID_END};
static ModuleId8 f_ComputerGolferModuleIds[] = {MODULE_COMPUTER_GOLFER, MODULE_ID_END};
static ModuleId8 f_ShotSimulationModuleIds[] = {MODULE_SHOT_SIMULATION, MODULE_ID_END};
static ModuleId8 f_OpeningModuleIds[] =
    {MODULE_OPENING, MODULE_UNUSED_OPENING_A, MODULE_UNUSED_OPENING_B, MODULE_ID_END};
static ModuleId8 f_TitleScreenModuleIds[] =
    {MODULE_TITLE_SCREEN, MODULE_UNUSED_TITLE_SCREEN, MODULE_ID_END};
static ModuleId8 f_BootLogosModuleIds[] = {MODULE_BOOT_LOGOS, MODULE_ID_END};
static ModuleId8 f_ConfirmWaitModuleIds[] = {MODULE_CONFIRM_WAIT, MODULE_ID_END};
static ModuleId8 f_ScorecardModuleIds[] =
    {MODULE_SCORECARD, MODULE_UNUSED_SCORECARD, MODULE_ID_END};
static ModuleId8 f_ClubSlotsModuleIds[] = {MODULE_CLUB_SLOTS, MODULE_ID_END};
static ModuleId8 f_CourseSelectModuleIds[] = {MODULE_COURSE_SELECT, MODULE_ID_END};
static ModuleId8 f_SaveMenuModuleIds[] = {MODULE_SAVE_MENU, MODULE_ID_END};
static ModuleId8 f_TransmitModuleIds[] =
    {MODULE_TRANSMIT, MODULE_UNUSED_TRANSMIT_A, MODULE_UNUSED_TRANSMIT_B, MODULE_ID_END};
static ModuleId8 f_StaffRollModuleIds[] = {MODULE_STAFF_ROLL, MODULE_ID_END};

static Overlay f_Overlays[OVERLAY_COUNT] = {
    OVERLAY_ENTRY(ovl_00, f_PlayModuleIds, NULL, NULL),
    OVERLAY_ENTRY(ovl_00, f_PlayModuleIds, NULL, NULL),
    OVERLAY_ENTRY(ovl_00, f_PlayModuleIds, LoadSceneryResources, FreeScenery),
    OVERLAY_ENTRY(ovl_01, f_MenuModuleIds, LoadMenuResources, FreeMenuResources),
    OVERLAY_ENTRY(ovl_02, f_HoleBuilderModuleIds, NULL, NULL),
    OVERLAY_ENTRY(ovl_03, f_ComputerGolferModuleIds, NULL, NULL),
    OVERLAY_ENTRY(ovl_04, f_ShotSimulationModuleIds, NULL, NULL),
    OVERLAY_ENTRY(ovl_05, f_OpeningModuleIds, LoadOpeningResources, FreeOpening),
    OVERLAY_ENTRY(ovl_06, f_TitleScreenModuleIds, LoadTitleScreen, UnloadTitleScreen),
    OVERLAY_ENTRY(ovl_07, f_BootLogosModuleIds, LoadBootLogos, NULL),
    OVERLAY_ENTRY(ovl_08, f_ConfirmWaitModuleIds, NULL, NULL),
    OVERLAY_ENTRY(ovl_09, f_ScorecardModuleIds, NULL, NULL),
    OVERLAY_ENTRY(ovl_10, f_ClubSlotsModuleIds, NULL, NULL),
    OVERLAY_ENTRY(ovl_11, f_CourseSelectModuleIds, NULL, NULL),
    OVERLAY_ENTRY(ovl_12, f_SaveMenuModuleIds, NULL, NULL),
    OVERLAY_ENTRY(ovl_13, f_TransmitModuleIds, LoadTransmitMenu, FreeTransmitMenu),
    OVERLAY_ENTRY(ovl_14, f_StaffRollModuleIds, LoadStaffRoll, FreeStaffRoll),
};

// A module that no overlay holds, so it is always alive.
#define NO_OVERLAY (-1)

static s8 f_OverlayIndexOfModule[MODULE_COUNT];

void LoadSceneryResources(void) {
    InitScenery();
    LoadGolfBallImages();
}

void LoadOpeningResources(void) {
    ResetCameraEditor();
    LoadOpening();
}

s32 GetOverlayOfModule(ModuleId moduleId) {
    return f_OverlayIndexOfModule[moduleId] | OVERLAY_ID_BASE;
}

void InitOverlayManager(void) {
    s32 i;
    ModuleId8 *moduleIds;
    s32 moduleIndex;

    for (i = 0; i < MODULE_COUNT; i++) {
        f_OverlayIndexOfModule[i] = NO_OVERLAY;
        g_IsModuleAlive[i] = FALSE;
        g_ModuleRdpRequestFrames[i] = RDP_REQUEST_FRAME_NEVER;
    }

    for (i = 0; (u32)i < OVERLAY_COUNT; i++) {
        moduleIds = f_Overlays[i].moduleIds;
        moduleIndex = 0;
        while (moduleIds[moduleIndex] != MODULE_ID_END) {
            if (f_OverlayIndexOfModule[moduleIds[moduleIndex]] != NO_OVERLAY &&
                !IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
                osSyncPrintf("OverlayManager\n");
            }

            f_OverlayIndexOfModule[moduleIds[moduleIndex++]] = i;
        }
    }

    for (i = 0; i < MODULE_COUNT; i++) {
        if (f_OverlayIndexOfModule[i] == NO_OVERLAY) {
            g_IsModuleAlive[i] = TRUE;
        }
    }
}

s32 GetOverlayIndex(s32 overlayId) {
    if (overlayId < OVERLAY_ID_BASE || (u32)overlayId > OVERLAY_ID_BASE + OVERLAY_COUNT - 1) {
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
    ModuleId8 *moduleIds;

    if (f_Overlays[overlayIndex].isLoaded) {
        if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
            osSyncPrintf("Already Loaded\n");
        }

        return TRUE;
    }

    for (otherOverlayIndex = 0; otherOverlayIndex < OVERLAY_COUNT; otherOverlayIndex++) {
        if (otherOverlayIndex == (u32)overlayIndex) {
            continue;
        }

        if ((u32)f_Overlays[otherOverlayIndex].vramStart > (u32)f_Overlays[overlayIndex].bssEnd) {
            continue;
        }

        if ((u32)f_Overlays[otherOverlayIndex].bssEnd < (u32)f_Overlays[overlayIndex].vramStart) {
            continue;
        }

        if (f_Overlays[otherOverlayIndex].isLoaded) {
            if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
                osSyncPrintf(
                    "Conflicting %d m.s. and other moduleset[%x] is alive\n",
                    overlayIndex,
                    otherOverlayIndex
                );
            }

            return FALSE;
        }

        moduleIds = f_Overlays[otherOverlayIndex].moduleIds;
        moduleIndex = 0;
        while (moduleIds[moduleIndex] != MODULE_ID_END) {
            if ((u32)(g_FrameCount - g_ModuleRdpRequestFrames[moduleIds[moduleIndex]]) <
                RDP_REQUEST_FRAMES_IN_USE) {
                if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
                    osSyncPrintf(
                        "Conflicting %d m.s. and other module[%x-%x]'s RDP request\n",
                        overlayIndex,
                        otherOverlayIndex,
                        moduleIndex
                    );
                }

                return FALSE;
            }

            moduleIndex++;
        }
    }

    return TRUE;
}

s32 LoadOverlay(s32 overlayId) {
    Overlay *overlay;
    ModuleId8 *moduleIds;
    s32 moduleIndex;

    if (CanLoadOverlay(overlayId)) {
        overlayId = GetOverlayIndex(overlayId);
        // overlayId now holds the overlay's index; a separate local breaks the match.
        if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
            osSyncPrintf("Load ModuleSet %x (absolute No.)\n", overlayId);
        }

        overlay = &f_Overlays[overlayId];
        moduleIds = overlay->moduleIds;
        osWritebackDCacheAll();
        nuPiReadRom(overlay->romStart, overlay->vramStart, overlay->romEnd - overlay->romStart);
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
    }
    else {
        if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
            osSyncPrintf("Can't Load ModuleSet %x (logical No.)\n", overlayId);
        }

        return FALSE;
    }
}

void UnloadOverlay(s32 overlayId) {
    Overlay *overlay;
    s32 moduleIndex;
    ModuleId8 *moduleIds;

    overlayId = GetOverlayIndex(overlayId);
    // overlayId now holds the overlay's index; a separate local breaks the match.
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

        if (g_IsDebugEnabled && osVirtualToPhysical(overlay->vramStart) < BASE_RDRAM_SIZE) {
            bzero(overlay->vramStart, overlay->textEnd - overlay->vramStart);
            osWritebackDCache(overlay->vramStart, overlay->textEnd - overlay->vramStart);
            osInvalICache(overlay->vramStart, overlay->textEnd - overlay->vramStart);
        }
    }
}

Boolean ReportModuleNotAlive(ModuleId moduleId UNUSED) {
    if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
        osSyncPrintf("Module %d not alive !!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }

    if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
        osSyncPrintf("Module %d not alive !!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }

    return FALSE;
}
