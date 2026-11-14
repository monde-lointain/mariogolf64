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

#ifndef SCENE_H
#define SCENE_H

#include <ultra64.h>

#include "system/c_language.h"
#include "system/overlay_manager.h"

// The game's scenes, most of which the debug scene menu offers. The map
// scenes each run one routine
// in the hole_editor overlay, and the hole-editor names below follow the
// menu's labels until those routines are decompiled.
enum Scene {
    SCENE_MAP_OVERVIEW = 0, // Moves the camera over the miniature map
    SCENE_HEIGHTS = 1,
    SCENE_GROUND_CAMERA = 2, // Flies the camera just above the ground
    SCENE_TREES = 3,
    SCENE_PIN = 5,
    SCENE_PLAY_HOLE = 7, // Picks the conditions and starts a hole
    SCENE_FLAG_PANEL = 8,
    // A top-down map where the editor toggles the ground cells' bits
    SCENE_CELL_EDITOR = 9,
    // A top-down map where the editor moves the ground points
    SCENE_POINT_EDITOR = 10,
    // A debug menu that loads a hole or saves the hole in memory
    SCENE_HOLE_FILES = 11,
    // The scene of a round the menu has loaded the hole for. It updates like
    // SCENE_PLAY_HOLE, but UpdateDebugScenes doesn't label it.
    SCENE_PLAY_ROUND = 12,
    // The scene menu's scene that replays the hole of f_BestShotReplayRecord,
    // labeled "S-SHOT". It updates like SCENE_PLAY_HOLE.
    SCENE_PLAY_BEST_SHOT_REPLAY = 13,
    SCENE_MENU = 14,
    SCENE_TRIANGLES = 15,
    SCENE_RINGS = 16,
    // No code enters SCENE_UNUSED_BLANK_A, SCENE_UNUSED_BLANK_B,
    // SCENE_UNUSED_UNDRAWN_A or SCENE_UNUSED_UNDRAWN_B. DrawGame alone names
    // them, filling the screen for the blank ones and drawing nothing for the
    // undrawn ones.
    SCENE_UNUSED_BLANK_A = 17,
    SCENE_UNUSED_BLANK_B = 18,
    SCENE_OPENING = 19, // The opening movie
    SCENE_UNUSED_UNDRAWN_A = 20,
    SCENE_TITLE_SCREEN = 21,
    SCENE_UNUSED_UNDRAWN_B = 22,
    // The current scene while UpdateOverlays swaps the scene's modules
    SCENE_SWAPPING_MODULES = 25,
    // Waits for A or Start, then enters the boot logos; no code found enters it
    SCENE_CONFIRM_WAIT = 26,
    SCENE_BOOT_LOGOS = 27,
    SCENE_WALLS = 28,
    SCENE_TRANSMIT = 29, // Trades data with Mario Golf for Game Boy
    SCENE_STAFF_ROLL = 30
};

typedef s32 Scene;

// Whether a scene is one of the editor's top-down ground maps
#define IsGroundEditorScene(scene) ((scene) == SCENE_CELL_EDITOR || (scene) == SCENE_POINT_EDITOR)

// g_SubModuleState values
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

// The game's modes, as GameState.gameMode holds them. The save menu names
// Stroke, Match Game, Get Character, Skins Match and Club Slots for a saved
// game; Mini-Golf is not a mode, as GameState.isMiniGolf marks it.
enum GameMode {
    GAME_MODE_STROKE = 0,
    // A mode played like Stroke, but chosen with no bet set and over all the
    // holes. Only MODE_ICON_UNLISTED_STROKE starts it, which no mode select list
    // offers; its icon is two scorecards like Stroke's with an "OUT" bubble, and
    // a saved game in it shows Stroke's label. SettleHoleBets settles its bets
    // like Stroke's.
    GAME_MODE_UNLISTED_STROKE = 1,
    GAME_MODE_MATCH_GAME = 2,
    // The walkthrough's Training: it has dry holes, cup positions taken in
    // turn and, for the computer golfer, a planned score of par on each hole.
    // Like Ring Shot it earns no Course Points, and its pause menu has
    // layouts for the driving range and mini golf.
    GAME_MODE_TRAINING = 3,
    GAME_MODE_GET_CHARACTER = 4,
    // The mode in which AddBirdieBadge counts badges and RecordTournamentPlace
    // reads the 30 contestants; the walkthrough's Tournament is the one mode
    // with birdie badges and 30 contestants
    GAME_MODE_TOURNAMENT = 5,
    // A mode no menu sets that is played like Stroke: its scorecard, pause menu
    // layout, results, save slot and bets settled by SettleHoleBets are Stroke's.
    // But entering the course select in it sets no bets, and IsRecordableHoleOut
    // records none of its hole-outs.
    GAME_MODE_UNRECORDED_STROKE = 6,
    // The walkthrough's Speed Golf: the HUD shows g_GameState.stopwatchFrameCount
    // in this mode, and its scorecard page shows each hole's time and the round's
    // score. EndHoleFlyover skips its call for a strong wind in it.
    GAME_MODE_SPEED_GOLF = 7,
    // A mode played as a replay is: SetUpPlayers sets up only its first player,
    // from a record, ChooseHoleConditions takes its hole from g_PlayRecords[0],
    // it has no pause menu layout, the HUD leaves the players' totals blank, and
    // UpdatePlay quits to the title after the hole. The decompiled code sets it
    // only by copying a loaded record's mode.
    GAME_MODE_RECORD_REPLAY = 8,
    // The mode whose holes GetCourseRecordIndex takes from f_RingShotHoles
    GAME_MODE_RING_SHOT = 9,
    GAME_MODE_SKINS_MATCH = 10,
    GAME_MODE_CLUB_SLOTS = 11,
    // Set by SetUpTitleHole for the hole behind the title screen, which is
    // built from assets of its own
    GAME_MODE_TITLE_HOLE = 12,
    // Set by the staff roll for the hole its cast plays
    GAME_MODE_STAFF_ROLL = 13
};

typedef u32 GameMode;

// Whether a game mode is a one-on-one match: Match Game or Get Character
#define IsOneOnOneMatchMode(mode) \
    ((mode) == GAME_MODE_MATCH_GAME || (mode) == GAME_MODE_GET_CHARACTER)

// The scene that last led to the title screen. The title screen fades in from
// white after the opening and from black otherwise.
enum TitleScreenEntrance {
    // Neither entrance has set it yet
    TITLE_ENTRANCE_NONE,
    TITLE_ENTRANCE_FROM_OPENING,
    TITLE_ENTRANCE_FROM_BOOT_LOGOS,
    // Set as the title screen fades in, so that any later return is from black
    TITLE_ENTRANCE_SHOWN
};

typedef s32 TitleScreenEntrance;
extern TitleScreenEntrance g_TitleScreenEntrance;

void UpdateOverlays(void);
Boolean IsSubModuleLoaded(void);
void RequestSubModule(void);
void UnloadSubModule(void);

#endif
