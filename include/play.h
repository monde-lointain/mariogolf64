#ifndef PLAY_H
#define PLAY_H

#include "common.h"

/* g_ShotFlags's bits, which UpdateShotFlags sets for the shot being set up */
#define SHOT_FLAG_ON_GREEN 1
#define SHOT_FLAG_NO_IMPACT_POINT 2 /* For a putter, or in mini golf */
#define SHOT_FLAG_APPROACH 4
/* Set while g_CurrentShot.unk50 is at most turn.c's SHOT_UNK8_MAX_UNK50 */
#define SHOT_FLAG_UNK8 8

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

/* Keeps the last heading for the next shot, in Training */
extern s8 g_IsTrainingHeadingKept;
extern s32 D_800C3070;
/* Set once A has been pressed to take the shot */
extern s32 g_HasPressedShotButton;
/* A heading that TurnAim can turn in place of the shot's */
extern s16 g_AltHeading;
/* The free-look camera's heading, in binary angle units */
extern s16 g_FreeLookHeading;
/* Frames, up to shot_labels.c's PIN_FLAG_HIDE_FRAME, since the shot was set up
 */
extern s32 g_ShotSetupFrameCount;
/* Frames, up to turn.c's IMPACT_POINT_LINGER_FRAMES, since Z or L was let go */
extern s32 g_ImpactPointLingerFrameCount;
/* SHOT_FLAG_* bits */
extern s32 g_ShotFlags;
extern s32 D_800C3094;
/* Asks for the HUD to turn away when the next player is picked */
extern s8 g_IsHudTurnAwayRequested;

#endif
