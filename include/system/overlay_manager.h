#ifndef OVERLAY_MANAGER_H
#define OVERLAY_MANAGER_H

#include <ultra64.h>

#include "system/c_language.h"

#define MODULE_COUNT 64
#define OVERLAY_COUNT 17
#define OVERLAY_ID_BASE 0x100
#define MODULE_ID_END (-1)

enum ModuleId {
    MODULE_GOLF_BALLS = 0x01,
    MODULE_MENU,
    MODULE_OPENING,
    MODULE_TITLE_SCREEN,
    MODULE_SCORECARD,
    MODULE_HOLE_BUILDER,
    MODULE_COMPUTER_GOLFER,
    MODULE_SHOT_SIMULATION,
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
typedef s8 ModuleId8;

typedef struct Overlay {
    Boolean8 isLoaded;
    ModuleId8 *moduleIds;
    u32 romStart;
    u32 romEnd;
    u8 *vramStart;
    u8 *textEnd;
    u8 *bssStart;
    u8 *bssEnd;
    void (*onLoad)(void);
    void (*onUnload)(void);
} Overlay;

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
