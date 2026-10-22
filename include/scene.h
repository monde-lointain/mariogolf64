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
 * The mode in which AddBirdieBadge counts badges and RecordTournamentPlace
 * reads the 30 contestants; the walkthrough's Tournament is the one mode
 * with birdie badges and 30 contestants
 */
#define GAME_MODE_TOURNAMENT 5
/* The mode whose holes GetCourseRecordIndex takes from g_RingShotHoles */
#define GAME_MODE_RING_SHOT 9

/* The kind of game the player chose, such as a tournament or a match */
extern u32 g_GameMode;

void UpdateOverlays(void);
s32 IsSubModuleLoaded(void);
void RequestSubModule(void);
void UnloadSubModule(void);

#endif
