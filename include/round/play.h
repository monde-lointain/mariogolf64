/*
 * Play: the players and their order, the holes and what comes between them,
 * replays, the fog and the ground.
 */

#ifndef PLAY_H
#define PLAY_H

#include <ultra64.h>

#include "camera/camera.h"
#include "math/math_util.h"
#include "round/player.h"
#include "save/play_record.h"
#include "shot/ball_simulation.h"
#include "shot/current_shot.h"
#include "system/c_language.h"

// g_ShotFlags's bits, which UpdateShotFlags sets for the shot being set up
#define SHOT_FLAG_ON_GREEN 1
// For a putter, or in mini golf
#define SHOT_FLAG_NO_IMPACT_POINT 2
#define SHOT_FLAG_APPROACH 4
// Set while the ball is at most turn.c's NEAR_CUP_MAX_DECIMETERS from the
// cup
#define SHOT_FLAG_NEAR_CUP 8
typedef s32 ShotFlags;

// How InitPlay sets up the game state before play starts
enum PlayKind {
    // One player's hole of Stroke, or of Ring Shot under GAME_FLAG_INIT_RING_SHOT
    PLAY_KIND_HOLE,
    // A round whose mode and players the GAME_FLAG_ROUND_KIND bits pick
    PLAY_KIND_FLAGGED_ROUND,
    // The hole of the Best Shot replay that f_BestShotReplayRecord holds
    PLAY_KIND_BEST_SHOT_REPLAY,
    // Loads no game state and skips PrepareHoleDisplay, as the menus have set
    // them up already
    PLAY_KIND_WITHOUT_HOLE_DISPLAY
};

typedef s32 PlayKind;

void InitPlay(PlayKind kind);

// Which pause screen is shown over the round
enum PauseScreen {
    PAUSE_SCREEN_NONE,
    PAUSE_SCREEN_PAUSE_MENU,
    PAUSE_SCREEN_SCORECARD,
    // Does nothing; nothing sets it
    PAUSE_SCREEN_UNUSED,
    PAUSE_SCREEN_FREE_LOOK,
    PAUSE_SCREEN_SAVE_MENU,
    PAUSE_SCREEN_PROMPT,
    // Waits for MODULE_SCORECARD, then opens the scorecard
    PAUSE_SCREEN_LOADING_SCORECARD,
    // Waits for MODULE_SAVE_MENU, then opens the save menu
    PAUSE_SCREEN_LOADING_SAVE_MENU,
    // Waits for MODULE_SAVE_MENU, then opens the controls screen
    PAUSE_SCREEN_LOADING_CONTROLS,
    PAUSE_SCREEN_CONTROLS
};

typedef s32 PauseScreen;

// g_LetterboxState's values: the aim camera's views, which all but the
// aiming view letterbox
enum LetterboxState {
    LETTERBOX_STATE_NONE,
    // The player moves the camera, or free look is on
    LETTERBOX_STATE_MOVING_CAMERA,
    LETTERBOX_STATE_LOOKING_DOWN_RANGE,
    // The first frame of looking back at the ball from beyond the cup
    LETTERBOX_STATE_STARTING_LOOK_BACK,
    LETTERBOX_STATE_LOOKING_BACK,
    // Nothing sets it. Every test treats it as the unletterboxed aiming view,
    // except that off the green the HUD animates the offset marker in it rather
    // than lighting the club head, as while the impact point is set.
    LETTERBOX_STATE_UNUSED_AIMING
};

typedef s32 LetterboxState;

// Whether a LetterboxState letterboxes the view. The original tests with &
// rather than &&
#define IsLetterboxState(state) \
    (((state) != LETTERBOX_STATE_NONE) & ((state) != LETTERBOX_STATE_UNUSED_AIMING))

// The state of the black bars that grow over the top and bottom of the
// HUD's area: the turn steps it and the round HUD draws the bars from it
extern LetterboxState g_LetterboxState;
// A PAUSE_SCREEN_* value
extern PauseScreen g_PauseScreen;

// Keeps the last heading for the next shot, in Training
extern Boolean8 g_IsTrainingHeadingKept;
// Set while EaseAimCamera moves the aim camera in place of the player
extern Boolean g_IsAimCameraEasing;
// Set once A has been pressed to take the shot
extern Boolean g_HasPressedShotButton;
// A heading that TurnAim can turn in place of the shot's
extern SignedBinaryAngle g_LookBackHeading;
// The free-look camera's heading, in binary angle units
extern SignedBinaryAngle g_FreeLookHeading;
// Frames, up to shot_labels.c's PIN_FLAG_HIDE_FRAME, since the shot was set up
extern s32 g_ShotSetupFrameCount;
// Frames, up to turn.c's IMPACT_POINT_LINGER_FRAMES, since Z or L was let go
extern s32 g_ImpactPointLingerFrameCount;
// SHOT_FLAG_* bits
extern ShotFlags g_ShotFlags;
// Cleared on the tee, and set once a shot of the hole has come to rest or the
// hole has been given up; the hole's first aim skips
// PLAYER_STATE_PREPARING_AIM
extern Boolean g_HasShotComeToRest;
// Asks for the HUD to turn away when the next player is picked
extern Boolean8 g_IsHudTurnAwayRequested;
// The shot a turn plays without the player: a replayed shot, or the
// computer golfer's finished plan
extern CurrentShot g_ScriptedShot;
// The strokes the replayed record took, after which the replay ends
extern s8 g_ReplayStrokeCount;

// The states of PlayContestMovies
enum ContestMovieState {
    CONTEST_MOVIES_DONE = -1,
    CONTEST_MOVIES_WAITING = 0,
    CONTEST_MOVIES_PLAYING = 60
};

typedef s8 ContestMovieState;

// A CONTEST_MOVIES_* value
extern ContestMovieState g_ContestMovieState;
extern u32 g_GroundDrawFrame;
extern CurrentShot g_SavedShots[PLAYER_COUNT_MAX];
extern Player g_SavedPlayers[PLAYER_COUNT_MAX];
// Read by no routine: UpdateTurns copies f_ShotLaunch here each frame
extern ShotLaunch g_LastShotLaunch;

// FillGroundNoise's random intensities run from a base up through a range,
// capped at MAX_GROUND_NOISE_INTENSITY. Most courses take the default; Shy
// Guy Desert and Yoshi's Island take a lighter base, and mini golf a lighter
// and narrower spread.
#define GROUND_NOISE_RANGE 0x60
#define GROUND_NOISE_BASE 0xBE
#define LIGHT_GROUND_NOISE_BASE 0xC8
#define MINI_GOLF_GROUND_NOISE_RANGE 0x40
#define MINI_GOLF_GROUND_NOISE_BASE 0xD2

void FillGroundNoise(s32 range, s32 base);
void CopyDoublePair(f64 *from, f64 *to);
s32 GetGaugeFramePower(s32 gaugeFrame);
void LoadBestShotReplayInputs(s32 index);
void LoadRingShotReplayInputs(s32 index);
void MarkNearbyGroundTiles(u32 x, u32 z);
void QuitPlayToTitle(void);
void RebuildGroundTextureCoordinates(void);
void ResetShotSetup(void);
void PlacePlayersOnTee(void);
void InitExpansionPakFeatures(void);
// Sets the fog factor and fog color
void SetSceneFog(Gfx **displayListHead);
void UpdatePlay(void);

void DrawGround(CameraMatrices *matrices, s32 terrainStyle);
s32 GetAimCameraGround(s32 x, s32 z);
s32 GetGaugeFill(void);
s32 GetRecordedScoreKind(s32 strokes);
void LoadBestShotReplay(PlayRecord *record);
void LoadRingShotRecord(RingShotRecord *record);
s32 MarkUsableClubs(s32 unused, s16 *clubs, Boolean8 *isUsable);
void SetRingShotCleared(Boolean8 isCleared);

#endif
