#ifndef PLAY_H
#define PLAY_H

#include <ultra64.h>

#include "ball_simulation.h"
#include "camera.h"
#include "current_shot.h"
#include "language.h"
#include "math_util.h"
#include "play_record.h"
#include "player.h"

#define SHOT_FLAG_ON_GREEN 1
#define SHOT_FLAG_NO_IMPACT_POINT 2
#define SHOT_FLAG_APPROACH 4
#define SHOT_FLAG_NEAR_CUP 8
typedef s32 ShotFlags;

enum PlayKind {
    PLAY_KIND_HOLE,
    PLAY_KIND_FLAGGED_ROUND,
    PLAY_KIND_BEST_SHOT_REPLAY,
    PLAY_KIND_WITHOUT_HOLE_DISPLAY
};

typedef s32 PlayKind;

void InitPlay(PlayKind kind);

enum PauseScreen {
    PAUSE_SCREEN_NONE,
    PAUSE_SCREEN_PAUSE_MENU,
    PAUSE_SCREEN_SCORECARD,
    PAUSE_SCREEN_UNUSED,
    PAUSE_SCREEN_FREE_LOOK,
    PAUSE_SCREEN_SAVE_MENU,
    PAUSE_SCREEN_PROMPT,
    PAUSE_SCREEN_LOADING_SCORECARD,
    PAUSE_SCREEN_LOADING_SAVE_MENU,
    PAUSE_SCREEN_LOADING_CONTROLS,
    PAUSE_SCREEN_CONTROLS
};

typedef s32 PauseScreen;

enum LetterboxState {
    LETTERBOX_STATE_NONE,
    LETTERBOX_STATE_MOVING_CAMERA,
    LETTERBOX_STATE_LOOKING_DOWN_RANGE,
    LETTERBOX_STATE_STARTING_LOOK_BACK,
    LETTERBOX_STATE_LOOKING_BACK,
    LETTERBOX_STATE_UNUSED_AIMING
};

typedef s32 LetterboxState;

#define IsLetterboxState(state) \
    (((state) != LETTERBOX_STATE_NONE) & ((state) != LETTERBOX_STATE_UNUSED_AIMING))

extern LetterboxState g_LetterboxState;
extern PauseScreen g_PauseScreen;

extern Boolean8 g_IsTrainingHeadingKept;
extern Boolean g_IsAimCameraEasing;
extern Boolean g_HasPressedShotButton;
extern SignedBinaryAngle g_LookBackHeading;
extern SignedBinaryAngle g_FreeLookHeading;
extern s32 g_ShotSetupFrameCount;
extern s32 g_ImpactPointLingerFrameCount;
extern ShotFlags g_ShotFlags;
extern Boolean g_HasShotComeToRest;
extern Boolean8 g_IsHudTurnAwayRequested;
extern CurrentShot g_ScriptedShot;
extern s8 g_ReplayStrokeCount;

enum ContestMovieState {
    CONTEST_MOVIES_DONE = -1,
    CONTEST_MOVIES_WAITING = 0,
    CONTEST_MOVIES_PLAYING = 60
};

typedef s8 ContestMovieState;

extern ContestMovieState g_ContestMovieState;
extern u32 g_GroundDrawFrame;
extern CurrentShot g_SavedShots[PLAYER_COUNT_MAX];
extern Player g_SavedPlayers[PLAYER_COUNT_MAX];
extern ShotLaunch g_LastShotLaunch;
extern u8 g_UnreadPlayBuffer[0xDEC];

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
