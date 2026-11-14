#ifndef SCENE_H
#define SCENE_H

#include <ultra64.h>

#include "c_language.h"
#include "overlay_manager.h"

enum Scene {
    SCENE_MAP_OVERVIEW = 0,
    SCENE_HEIGHTS = 1,
    SCENE_GROUND_CAMERA = 2,
    SCENE_TREES = 3,
    SCENE_PIN = 5,
    SCENE_PLAY_HOLE = 7,
    SCENE_FLAG_PANEL = 8,
    SCENE_CELL_EDITOR = 9,
    SCENE_POINT_EDITOR = 10,
    SCENE_HOLE_FILES = 11,
    SCENE_PLAY_ROUND = 12,
    SCENE_PLAY_BEST_SHOT_REPLAY = 13,
    SCENE_MENU = 14,
    SCENE_TRIANGLES = 15,
    SCENE_RINGS = 16,
    SCENE_UNUSED_BLANK_A = 17,
    SCENE_UNUSED_BLANK_B = 18,
    SCENE_OPENING = 19,
    SCENE_UNUSED_UNDRAWN_A = 20,
    SCENE_TITLE_SCREEN = 21,
    SCENE_UNUSED_UNDRAWN_B = 22,
    SCENE_SWAPPING_MODULES = 25,
    SCENE_CONFIRM_WAIT = 26,
    SCENE_BOOT_LOGOS = 27,
    SCENE_WALLS = 28,
    SCENE_TRANSMIT = 29,
    SCENE_STAFF_ROLL = 30
};

typedef s32 Scene;

#define IsGroundEditorScene(scene) ((scene) == SCENE_CELL_EDITOR || (scene) == SCENE_POINT_EDITOR)

enum SubModuleState {
    SUB_MODULE_LOADED,
    SUB_MODULE_SWAP_PENDING,
    SUB_MODULE_UNLOADED = -1
};

typedef s32 SubModuleState;

extern Scene g_Scene;
extern Scene g_NextScene;
extern ModuleId g_SceneModuleToUnload;
extern ModuleId g_SceneModuleToLoad;
extern ModuleId g_SubModuleId;
extern ModuleId g_PendingSubModuleId;
extern SubModuleState g_SubModuleState;

enum GameMode {
    GAME_MODE_STROKE = 0,
    GAME_MODE_UNLISTED_STROKE = 1,
    GAME_MODE_MATCH_GAME = 2,
    GAME_MODE_TRAINING = 3,
    GAME_MODE_GET_CHARACTER = 4,
    GAME_MODE_TOURNAMENT = 5,
    GAME_MODE_UNRECORDED_STROKE = 6,
    GAME_MODE_SPEED_GOLF = 7,
    GAME_MODE_RECORD_REPLAY = 8,
    GAME_MODE_RING_SHOT = 9,
    GAME_MODE_SKINS_MATCH = 10,
    GAME_MODE_CLUB_SLOTS = 11,
    GAME_MODE_TITLE_HOLE = 12,
    GAME_MODE_STAFF_ROLL = 13
};

typedef u32 GameMode;

#define IsOneOnOneMatchMode(mode) \
    ((mode) == GAME_MODE_MATCH_GAME || (mode) == GAME_MODE_GET_CHARACTER)

enum TitleScreenEntrance {
    TITLE_ENTRANCE_NONE,
    TITLE_ENTRANCE_FROM_OPENING,
    TITLE_ENTRANCE_FROM_BOOT_LOGOS,
    TITLE_ENTRANCE_SHOWN
};

typedef s32 TitleScreenEntrance;

extern TitleScreenEntrance g_TitleScreenEntrance;

void UpdateOverlays(void);
Boolean IsSubModuleLoaded(void);
void RequestSubModule(void);
void UnloadSubModule(void);

#endif
