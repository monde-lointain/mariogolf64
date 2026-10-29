#ifndef SCENE_H
#define SCENE_H

#include <ultra64.h>

/*
 * The game runs one scene at a time: a menu, a round of golf and so on.
 * A scene's code lives in a module. To change scenes, code names the
 * module to unload, the module to load and the scene to enter next, then
 * sets the current scene to SCENE_SWAPPING_MODULES. UpdateOverlays then
 * swaps the modules over the following frames and enters the next scene.
 *
 * Alongside the scene's module, one sub-module can be swapped the same way
 * through RequestSubModule.
 */

/*
 * The game's scenes, most of which the debug scene menu offers. The map
 * scenes each run one routine
 * in the hole_editor overlay, and the hole-editor names below follow the
 * menu's labels until those routines are decompiled.
 */
enum Scene {
  SCENE_MAP_OVERVIEW = 0, /* Moves the camera over the miniature map */
  SCENE_HEIGHTS = 1,
  SCENE_GROUND_CAMERA = 2, /* Flies the camera just above the ground */
  SCENE_TREES = 3,
  SCENE_PIN = 5,
  SCENE_PLAY_HOLE = 7, /* Picks the conditions and starts a hole */
  SCENE_FLAG_PANEL = 8,
  SCENE_UNK9 = 9, /* A top-down map scene */
  SCENE_UNK10 = 10, /* A top-down map scene */
  SCENE_UNK11 = 11,
  SCENE_PLAY_UNK12 = 12, /* Updates like SCENE_PLAY_HOLE */
  SCENE_PLAY_UNK13 = 13, /* Updates like SCENE_PLAY_HOLE */
  SCENE_MENU = 14,
  SCENE_TRIANGLES = 15,
  SCENE_RINGS = 16,
  SCENE_UNK17 = 17,
  SCENE_UNK18 = 18,
  SCENE_OPENING = 19, /* The opening movie */
  SCENE_UNK20 = 20,
  SCENE_TITLE_SCREEN = 21,
  SCENE_UNK22 = 22,
  /* The current scene while UpdateOverlays swaps the scene's modules */
  SCENE_SWAPPING_MODULES = 25,
  /* Waits for A or Start, then enters the boot logos; no code found enters it
   */
  SCENE_CONFIRM_WAIT = 26,
  SCENE_BOOT_LOGOS = 27,
  SCENE_WALLS = 28,
  SCENE_TRANSMIT = 29, /* Trades data with Mario Golf for Game Boy */
  SCENE_STAFF_ROLL = 30
};

/* g_SubModuleState values */
enum SubModuleState {
  SUB_MODULE_LOADED,
  SUB_MODULE_SWAP_PENDING,
  SUB_MODULE_UNLOADED = -1
};

extern s32 g_Scene;
extern s32 g_NextScene;
extern s32 g_SceneModuleToUnload;
extern s32 g_SceneModuleToLoad;
extern s32 g_SubModuleId;
extern s32 g_PendingSubModuleId;
extern s32 g_SubModuleState;

/* The game's modes, as GameState.gameMode holds them */
/*
 * The game's modes. The save menu names Stroke, Match Game, Get Character,
 * Skins Match and Club Slots for a saved game; Mini-Golf is not a mode, as
 * GameState.isMiniGolf marks it.
 */
enum GameMode {
  GAME_MODE_STROKE = 0,
  GAME_MODE_UNK1 = 1, /* SettleHoleBets settles its bets like Stroke's */
  GAME_MODE_MATCH_GAME = 2,
  /*
   * The walkthrough's Training: it has dry holes, cup positions taken in
   * turn and, for the computer golfer, a planned score of par on each hole.
   * Like Ring Shot it earns no Course Points, and its pause menu has
   * layouts for the driving range and mini golf.
   */
  GAME_MODE_TRAINING = 3,
  GAME_MODE_GET_CHARACTER = 4,
  /*
   * The mode in which AddBirdieBadge counts badges and RecordTournamentPlace
   * reads the 30 contestants; the walkthrough's Tournament is the one mode
   * with birdie badges and 30 contestants
   */
  GAME_MODE_TOURNAMENT = 5,
  GAME_MODE_UNK6 = 6, /* SettleHoleBets settles its bets like Stroke's */
  /*
   * The walkthrough's Speed Golf: the HUD shows g_GameState.stopwatchFrameCount
   * in this mode, and its scorecard page shows each hole's time and the round's
   * score. EndHoleFlyover skips its call for a strong wind in it.
   */
  GAME_MODE_SPEED_GOLF = 7,
  /* A mode in which SetUpPlayers sets up the players from a record */
  GAME_MODE_UNK8 = 8,
  /* The mode whose holes GetCourseRecordIndex takes from f_RingShotHoles */
  GAME_MODE_RING_SHOT = 9,
  GAME_MODE_SKINS_MATCH = 10,
  GAME_MODE_CLUB_SLOTS = 11,
  /* A mode in which every hole is built from the same hole assets */
  GAME_MODE_UNK12 = 12,
  /* Set by the staff roll for the hole its cast plays */
  GAME_MODE_STAFF_ROLL = 13
};

/*
 * The scene that last led to the title screen. The title screen fades in from
 * white after the opening and from black otherwise.
 */
typedef enum TitleScreenEntrance {
  /* Neither entrance has set it yet */
  TITLE_ENTRANCE_NONE,
  TITLE_ENTRANCE_FROM_OPENING,
  TITLE_ENTRANCE_FROM_BOOT_LOGOS,
  /* Set as the title screen fades in, so that any later return is from black */
  TITLE_ENTRANCE_SHOWN
} TitleScreenEntrance;

extern s32 g_TitleScreenEntrance;

void UpdateOverlays(void);
Boolean IsSubModuleLoaded(void);
void RequestSubModule(void);
void UnloadSubModule(void);

#endif
