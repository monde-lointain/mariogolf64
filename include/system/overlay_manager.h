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

#ifndef OVERLAY_MANAGER_H
#define OVERLAY_MANAGER_H

#include <ultra64.h>

#include "system/c_language.h"

#define MODULE_COUNT 64
#define OVERLAY_COUNT 17
#define OVERLAY_ID_BASE 0x100
// Ends an overlay's list of module IDs
#define MODULE_ID_END (-1)

// Module IDs. A module is named for the overlay that f_Overlays puts it in or
// for the code that checks it. A MODULE_UNUSED_ module shares the overlay
// of the module it is named after, and no code names it outside f_Overlays'
// lists.
enum ModuleId {
    // Holds the golf balls and their afterimages (golf_balls.c)
    MODULE_GOLF_BALLS = 0x01,
    MODULE_MENU,
    MODULE_OPENING,
    MODULE_TITLE_SCREEN,
    MODULE_SCORECARD,
    MODULE_HOLE_BUILDER,
    MODULE_COMPUTER_GOLFER,
    MODULE_SHOT_SIMULATION,
    // Holds the Boo that the wind indicator draws (golf_balls.c's data)
    MODULE_WIND_BOO,
    MODULE_PLAY,
    MODULE_HOLE_OBJECTS,
    MODULE_BOOT_LOGOS,
    MODULE_CONFIRM_WAIT,
    MODULE_UNUSED_SCORECARD,
    MODULE_ROUND_RESULTS,
    MODULE_UNUSED_OPENING_A,
    MODULE_ROUND_EVENTS,
    MODULE_SHOT_EFFECTS,
    MODULE_UNUSED_OPENING_B,
    MODULE_PAUSE_MENU,
    MODULE_COURSE_SELECT,
    MODULE_SAVE_MENU,
    MODULE_SHOT_LABELS,
    MODULE_UNUSED_PLAY,
    MODULE_CLUB_PANEL,
    MODULE_CLUB_SLOTS,
    MODULE_TRANSMIT,
    MODULE_UNUSED_HOLE_BUILDER,
    MODULE_UNUSED_TRANSMIT_A,
    MODULE_UNUSED_TITLE_SCREEN,
    MODULE_STAFF_ROLL,
    MODULE_UNUSED_TRANSMIT_B
};

typedef s32 ModuleId;
// A module ID as an overlay's list stores it
typedef s8 ModuleId8;

// An entry of the overlay table: the segment it loads and the modules in it
typedef struct Overlay {
    Boolean8 isLoaded;
    ModuleId8 *moduleIds; // Terminated by MODULE_ID_END
    u32 romStart;
    u32 romEnd;
    u8 *vramStart;
    u8 *textEnd;
    u8 *bssStart;
    u8 *bssEnd;
    void (*onLoad)(void);
    void (*onUnload)(void);
} Overlay;

// Whether each module's overlay is loaded
extern Boolean8 g_IsModuleAlive[MODULE_COUNT];
extern s32 g_ModuleRdpRequestFrames[MODULE_COUNT];

void LoadSceneryResources(void);
void LoadOpeningResources(void);
s32 GetOverlayOfModule(ModuleId moduleId);
void InitOverlayManager(void);
s32 GetOverlayIndex(s32 overlayId);
Boolean CanLoadOverlay(s32 overlayId);
Boolean LoadOverlay(s32 overlayId);
void UnloadOverlay(s32 overlayId);
Boolean ReportModuleNotAlive(ModuleId moduleId);

#endif
