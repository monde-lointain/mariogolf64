#ifndef PAUSE_MENU_H
#define PAUSE_MENU_H

#include <ultra64.h>

#include "controller.h"
#include "language.h"

enum SaveMenuList {
    SAVE_MENU_CLOSED,
    SAVE_MENU_GAMES,
    SAVE_MENU_REPLAYS
};

typedef s32 SaveMenuList;

enum PauseAction {
    PAUSE_ACTION_FREE_LOOK,
    PAUSE_ACTION_SCORECARD,
    PAUSE_ACTION_GIVE_UP,
    PAUSE_ACTION_SAVE_AND_QUIT,
    PAUSE_ACTION_QUIT_GAME,
    PAUSE_ACTION_RETURN_TO_MENU,
    PAUSE_ACTION_HOLE_OUT,
    PAUSE_ACTION_CHANGE_WIND_DIRECTION,
    PAUSE_ACTION_CHANGE_WIND_SPEED,
    PAUSE_ACTION_START_OVER,
    PAUSE_ACTION_CHANGE_WEATHER,
    PAUSE_ACTION_CHANGE_TERRAIN,
    PAUSE_ACTION_GRID_COLOR,
    PAUSE_ACTION_CONTROLS,
    PAUSE_ACTION_QUIT_BROUGHT_OVER_GAME,
    PAUSE_ACTION_QUIT_CLUB_TOURNAMENT,
    PAUSE_ACTION_NONE = -1
};

typedef s32 PauseAction;
// A PauseAction in a signed halfword, as the pause options table keeps it.
typedef s16 PauseAction16;

enum ChoiceBoxId {
    CHOICE_BOX_ID_TRAINING_SHOT,
    CHOICE_BOX_ID_TRAINING_HOLE_END,
    CHOICE_BOX_ID_RING_SHOT,
    CHOICE_BOX_ID_DRIVING_RANGE,
    CHOICE_BOX_ID_NONE = -1
};

typedef s32 ChoiceBoxId;

enum ChoiceResult {
    CHOICE_RESULT_NONE = -1,
    CHOICE_RESULT_HOLE_OUT = 2,
    CHOICE_RESULT_CONTINUE,
    CHOICE_RESULT_MULLIGAN,
    CHOICE_RESULT_START_OVER,
    CHOICE_RESULT_RING_SHOT_HOLE_OUT,
    CHOICE_RESULT_RETURN_TO_MENU
};

extern f32 g_LiePanelPosition[2];
extern f32 g_WindPanelPosition[2];

void DrawPauseArrows(Gfx **displayListHead, s32 x, s32 y, s32 gap);
// A true isClosing for RequestPauseMenu; the original passes -1 rather
// than TRUE.
#define CLOSING_PAUSE_MENU (-1)

void RequestPauseMenu(Boolean canGiveUp, Boolean isClosing);
PauseAction UpdatePauseMenu(ControllerState *controller);
void OpenChoiceBox(ChoiceBoxId choiceBoxId);
s32 UpdateChoiceBox(ControllerState *controller);
void CloseSaveMenu(void);
void HideRoundHud(void);
void LeaveGameSaveMenu(void);
void OpenControlsScreen(void);
void OpenSaveMenu(s32 kind);
void ResumeRound(void);
void SaveGameToSlot(void);
void SaveReplayToSlot(void);
void UpdateControlsScreen(ControllerState *controller);
void UpdateGameSaveMenu(ControllerState *controller);

#define PAUSE_SOUND_CHANNEL 2
#define RING_SHOT_RETRY_SOUND_CHANNEL 3

void DrawChoiceBox(Gfx **displayListHead);
void DrawPauseMenu(Gfx **displayListHead);

#endif
