#ifndef OVERLAY_MANAGER_H
#define OVERLAY_MANAGER_H

#include <ultra64.h>

/*
 * The game's code outside the main segment is split into modules, each
 * identified by a number below MODULE_COUNT. Modules reach memory inside
 * overlays: segments of the ROM, each holding the code and data of several
 * modules, that are loaded on demand to fixed addresses. Overlays whose
 * address ranges overlap exclude one another. The first three entries of
 * the overlay table all load segment ovl_00, each with its own state.
 *
 * Callers name an overlay by its ID, which is its index in the table plus
 * OVERLAY_ID_BASE.
 */

#define MODULE_COUNT 64
#define OVERLAY_COUNT 17
#define OVERLAY_ID_BASE 0x100
#define MODULE_ID_END (-1)

/* The module that holds the golf balls' afterimages */
#define MODULE_UNK1 1

typedef struct Overlay {
  /* 0x00 */ s8 isLoaded;
  /* 0x04 */ s8* moduleIds; /* Terminated by MODULE_ID_END */
  /* 0x08 */ u32 romStart;
  /* 0x0C */ u32 romEnd;
  /* 0x10 */ u8* vramStart;
  /* 0x14 */ u8* textEnd;
  /* 0x18 */ u8* bssStart;
  /* 0x1C */ u8* bssEnd;
  /* 0x20 */ void (*onLoad)(void);
  /* 0x24 */ void (*onUnload)(void);
} Overlay; /* size = 0x28 */

void OnLoadOverlay2(void);
void OnLoadOverlay7(void);
s32 GetOverlayOfModule(s32 moduleId);
void InitOverlayManager(void);
s32 GetOverlayIndex(s32 overlayId);
s32 CanLoadOverlay(s32 overlayId);
s32 LoadOverlay(s32 overlayId);
void UnloadOverlay(s32 overlayId);
s32 ReportModuleNotAlive(s32 moduleId);

extern s8 g_IsModuleAlive[MODULE_COUNT];
extern s32 g_ModuleRdpRequestFrames[MODULE_COUNT];

#endif
