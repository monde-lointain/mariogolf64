#include "common.h"
#include "debug_flags.h"
#include "functions.h"
#include "overlay_manager.h"
#include "segment_symbols.h"
#include "variables.h"

/* A frame long enough ago that no module conflicts with a module set */
#define RDP_REQUEST_FRAME_NEVER 0x80000000

/* Modules whose display lists the RDP may still be reading are in use for
   this many frames after they make a request */
#define RDP_REQUEST_FRAMES_IN_USE 2

/* Size of the RDRAM that every console has, without the Expansion Pak */
#define BASE_RDRAM_SIZE 0x400000

#define MODULE_SET(name, moduleIds, onLoad, onUnload) \
    { FALSE, moduleIds, (u32)name##_ROM_START, (u32)name##_ROM_END, name##_VRAM, \
      name##_TEXT_END, name##_BSS_START, name##_BSS_END, onLoad, onUnload }

static s8 sUnusedModuleIds[] = { 0x00, MODULE_ID_END };
static s8 sModuleIdsOfSets00To02[] = {
    0x0F, 0x09, 0x01, 0x0A, 0x0B, 0x11, 0x12, 0x14, 0x17, 0x18, 0x19, MODULE_ID_END,
};
static s8 sModuleIdsOfSet03[] = { 0x02, MODULE_ID_END };
static s8 sModuleIdsOfSet04[] = { 0x06, 0x1C, MODULE_ID_END };
static s8 sModuleIdsOfSet05[] = { 0x07, MODULE_ID_END };
static s8 sModuleIdsOfSet06[] = { 0x08, MODULE_ID_END };
static s8 sModuleIdsOfSet07[] = { 0x03, 0x10, 0x13, MODULE_ID_END };
static s8 sModuleIdsOfSet08[] = { 0x04, 0x1E, MODULE_ID_END };
static s8 sModuleIdsOfSet09[] = { 0x0C, MODULE_ID_END };
static s8 sModuleIdsOfSet10[] = { 0x0D, MODULE_ID_END };
static s8 sModuleIdsOfSet11[] = { 0x05, 0x0E, MODULE_ID_END };
static s8 sModuleIdsOfSet12[] = { 0x1A, MODULE_ID_END };
static s8 sModuleIdsOfSet13[] = { 0x15, MODULE_ID_END };
static s8 sModuleIdsOfSet14[] = { 0x16, MODULE_ID_END };
static s8 sModuleIdsOfSet15[] = { 0x1B, 0x1D, 0x20, MODULE_ID_END };
static s8 sModuleIdsOfSet16[] = { 0x1F, MODULE_ID_END };

ModuleSet g_ModuleSets[MODULE_SET_COUNT] = {
    MODULE_SET(ovl_00, sModuleIdsOfSets00To02, NULL, NULL),
    MODULE_SET(ovl_00, sModuleIdsOfSets00To02, NULL, NULL),
    MODULE_SET(ovl_00, sModuleIdsOfSets00To02, OnLoadModuleSet2, func_80080E14),
    MODULE_SET(ovl_01, sModuleIdsOfSet03, func_8005A580, func_8005ACF8),
    MODULE_SET(ovl_02, sModuleIdsOfSet04, NULL, NULL),
    MODULE_SET(ovl_03, sModuleIdsOfSet05, NULL, NULL),
    MODULE_SET(ovl_04, sModuleIdsOfSet06, NULL, NULL),
    MODULE_SET(ovl_05, sModuleIdsOfSet07, OnLoadModuleSet7, func_801F5100_ovl_05),
    MODULE_SET(ovl_06, sModuleIdsOfSet08, func_801F4C84_ovl_06, func_801F4D48_ovl_06),
    MODULE_SET(ovl_07, sModuleIdsOfSet09, func_801F4A40_ovl_07, NULL),
    MODULE_SET(ovl_08, sModuleIdsOfSet10, NULL, NULL),
    MODULE_SET(ovl_09, sModuleIdsOfSet11, NULL, NULL),
    MODULE_SET(ovl_10, sModuleIdsOfSet12, NULL, NULL),
    MODULE_SET(ovl_11, sModuleIdsOfSet13, NULL, NULL),
    MODULE_SET(ovl_12, sModuleIdsOfSet14, NULL, NULL),
    MODULE_SET(ovl_13, sModuleIdsOfSet15, func_801F4B20_ovl_13, func_801F4E70_ovl_13),
    MODULE_SET(ovl_14, sModuleIdsOfSet16, func_801F4B9C_ovl_14, func_801F4E54_ovl_14),
};

/* The index of the module set that holds each module, or -1 for none */
static s8 sModuleSetOfModule[MODULE_COUNT];

void OnLoadModuleSet2(void) {
    func_80080DCC();
    func_801FD624_ovl_00();
}

void OnLoadModuleSet7(void) {
    func_802033B0_ovl_05();
    func_801F5018_ovl_05();
}

s32 GetModuleSetOfModule(s32 moduleId) {
    return sModuleSetOfModule[moduleId] | MODULE_SET_NUMBER_BASE;
}

void InitOverlayManager(void) {
    s32 i;
    s8 *moduleIds;
    s32 moduleIndex;

    for (i = 0; i < MODULE_COUNT; i++) {
        sModuleSetOfModule[i] = -1;
        g_IsModuleAlive[i] = FALSE;
        g_ModuleRdpRequestFrames[i] = RDP_REQUEST_FRAME_NEVER;
    }

    for (i = 0; i < sizeof(g_ModuleSets) / sizeof(g_ModuleSets[0]); i++) {
        moduleIds = g_ModuleSets[i].moduleIds;
        moduleIndex = 0;
        while (moduleIds[moduleIndex] != MODULE_ID_END) {
            if (sModuleSetOfModule[moduleIds[moduleIndex]] != -1 && !IsDebugFlagSet(DEBUG_FLAG_QUIET_OVERLAY_LOG)) {
                osSyncPrintf("OverlayManager\n");
            }
            sModuleSetOfModule[moduleIds[moduleIndex++]] = i;
        }
    }

    for (i = 0; i < MODULE_COUNT; i++) {
        if (sModuleSetOfModule[i] == -1) {
            g_IsModuleAlive[i] = TRUE;
        }
    }
}

s32 GetModuleSetIndex(s32 moduleSetNumber) {
    if (moduleSetNumber < MODULE_SET_NUMBER_BASE || (u32)moduleSetNumber > MODULE_SET_NUMBER_BASE + MODULE_SET_COUNT - 1) {
        if (!IsDebugFlagSet(DEBUG_FLAG_QUIET_OVERLAY_LOG)) {
            osSyncPrintf("Illegal ModuleSetNo.\n");
        }
        return 0;
    }
    return moduleSetNumber & 0xFF;
}

s32 CanLoadModuleSet(s32 moduleSetNumber) {
    s32 setIndex = GetModuleSetIndex(moduleSetNumber);
    u32 otherIndex;
    s32 moduleIndex;
    s8 *moduleIds;

    if (g_ModuleSets[setIndex].isLoaded) {
        if (!IsDebugFlagSet(DEBUG_FLAG_QUIET_OVERLAY_LOG)) {
            osSyncPrintf("Already Loaded\n");
        }
        return TRUE;
    }

    for (otherIndex = 0; otherIndex < MODULE_SET_COUNT; otherIndex++) {
        if (otherIndex == setIndex) {
            continue;
        }
        if ((u32)g_ModuleSets[otherIndex].vramStart > (u32)g_ModuleSets[setIndex].bssEnd) {
            continue;
        }
        if ((u32)g_ModuleSets[otherIndex].bssEnd < (u32)g_ModuleSets[setIndex].vramStart) {
            continue;
        }
        if (g_ModuleSets[otherIndex].isLoaded) {
            if (!IsDebugFlagSet(DEBUG_FLAG_QUIET_OVERLAY_LOG)) {
                osSyncPrintf("Conflicting %d m.s. and other moduleset[%x] is alive\n", setIndex, otherIndex);
            }
            return FALSE;
        }
        moduleIds = g_ModuleSets[otherIndex].moduleIds;
        moduleIndex = 0;
        while (moduleIds[moduleIndex] != MODULE_ID_END) {
            if ((u32)(g_FrameCount - g_ModuleRdpRequestFrames[moduleIds[moduleIndex]]) < RDP_REQUEST_FRAMES_IN_USE) {
                if (!IsDebugFlagSet(DEBUG_FLAG_QUIET_OVERLAY_LOG)) {
                    osSyncPrintf("Conflicting %d m.s. and other module[%x-%x]'s RDP request\n", setIndex,
                                 otherIndex, moduleIndex);
                }
                return FALSE;
            }
            moduleIndex++;
        }
    }
    return TRUE;
}

s32 LoadModuleSet(s32 moduleSetNumber) {
    ModuleSet *set;
    s8 *moduleIds;
    s32 moduleIndex;

    if (CanLoadModuleSet(moduleSetNumber)) {
        moduleSetNumber = GetModuleSetIndex(moduleSetNumber);
        if (!IsDebugFlagSet(DEBUG_FLAG_QUIET_OVERLAY_LOG)) {
            osSyncPrintf("Load ModuleSet %x (absolute No.)\n", moduleSetNumber);
        }
        set = &g_ModuleSets[moduleSetNumber];
        moduleIds = set->moduleIds;
        osWritebackDCacheAll();
        nuPiReadRom(set->romStart, set->vramStart, set->romEnd - set->romStart);
        bzero(set->bssStart, set->bssEnd - set->bssStart);
        osWritebackDCache(set->bssStart, set->bssEnd - set->bssStart);
        osInvalICache(set->vramStart, set->textEnd - set->vramStart);
        if (set->onLoad != NULL) {
            set->onLoad();
        }
        moduleIndex = 0;
        while (moduleIds[moduleIndex] != MODULE_ID_END) {
            g_IsModuleAlive[moduleIds[moduleIndex++]] = TRUE;
        }
        set->isLoaded = TRUE;
        if (!IsDebugFlagSet(DEBUG_FLAG_QUIET_OVERLAY_LOG)) {
            osSyncPrintf("Complete Load ModuleSet %x (absolute No.)\n", moduleSetNumber);
        }
        return TRUE;
    } else {
        if (!IsDebugFlagSet(DEBUG_FLAG_QUIET_OVERLAY_LOG)) {
            osSyncPrintf("Can't Load ModuleSet %x (logical No.)\n", moduleSetNumber);
        }
        return FALSE;
    }
}

void UnloadModuleSet(s32 moduleSetNumber) {
    ModuleSet *set;
    s32 moduleIndex;
    s8 *moduleIds;

    moduleSetNumber = GetModuleSetIndex(moduleSetNumber);
    set = &g_ModuleSets[moduleSetNumber];
    if (set->isLoaded) {
        if (set->onUnload != NULL) {
            set->onUnload();
        }
        moduleIds = set->moduleIds;
        set->isLoaded = FALSE;
        if (!IsDebugFlagSet(DEBUG_FLAG_QUIET_OVERLAY_LOG)) {
            osSyncPrintf("Moduleset %x Disposed\n", moduleSetNumber);
        }
        moduleIndex = 0;
        while (moduleIds[moduleIndex] != MODULE_ID_END) {
            g_IsModuleAlive[moduleIds[moduleIndex++]] = FALSE;
        }
        osWritebackDCacheAll();
        /* Clear the code so that stale calls into it fail early */
        if (g_IsDebugEnabled && osVirtualToPhysical(set->vramStart) < BASE_RDRAM_SIZE) {
            bzero(set->vramStart, set->textEnd - set->vramStart);
            osWritebackDCache(set->vramStart, set->textEnd - set->vramStart);
            osInvalICache(set->vramStart, set->textEnd - set->vramStart);
        }
    }
}

/* The module number is missing from the format's arguments in the original */
s32 ReportModuleNotAlive(void) {
    if (!IsDebugFlagSet(DEBUG_FLAG_QUIET_OVERLAY_LOG)) {
        osSyncPrintf("Module %d not alive !!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }
    if (!IsDebugFlagSet(DEBUG_FLAG_QUIET_OVERLAY_LOG)) {
        osSyncPrintf("Module %d not alive !!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }
    return 0;
}
