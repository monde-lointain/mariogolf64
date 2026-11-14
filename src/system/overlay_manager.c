/*
 * Loads and unloads the overlays that hold the game's modules.
 */

#include "system/overlay_manager.h"

#include <nusys.h>
#include <ultra64.h>

#include "cinematics/boot_logos.h"
#include "cinematics/opening.h"
#include "cinematics/opening_camera_editor.h"
#include "cinematics/staff_roll.h"
#include "debug/game_flags.h"
#include "menus/menu.h"
#include "menus/title_screen.h"
#include "menus/transmit_menu.h"
#include "round/round_hud.h"
#include "shot/golf_balls.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/main.h"
#include "system/segment_symbols.h"

// A frame long enough ago that no module conflicts with an overlay
#define RDP_REQUEST_FRAME_NEVER 0x80000000

// Modules whose display lists the RDP may still be reading are in use for
// this many frames after they make a request
#define RDP_REQUEST_FRAMES_IN_USE 2

// An f_OverlayIndexOfModule entry for a module in no overlay
#define NO_OVERLAY (-1)

// Size of the RDRAM that every console has, without the Expansion Pak
#define BASE_RDRAM_SIZE 0x400000

// An f_Overlays entry that loads the named segment
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

Boolean8 g_IsModuleAlive[MODULE_COUNT];
// The frame on which each module last gave the RDP a display list
s32 g_ModuleRdpRequestFrames[MODULE_COUNT];

// The modules of each overlay, each list ending with MODULE_ID_END
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

// The overlay table. The first three entries load the same segment, and only
// the third loads and frees the scenery with it.
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

// The index of the overlay that holds each module, or -1 for none
static s8 f_OverlayIndexOfModule[MODULE_COUNT];

// The load hooks of the overlays that load more than one file's resources
void LoadSceneryResources(void) {
    InitScenery();
    LoadGolfBallImages();
}

void LoadOpeningResources(void) {
    ResetCameraEditor();
    LoadOpening();
}

// The ID of the overlay that holds the module
s32 GetOverlayOfModule(ModuleId moduleId) {
    return f_OverlayIndexOfModule[moduleId] | OVERLAY_ID_BASE;
}

// Indexes the overlay of each module, marking the modules in no overlay alive
void InitOverlayManager(void) {
    s32 i;
    ModuleId8 *moduleIds;
    s32 moduleIndex;

    for (i = 0; i < MODULE_COUNT; i++) {
        f_OverlayIndexOfModule[i] = NO_OVERLAY;
        g_IsModuleAlive[i] = FALSE;
        g_ModuleRdpRequestFrames[i] = RDP_REQUEST_FRAME_NEVER;
    }

    // The unsigned compare is there to match: a signed one gives 2 words
    // differing, a u32 index 4, and != 10
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

// The overlay's index in f_Overlays, or 0 for an ID out of range
s32 GetOverlayIndex(s32 overlayId) {
    if (overlayId < OVERLAY_ID_BASE || (u32)overlayId > OVERLAY_ID_BASE + OVERLAY_COUNT - 1) {
        if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
            osSyncPrintf("Illegal ModuleSetNo.\n");
        }

        return 0;
    }

    return overlayId & 0xFF;
}

// Whether the overlay is loaded or can be: no loaded overlay may overlap its
// memory, and no overlapping overlay's modules may have display lists that the
// RDP could still be reading
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

        // Overlays apart in memory don't conflict
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

// Reads the overlay's segment from ROM, clears its bss and marks its modules
// alive, and returns whether it could
Boolean LoadOverlay(s32 overlayId) {
    Overlay *overlay;
    ModuleId8 *moduleIds;
    s32 moduleIndex;

    if (CanLoadOverlay(overlayId)) {
        overlayId = GetOverlayIndex(overlayId);
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

// Marks the overlay's modules dead, after its unload hook frees what they hold
void UnloadOverlay(s32 overlayId) {
    Overlay *overlay;
    s32 moduleIndex;
    ModuleId8 *moduleIds;

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

        // Clear the code so that stale calls into it fail early
        if (g_IsDebugEnabled && osVirtualToPhysical(overlay->vramStart) < BASE_RDRAM_SIZE) {
            bzero(overlay->vramStart, overlay->textEnd - overlay->vramStart);
            osWritebackDCache(overlay->vramStart, overlay->textEnd - overlay->vramStart);
            osInvalICache(overlay->vramStart, overlay->textEnd - overlay->vramStart);
        }
    }
}

// Reports a call into a module whose overlay isn't loaded, twice, and
// returns FALSE so that the caller's test fails. The module number is missing
// from the format's arguments in the original.
Boolean ReportModuleNotAlive(ModuleId moduleId UNUSED) {
    if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
        osSyncPrintf("Module %d not alive !!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }

    if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
        osSyncPrintf("Module %d not alive !!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }

    return FALSE;
}
