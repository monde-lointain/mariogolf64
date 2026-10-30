#ifndef PLAY_H
#define PLAY_H

#include "common.h"

#include "math_util.h"

/* g_ShotFlags's bits, which UpdateShotFlags sets for the shot being set up */
#define SHOT_FLAG_ON_GREEN 1
/* For a putter, or in mini golf */
#define SHOT_FLAG_NO_IMPACT_POINT 2
#define SHOT_FLAG_APPROACH 4
/* Set while the ball is at most turn.c's NEAR_CUP_MAX_DECIMETERS from the
 * cup */
#define SHOT_FLAG_NEAR_CUP 8

/* How InitPlay sets up the game state before play starts */
enum PlayKind {
  /* One player's hole of Stroke, or of Ring Shot under GAME_FLAG_INIT_RING_SHOT
   */
  PLAY_KIND_HOLE,
  /* A round whose mode and players the GAME_FLAG_ROUND_KIND bits pick */
  PLAY_KIND_FLAGGED_ROUND,
  /* The hole of the Best Shot replay that D_800FED40 holds */
  PLAY_KIND_BEST_SHOT_REPLAY,
  /* Loads no game state and skips PrepareHoleDisplay, as the menus have set
   * them up already */
  PLAY_KIND_WITHOUT_HOLE_DISPLAY
};

typedef s32 PlayKind;

void InitPlay(PlayKind kind);

/* Which pause screen is shown over the round */
enum PauseScreen {
  PAUSE_SCREEN_NONE,
  PAUSE_SCREEN_PAUSE_MENU,
  PAUSE_SCREEN_SCORECARD,
  /* Does nothing; nothing sets it */
  PAUSE_SCREEN_UNK3,
  PAUSE_SCREEN_FREE_LOOK,
  PAUSE_SCREEN_SAVE_MENU,
  PAUSE_SCREEN_PROMPT,
  /* Waits for MODULE_SCORECARD, then opens the scorecard */
  PAUSE_SCREEN_LOADING_SCORECARD,
  /* Waits for MODULE_SAVE_MENU, then opens the save menu */
  PAUSE_SCREEN_LOADING_SAVE_MENU,
  /* Waits for MODULE_SAVE_MENU, then opens the controls screen */
  PAUSE_SCREEN_LOADING_CONTROLS,
  PAUSE_SCREEN_CONTROLS
};

/* g_LetterboxState's values: the aim camera's views, which all but the
 * aiming view letterbox */
enum LetterboxState {
  LETTERBOX_STATE_NONE,
  /* The player moves the camera, or free look is on */
  LETTERBOX_STATE_MOVING_CAMERA,
  LETTERBOX_STATE_LOOKING_DOWN_RANGE,
  /* The first frame of looking back at the ball from beyond the cup */
  LETTERBOX_STATE_STARTING_LOOK_BACK,
  LETTERBOX_STATE_LOOKING_BACK,
  /* Nothing sets it */
  LETTERBOX_STATE_UNK5
};

/*
 * The state of the black bars that grow over the top and bottom of the
 * HUD's area: the turn steps it and the round HUD draws the bars from it
 */
extern s32 g_LetterboxState;
/* A PAUSE_SCREEN_* value */
extern s32 g_PauseScreen;

/* Keeps the last heading for the next shot, in Training */
extern s8 g_IsTrainingHeadingKept;
extern s32 D_800C3070;
/* Set once A has been pressed to take the shot */
extern Boolean g_HasPressedShotButton;
/* A heading that TurnAim can turn in place of the shot's */
extern SignedBinaryAngle g_AltHeading;
/* The free-look camera's heading, in binary angle units */
extern SignedBinaryAngle g_FreeLookHeading;
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

/*
 * FillGroundNoise's random intensities run from a base up through a range,
 * capped at MAX_GROUND_NOISE_INTENSITY. Most courses take the default; Shy
 * Guy Desert and Yoshi's Island take a lighter base, and mini golf a lighter
 * and narrower spread.
 */
#define GROUND_NOISE_RANGE 0x60
#define GROUND_NOISE_BASE 0xBE
#define LIGHT_GROUND_NOISE_BASE 0xC8
#define MINI_GOLF_GROUND_NOISE_RANGE 0x40
#define MINI_GOLF_GROUND_NOISE_BASE 0xD2
#define MAX_GROUND_NOISE_INTENSITY 0xFF

void FillGroundNoise(s32 range, s32 base);

#endif
