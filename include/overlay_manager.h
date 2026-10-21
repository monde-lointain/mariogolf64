#ifndef OVERLAY_MANAGER_H
#define OVERLAY_MANAGER_H

#include <ultra64.h>

/*
 * The game's code outside the main segment is split into modules, each
 * identified by a number below MODULE_COUNT. A module set is an overlay:
 * one segment of the ROM holding the code and data of several modules,
 * loaded to a fixed address on demand. Sets that share an address
 * exclude one another.
 *
 * Callers name a module set by its "absolute" number, the set's index
 * plus MODULE_SET_NUMBER_BASE.
 */

#define MODULE_COUNT 64
#define MODULE_SET_COUNT 17
#define MODULE_SET_NUMBER_BASE 0x100
#define MODULE_ID_END (-1)

typedef struct ModuleSet {
    /* 0x00 */ s8 isLoaded;
    /* 0x04 */ s8 *moduleIds; /* Terminated by MODULE_ID_END */
    /* 0x08 */ u32 romStart;
    /* 0x0C */ u32 romEnd;
    /* 0x10 */ u8 *vramStart;
    /* 0x14 */ u8 *textEnd;
    /* 0x18 */ u8 *bssStart;
    /* 0x1C */ u8 *bssEnd;
    /* 0x20 */ void (*onLoad)(void);
    /* 0x24 */ void (*onUnload)(void);
} ModuleSet; /* size = 0x28 */

void OnLoadModuleSet2(void);
void OnLoadModuleSet7(void);
s32 GetModuleSetOfModule(s32 moduleId);
void InitOverlayManager(void);
s32 GetModuleSetIndex(s32 moduleSetNumber);
s32 CanLoadModuleSet(s32 moduleSetNumber);
s32 LoadModuleSet(s32 moduleSetNumber);
void UnloadModuleSet(s32 moduleSetNumber);
s32 ReportModuleNotAlive(void);

extern ModuleSet g_ModuleSets[MODULE_SET_COUNT];
extern u8 g_IsModuleAlive[MODULE_COUNT];
extern s32 g_ModuleRdpRequestFrames[MODULE_COUNT];

#endif
