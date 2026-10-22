#include "common.h"
#include "game_flags.h"
#include "functions.h"
#include "overlay_manager.h"
#include "segment_symbols.h"
#include "variables.h"

/* A frame long enough ago that no module conflicts with an overlay */
#define RDP_REQUEST_FRAME_NEVER 0x80000000

/* Modules whose display lists the RDP may still be reading are in use for
   this many frames after they make a request */
#define RDP_REQUEST_FRAMES_IN_USE 2

/* Size of the RDRAM that every console has, without the Expansion Pak */
#define BASE_RDRAM_SIZE 0x400000

#define OVERLAY_ENTRY(name, moduleIds, onLoad, onUnload) \
    { FALSE, moduleIds, (u32)name##_ROM_START, (u32)name##_ROM_END, name##_VRAM, \
      name##_TEXT_END, name##_BSS_START, name##_BSS_END, onLoad, onUnload }

static s8 sUnusedModuleIds[] = { 0x00, MODULE_ID_END };
static s8 sModuleIdsOfOverlays00To02[] = {
    0x0F, 0x09, 0x01, 0x0A, 0x0B, 0x11, 0x12, 0x14, 0x17, 0x18, 0x19, MODULE_ID_END,
};
static s8 sModuleIdsOfOverlay03[] = { 0x02, MODULE_ID_END };
static s8 sModuleIdsOfOverlay04[] = { 0x06, 0x1C, MODULE_ID_END };
static s8 sModuleIdsOfOverlay05[] = { 0x07, MODULE_ID_END };
static s8 sModuleIdsOfOverlay06[] = { 0x08, MODULE_ID_END };
static s8 sModuleIdsOfOverlay07[] = { 0x03, 0x10, 0x13, MODULE_ID_END };
static s8 sModuleIdsOfOverlay08[] = { 0x04, 0x1E, MODULE_ID_END };
static s8 sModuleIdsOfOverlay09[] = { 0x0C, MODULE_ID_END };
static s8 sModuleIdsOfOverlay10[] = { 0x0D, MODULE_ID_END };
static s8 sModuleIdsOfOverlay11[] = { 0x05, 0x0E, MODULE_ID_END };
static s8 sModuleIdsOfOverlay12[] = { 0x1A, MODULE_ID_END };
static s8 sModuleIdsOfOverlay13[] = { 0x15, MODULE_ID_END };
static s8 sModuleIdsOfOverlay14[] = { 0x16, MODULE_ID_END };
static s8 sModuleIdsOfOverlay15[] = { 0x1B, 0x1D, 0x20, MODULE_ID_END };
static s8 sModuleIdsOfOverlay16[] = { 0x1F, MODULE_ID_END };

Overlay g_Overlays[OVERLAY_COUNT] = {
    OVERLAY_ENTRY(ovl_00, sModuleIdsOfOverlays00To02, NULL, NULL),
    OVERLAY_ENTRY(ovl_00, sModuleIdsOfOverlays00To02, NULL, NULL),
    OVERLAY_ENTRY(ovl_00, sModuleIdsOfOverlays00To02, OnLoadOverlay2, func_80080E14),
    OVERLAY_ENTRY(ovl_01, sModuleIdsOfOverlay03, LoadMenuResources, FreeMenuResources),
    OVERLAY_ENTRY(ovl_02, sModuleIdsOfOverlay04, NULL, NULL),
    OVERLAY_ENTRY(ovl_03, sModuleIdsOfOverlay05, NULL, NULL),
    OVERLAY_ENTRY(ovl_04, sModuleIdsOfOverlay06, NULL, NULL),
    OVERLAY_ENTRY(ovl_05, sModuleIdsOfOverlay07, OnLoadOverlay7, func_801F5100_ovl_05),
    OVERLAY_ENTRY(ovl_06, sModuleIdsOfOverlay08, func_801F4C84_ovl_06, func_801F4D48_ovl_06),
    OVERLAY_ENTRY(ovl_07, sModuleIdsOfOverlay09, LoadBootLogos, NULL),
    OVERLAY_ENTRY(ovl_08, sModuleIdsOfOverlay10, NULL, NULL),
    OVERLAY_ENTRY(ovl_09, sModuleIdsOfOverlay11, NULL, NULL),
    OVERLAY_ENTRY(ovl_10, sModuleIdsOfOverlay12, NULL, NULL),
    OVERLAY_ENTRY(ovl_11, sModuleIdsOfOverlay13, NULL, NULL),
    OVERLAY_ENTRY(ovl_12, sModuleIdsOfOverlay14, NULL, NULL),
    OVERLAY_ENTRY(ovl_13, sModuleIdsOfOverlay15, func_801F4B20_ovl_13, func_801F4E70_ovl_13),
    OVERLAY_ENTRY(ovl_14, sModuleIdsOfOverlay16, func_801F4B9C_ovl_14, func_801F4E54_ovl_14),
};

/* The index of the overlay that holds each module, or -1 for none */
static s8 sOverlayIndexOfModule[MODULE_COUNT];

void OnLoadOverlay2(void) {
    func_80080DCC();
    func_801FD624_ovl_00();
}

void OnLoadOverlay7(void) {
    func_802033B0_ovl_05();
    func_801F5018_ovl_05();
}

s32 GetOverlayOfModule(s32 moduleId) {
    return sOverlayIndexOfModule[moduleId] | OVERLAY_ID_BASE;
}

void InitOverlayManager(void) {
    s32 i;
    s8 *moduleIds;
    s32 moduleIndex;

    for (i = 0; i < MODULE_COUNT; i++) {
        sOverlayIndexOfModule[i] = -1;
        g_IsModuleAlive[i] = FALSE;
        g_ModuleRdpRequestFrames[i] = RDP_REQUEST_FRAME_NEVER;
    }

    for (i = 0; i < sizeof(g_Overlays) / sizeof(g_Overlays[0]); i++) {
        moduleIds = g_Overlays[i].moduleIds;
        moduleIndex = 0;
        while (moduleIds[moduleIndex] != MODULE_ID_END) {
            if (sOverlayIndexOfModule[moduleIds[moduleIndex]] != -1 && !IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
                osSyncPrintf("OverlayManager\n");
            }
            sOverlayIndexOfModule[moduleIds[moduleIndex++]] = i;
        }
    }

    for (i = 0; i < MODULE_COUNT; i++) {
        if (sOverlayIndexOfModule[i] == -1) {
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

s32 CanLoadOverlay(s32 overlayId) {
    s32 overlayIndex = GetOverlayIndex(overlayId);
    u32 otherOverlayIndex;
    s32 moduleIndex;
    s8 *moduleIds;

    if (g_Overlays[overlayIndex].isLoaded) {
        if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
            osSyncPrintf("Already Loaded\n");
        }
        return TRUE;
    }

    for (otherOverlayIndex = 0; otherOverlayIndex < OVERLAY_COUNT; otherOverlayIndex++) {
        if (otherOverlayIndex == overlayIndex) {
            continue;
        }
        if ((u32)g_Overlays[otherOverlayIndex].vramStart > (u32)g_Overlays[overlayIndex].bssEnd) {
            continue;
        }
        if ((u32)g_Overlays[otherOverlayIndex].bssEnd < (u32)g_Overlays[overlayIndex].vramStart) {
            continue;
        }
        if (g_Overlays[otherOverlayIndex].isLoaded) {
            if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
                osSyncPrintf("Conflicting %d m.s. and other moduleset[%x] is alive\n", overlayIndex, otherOverlayIndex);
            }
            return FALSE;
        }
        moduleIds = g_Overlays[otherOverlayIndex].moduleIds;
        moduleIndex = 0;
        while (moduleIds[moduleIndex] != MODULE_ID_END) {
            if ((u32)(g_FrameCount - g_ModuleRdpRequestFrames[moduleIds[moduleIndex]]) < RDP_REQUEST_FRAMES_IN_USE) {
                if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
                    osSyncPrintf("Conflicting %d m.s. and other module[%x-%x]'s RDP request\n", overlayIndex,
                                 otherOverlayIndex, moduleIndex);
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
    s8 *moduleIds;
    s32 moduleIndex;

    if (CanLoadOverlay(overlayId)) {
        overlayId = GetOverlayIndex(overlayId);
        if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
            osSyncPrintf("Load ModuleSet %x (absolute No.)\n", overlayId);
        }
        overlay = &g_Overlays[overlayId];
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
    } else {
        if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
            osSyncPrintf("Can't Load ModuleSet %x (logical No.)\n", overlayId);
        }
        return FALSE;
    }
}

void UnloadOverlay(s32 overlayId) {
    Overlay *overlay;
    s32 moduleIndex;
    s8 *moduleIds;

    overlayId = GetOverlayIndex(overlayId);
    overlay = &g_Overlays[overlayId];
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
        if (g_IsDebugEnabled && osVirtualToPhysical(overlay->vramStart) < BASE_RDRAM_SIZE) {
            bzero(overlay->vramStart, overlay->textEnd - overlay->vramStart);
            osWritebackDCache(overlay->vramStart, overlay->textEnd - overlay->vramStart);
            osInvalICache(overlay->vramStart, overlay->textEnd - overlay->vramStart);
        }
    }
}

/* The module number is missing from the format's arguments in the original */
s32 ReportModuleNotAlive(void) {
    if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
        osSyncPrintf("Module %d not alive !!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }
    if (!IsGameFlagSet(GAME_FLAG_QUIET_OVERLAY_LOG)) {
        osSyncPrintf("Module %d not alive !!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }
    return 0;
}
