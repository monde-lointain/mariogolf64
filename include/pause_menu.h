#ifndef PAUSE_MENU_H
#define PAUSE_MENU_H

#include <ultra64.h>

#include "controller.h"

/* The choice boxes that OpenChoiceBox opens */
enum ChoiceBoxId {
  /* In Training, after a shot that leaves the ball out of the cup */
  CHOICE_BOX_ID_TRAINING_SHOT,
  /* In Training, after the reaction that ends the hole */
  CHOICE_BOX_ID_TRAINING_HOLE_END,
  /* In Ring Shot, after the reaction that ends the hole */
  CHOICE_BOX_ID_RING_SHOT,
  /* In Training on the driving range, in place of both Training boxes */
  CHOICE_BOX_ID_DRIVING_RANGE
};

/* What UpdateChoiceBox returns, by the choice made */
enum ChoiceResult {
  /* Until a choice is made */
  CHOICE_RESULT_NONE = -1,
  /* Hole Out, or Continue once the hole has ended */
  CHOICE_RESULT_HOLE_OUT = 2,
  /* Continue after a shot */
  CHOICE_RESULT_CONTINUE,
  CHOICE_RESULT_MULLIGAN,
  CHOICE_RESULT_START_OVER,
  /* Hole Out in Ring Shot */
  CHOICE_RESULT_RING_SHOT_HOLE_OUT,
  CHOICE_RESULT_RETURN_TO_MENU
};

/* Where the panels of the lie and of the wind slide to beside the menu */
extern f32 g_LiePanelPosition[2];
extern f32 g_WindPanelPosition[2];

void DrawPauseArrows(Gfx** displayListHead, s32 x, s32 y, s32 gap);
void OpenChoiceBox(s32 choiceBoxIndex);
s32 UpdateChoiceBox(ControllerState* controller);

#endif
