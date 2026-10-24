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
 * Scenes the debug scene menu offers. The map scenes each run one routine
 * in the hole_editor overlay, and the hole-editor names below follow the
 * menu's labels until those routines are decompiled.
 */
#define SCENE_MAP_OVERVIEW 0    /* Moves the camera over the miniature map */
#define SCENE_HEIGHTS 1
#define SCENE_GROUND_CAMERA 2   /* Flies the camera just above the ground */
#define SCENE_TREES 3
#define SCENE_PIN 5
#define SCENE_PLAY_HOLE 7       /* Picks the conditions and starts a hole */
#define SCENE_FLAG_PANEL 8
#define SCENE_UNK9 9            /* A top-down map scene */
#define SCENE_UNK10 10          /* A top-down map scene */
#define SCENE_UNK11 11
#define SCENE_PLAY_UNK12 12     /* Updates like SCENE_PLAY_HOLE */
#define SCENE_PLAY_UNK13 13     /* Updates like SCENE_PLAY_HOLE */
#define SCENE_MENU 14
#define SCENE_TITLE_SCREEN 21
#define SCENE_TRIANGLES 15
#define SCENE_RINGS 16
#define SCENE_BOOT_LOGOS 27
#define SCENE_WALLS 28
#define SCENE_STAFF_ROLL 30

/* The current scene while UpdateOverlays swaps the scene's modules */
#define SCENE_SWAPPING_MODULES 0x19

/* g_SubModuleState values */
#define SUB_MODULE_LOADED 0
#define SUB_MODULE_SWAP_PENDING 1
#define SUB_MODULE_UNLOADED (-1)

extern s32 g_Scene;
extern s32 g_NextScene;
extern s32 g_SceneModuleToUnload;
extern s32 g_SceneModuleToLoad;
extern s32 g_SubModuleId;
extern s32 g_PendingSubModuleId;
extern s32 g_SubModuleState;

/*
 * The modes that the save menu names for a saved game. Mini-Golf is not one
 * of them; GameState.isMiniGolf marks it.
 */
#define GAME_MODE_STROKE 0
#define GAME_MODE_MATCH_GAME 2
#define GAME_MODE_GET_CHARACTER 4
#define GAME_MODE_SKINS_MATCH 10
#define GAME_MODE_CLUB_SLOTS 11

/*
 * The mode in which AddBirdieBadge counts badges and RecordTournamentPlace
 * reads the 30 contestants; the walkthrough's Tournament is the one mode
 * with birdie badges and 30 contestants
 */
#define GAME_MODE_TOURNAMENT 5
/* The mode whose holes GetCourseRecordIndex takes from g_RingShotHoles */
#define GAME_MODE_RING_SHOT 9
/* Set by the staff roll for the hole its cast plays */
#define GAME_MODE_STAFF_ROLL 13

/* Modes whose hole bets SettleHoleBets settles like Stroke's */
#define GAME_MODE_UNK1 1
#define GAME_MODE_UNK6 6

/* A mode in which EndHoleFlyover skips its call for a strong wind */
#define GAME_MODE_UNK7 7

/* A mode in which SetUpPlayers sets up the players from a record */
#define GAME_MODE_UNK8 8

/*
 * A mode with dry holes, cup positions taken in turn and, for the computer
 * golfer, a planned score of par on each hole
 */
#define GAME_MODE_UNK3 3

/* A mode in which every hole is built from the same hole assets */
#define GAME_MODE_UNK12 12

/* The kind of game the player chose, such as a tournament or a match */
extern u32 g_GameMode;

void UpdateOverlays(void);
s32 IsSubModuleLoaded(void);
void RequestSubModule(void);
void UnloadSubModule(void);

#endif
