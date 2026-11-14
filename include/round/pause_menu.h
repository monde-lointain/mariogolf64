/*
 * The pause menu over a round, and the choice box of a turn.
 */

#ifndef PAUSE_MENU_H
#define PAUSE_MENU_H

#include <ultra64.h>

#include "controllers/controller.h"
#include "math/geometry.h"
#include "system/c_language.h"

// What OpenSaveMenu lists, and f_MenuKind
enum SaveMenuList {
    SAVE_MENU_CLOSED,
    SAVE_MENU_GAMES,
    SAVE_MENU_REPLAYS
};

typedef s32 SaveMenuList;

// What choosing a pause option does, as its PauseOption.action. Each option's
// action is its own index, and is named for the option's label.
enum PauseAction {
    // "Course View"
    PAUSE_ACTION_FREE_LOOK,
    PAUSE_ACTION_SCORECARD,
    // Only while debugging or with f_IsGiveUpAllowed set
    PAUSE_ACTION_GIVE_UP,
    PAUSE_ACTION_SAVE_AND_QUIT,
    PAUSE_ACTION_QUIT_GAME,
    PAUSE_ACTION_RETURN_TO_MENU,
    PAUSE_ACTION_HOLE_OUT,
    PAUSE_ACTION_CHANGE_WIND_DIRECTION,
    PAUSE_ACTION_CHANGE_WIND_SPEED,
    PAUSE_ACTION_START_OVER,
    PAUSE_ACTION_CHANGE_WEATHER,
    // "Change Terrain", which changes the lie
    PAUSE_ACTION_CHANGE_TERRAIN,
    PAUSE_ACTION_GRID_COLOR,
    PAUSE_ACTION_CONTROLS,
    // "Quit Game" in the PAUSE_LAYOUT_BROUGHT_OVER layouts
    PAUSE_ACTION_QUIT_BROUGHT_OVER_GAME,
    // "Quit Game" in the PAUSE_LAYOUT_CLUB_TOURNAMENT layouts
    PAUSE_ACTION_QUIT_CLUB_TOURNAMENT,
    // UpdatePauseMenu's result while no option is chosen
    PAUSE_ACTION_NONE = -1
};

typedef s32 PauseAction;
typedef s16 PauseAction16;

// The choice boxes that OpenChoiceBox opens
enum ChoiceBoxId {
    // In Training, after a shot that leaves the ball out of the cup
    CHOICE_BOX_ID_TRAINING_SHOT,
    // In Training, after the reaction that ends the hole
    CHOICE_BOX_ID_TRAINING_HOLE_END,
    // In Ring Shot, after the reaction that ends the hole
    CHOICE_BOX_ID_RING_SHOT,
    // In Training on the driving range, in place of both Training boxes
    CHOICE_BOX_ID_DRIVING_RANGE,
    CHOICE_BOX_ID_NONE = -1
};

typedef s32 ChoiceBoxId;

// What UpdateChoiceBox returns, by the choice made
enum ChoiceResult {
    // Until a choice is made
    CHOICE_RESULT_NONE = -1,
    // Hole Out, or Continue once the hole has ended
    CHOICE_RESULT_HOLE_OUT = 2,
    // Continue after a shot
    CHOICE_RESULT_CONTINUE,
    CHOICE_RESULT_MULLIGAN,
    CHOICE_RESULT_START_OVER,
    // Hole Out in Ring Shot
    CHOICE_RESULT_RING_SHOT_HOLE_OUT,
    CHOICE_RESULT_RETURN_TO_MENU
};

typedef s32 ChoiceResult;
typedef u8 UChoiceResult8;

// Where the panels of the lie and of the wind slide to beside the menu
extern Vec2 g_LiePanelPosition;
extern Vec2 g_WindPanelPosition;

void DrawPauseArrows(Gfx **displayListHead, s32 x, s32 y, s32 gap);
#define CLOSING_PAUSE_MENU (-1)
void RequestPauseMenu(Boolean canGiveUp, Boolean isClosing);
PauseAction UpdatePauseMenu(ControllerState *controller);
void OpenChoiceBox(ChoiceBoxId choiceBoxId);
ChoiceResult UpdateChoiceBox(ControllerState *controller);
void CloseSaveMenu(void);
void HideRoundHud(void);
void LeaveGameSaveMenu(void);
void OpenControlsScreen(void);
// kind is a SaveMenuList value
void OpenSaveMenu(SaveMenuList kind);
void ResumeRound(void);
void SaveGameToSlot(void);
void SaveReplayToSlot(void);
void UpdateControlsScreen(ControllerState *controller);
void UpdateGameSaveMenu(ControllerState *controller);

// The channels of the pause and Ring Shot retry sounds
#define PAUSE_SOUND_CHANNEL 2
#define RING_SHOT_RETRY_SOUND_CHANNEL 3

void DrawChoiceBox(Gfx **displayListHead);
void DrawPauseMenu(Gfx **displayListHead);

#endif
