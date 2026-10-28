#ifndef PLAY_H
#define PLAY_H

#include "common.h"

/* g_LetterboxState's values */
enum LetterboxState {
  LETTERBOX_STATE_NONE,
  LETTERBOX_STATE_UNK1,
  LETTERBOX_STATE_UNK2,
  LETTERBOX_STATE_UNK3,
  LETTERBOX_STATE_UNK4,
  LETTERBOX_STATE_UNK5
};

/*
 * The state of the black bars that grow over the top and bottom of the
 * HUD's area: the turn steps it and the round HUD draws the bars from it
 */
extern s32 g_LetterboxState;

#endif
