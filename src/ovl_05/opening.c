#include "opening.h"

#include <math.h>
#include <stdlib.h>
#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "ball_simulation.h"
#include "billboard.h"
#include "camera.h"
#include "camera_editor.h"
#include "character.h"
#include "character_slot.h"
#include "club.h"
#include "compiler.h"
#include "controller.h"
#include "course_space.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "debug_text.h"
#include "effects.h"
#include "game.h"
#include "game_flags.h"
#include "game_state.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "heap.h"
#include "hole_display.h"
#include "language.h"
#include "main.h"
#include "math_util.h"
#include "menu.h"
#include "opening_ball.h"
#include "overlay_manager.h"
#include "rcp.h"
#include "round_hud.h"
#include "scene.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "surface.h"
#include "terrain.h"
#include "text.h"

#define OPENING_BILLBOARD_HEAP_SIZE 0x7038

enum OpeningShotState {
    OPENING_SHOT_IDLE,
    OPENING_SHOT_FLYING,
    OPENING_SHOT_PLAIN_FLIGHT
};

typedef s32 OpeningShotState;

enum OpeningBallImage {
    MARIO_FACE_BALL,
    DONKEY_KONG_FACE_BALL,
    YOSHI_FACE_BALL,
    LUIGI_FACE_BALL,
    PLAIN_BALL,
    OPENING_BALL_IMAGE_COUNT
};

typedef s32 OpeningBallImage;

#define OPENING_BALL_SLOT_COUNT 2
#define OPENING_BALL_IMAGE_SIZE 0x1208
#define OPENING_BALL_IMAGE_WIDTH 64
#define OPENING_BALL_STRIP_HEIGHT 32
#define OPENING_BALL_STRIP_COUNT 2
#define OPENING_BALL_STRIP_SIZE (OPENING_BALL_IMAGE_WIDTH * OPENING_BALL_STRIP_HEIGHT)
#define OPENING_BALL_NORMAL_SCALE 1.0f
#define OPENING_BALL_LARGE_SCALE 3.0f

#define OPENING_SCENE_END 999
#define LAKITU_TRAIL_DEPTH 300.0f

enum OpeningBallState {
    OPENING_BALL_HIDDEN,
    OPENING_BALL_ON_GROUND,
    OPENING_BALL_IN_AIR
};

typedef s32 OpeningBallState;

#define OPENING_NAME_LOGO_COUNT 7
#define OPENING_NAME_SLOT_COUNT 2
#define OPENING_NAME_LOGO_SIZE 0x2208

typedef struct OpeningNameLogo {
    WideAssetId assetId;
    s32 width;
    s32 height;
    s32 x;
    s32 y;
    s32 shownWidth;
    s32 shownHeight;
} OpeningNameLogo;

#define NAME_LOGO_UNFOLD 0
#define NAME_LOGO_FLIP_DOWN 10
#define NAME_LOGO_FLIP_UP 20
#define NAME_LOGO_STILL 30

enum OpeningCue {
    OPENING_CUE_MARIO_TEE_SHOT,
    OPENING_CUE_PEACH_CHEER,
    OPENING_CUE_LUIGI_CHEER,
    OPENING_CUE_PEACH_TEE_SHOT = 4,
    OPENING_CUE_PLUM_CHEER,
    OPENING_CUE_PLUM_TEE_SHOT = 7,
    OPENING_CUE_MARIO_CHEER,
    OPENING_CUE_LUIGI_TEE_SHOT,
    OPENING_CUE_PEACH_PLUM_WALK = 11,
    OPENING_CUE_MARIO_LUIGI_WALK,
    OPENING_CUE_MARIO_REACH_BALL,
    OPENING_CUE_PEACH_WAVE,
    OPENING_CUE_PEACH_APPROACH,
    OPENING_CUE_BALL_INSPECTION = 17,
    OPENING_CUE_MARIO_LOOK_UP,
    OPENING_CUE_FREEZE,
    OPENING_CUE_UNFREEZE,
    OPENING_CUE_BABY_MARIO_ARRIVAL,
    OPENING_CUE_BABY_MARIO_RIDE,
    OPENING_CUE_MARIO_PUZZLED,
    OPENING_CUE_CLEAR_CHARACTERS,
    OPENING_CUE_LUIGI_SEARCH,
    OPENING_CUE_TOAD_FLAG,
    OPENING_CUE_LUIGI_DISMAY,
    OPENING_CUE_GATHERING,
    OPENING_CUE_COUNT,
    OPENING_CUE_INVALID = 29,
    OPENING_CUES_FINISHED,
    OPENING_CUE_DONE
};

typedef s32 OpeningCue;

enum LookAtEase {
    LOOK_AT_EASE_NONE,
    LOOK_AT_EASE_FAST,
    LOOK_AT_EASE_SLOW,
    LOOK_AT_EASE_SPEEDING_UP
};

typedef s32 LookAtEase;

#define LAKITU_IMAGE_BUFFER_SIZE 0x608
#define LAKITU_TRAIL_IMAGE_BUFFER_SIZE 0xC08

#define LAKITU_TRAIL_SEGMENT_COUNT 40

#define ASSET_LOGO_LETTER_IMAGES 0x945
#define ASSET_LOGO_CLUB_IMAGE 0x946
#define ASSET_LOGO_MARIO_IMAGE 0x94B
#define ASSET_LOGO_BALL_IMAGES 0x94E
#define LOGO_LETTER_IMAGES_BUFFER_SIZE 0x5308
#define LOGO_CLUB_IMAGE_BUFFER_SIZE 0x3058
#define LOGO_MARIO_IMAGE_BUFFER_SIZE 0x448
#define LOGO_BALL_IMAGES_BUFFER_SIZE 0x3208

#define LOGO_LETTER_COUNT 9

enum LogoLetterIndex {
    LOGO_LETTER_M,
    LOGO_LETTER_A,
    LOGO_LETTER_R,
    LOGO_LETTER_I,
    LOGO_LETTER_FIRST_O,
    LOGO_LETTER_G,
    LOGO_LETTER_SECOND_O,
    LOGO_LETTER_L,
    LOGO_LETTER_F
};

typedef s32 LogoLetterIndex;

#define LOGO_LETTER_HIDDEN (-1000)

#define LOGO_M_DROP_X 57
#define LOGO_A_DROP_X 81
#define LOGO_R_DROP_X 112
#define LOGO_I_DROP_X 132
#define LOGO_FIRST_O_DROP_X 154
#define LOGO_G_DROP_X 193
#define LOGO_SECOND_O_DROP_X 220
#define LOGO_L_DROP_X 242
#define LOGO_F_DROP_X 265
#define LOGO_M_SHRUNK_X 86
#define LOGO_A_SHRUNK_X 104
#define LOGO_R_SHRUNK_X 125
#define LOGO_I_SHRUNK_X 139
#define LOGO_FIRST_O_SHRUNK_X 154
#define LOGO_G_SHRUNK_X 184
#define LOGO_SECOND_O_SHRUNK_X 204
#define LOGO_L_SHRUNK_X 220
#define LOGO_F_SHRUNK_X 234
#define LOGO_M_DEPTH 8
#define LOGO_A_DEPTH 12
#define LOGO_R_DEPTH 16
#define LOGO_I_DEPTH 20
#define LOGO_FIRST_O_DEPTH 24
#define LOGO_G_DEPTH 28
#define LOGO_SECOND_O_DEPTH 36
#define LOGO_L_DEPTH 40
#define LOGO_F_DEPTH 44

enum LogoLetterState {
    LOGO_LETTER_WAITING,
    LOGO_LETTER_FALLING,
    LOGO_LETTER_SQUASHING,
    LOGO_LETTER_SQUASHED,
    LOGO_LETTER_STRETCHING,
    LOGO_LETTER_LANDED = 9
};

typedef s32 LogoLetterState;

#define OPENING_SHOT_DASH_COUNT_MAX 200

void DrawNameLogos(Gfx **displayListHead);
void DrawOpeningBalls(Gfx **displayListHead, CameraMatrices *matrices);
void DrawOpeningImpactLines(Gfx **displayListHead);
void DrawLakitu(Gfx **displayListHead);
void DrawOpeningBurst(Gfx **displayListHead);
void DrawOpeningShotBall(Gfx **displayListHead);
void DrawOpeningSparks(Gfx **displayListHead);

#define ROTATING_SOUND_CHANNEL_COUNT 11

void LoadOpeningShotBall(void);
void LoadOpeningBurst(void);
void LoadLakitu(void);
void LoadOpeningImpactLines(void);
void EaseOpeningLookAt(void);
void LoadOpeningBall(OpeningBallImage image, s32 slot);

#define OPENING_EFFECT_IMAGE_SIZE 0x1208

#define SPARK_COUNT 20

enum AmbientSoundChoice {
    AMBIENT_SOUND_NONE,
    AMBIENT_SOUND_FIRST,
    AMBIENT_SOUND_SECOND
};

typedef s32 AmbientSoundChoice;

enum SparksState {
    SPARKS_HIDDEN,
    SPARKS_STARTING,
    SPARKS_FLYING
};

typedef s32 SparksState;

enum LogoState {
    LOGO_DROPPING_LETTERS,
    LOGO_CLUB_STARTING,
    LOGO_CLUB_CIRCLING,
    LOGO_CLUB_SPINNING,
    LOGO_SHINING,
    LOGO_SHRINK_STARTING,
    LOGO_SHRINKING,
    LOGO_SHRUNK
};

typedef s32 LogoState;

enum LogoFadeStage {
    LOGO_FADE_NONE,
    LOGO_FADE_GRAYING,
    LOGO_FADE_GRAY,
    LOGO_FADE_WHITENING,
    LOGO_FADE_WHITE,
    LOGO_FADE_LOGO_OUT
};

typedef s32 LogoFadeStage;

#define LOGO_SHINE_FIRST_FRAME 20
#define LOGO_SHINE_FIRST_SHOWN_FRAME 4
#define LOGO_SHINE_LAST_SHOWN_FRAME 36
#define LOGO_SHINE_LAST_FRAME 64

#define LOGO_CLUB_STRIP_COUNT 6

#define ASSET_SHOT_BALL_IMAGE 0x939

#define ASSET_MARIO_BALL_IMAGE 0x901
#define ASSET_DONKEY_KONG_BALL_IMAGE 0x902
#define ASSET_YOSHI_BALL_IMAGE 0x903
#define ASSET_LUIGI_BALL_IMAGE 0x92B

#define ASSET_MARIO_NAME_LOGO 0x904
#define ASSET_BABY_MARIO_NAME_LOGO 0x905
#define ASSET_DONKEY_KONG_NAME_LOGO 0x906
#define ASSET_LUIGI_NAME_LOGO 0x907
#define ASSET_PEACH_NAME_LOGO 0x909
#define ASSET_PLUM_NAME_LOGO 0x90A
#define ASSET_YOSHI_NAME_LOGO 0x90B

#define BURST_NOT_STARTED (-1)
#define LAKITU_TRAIL_START_ALPHA 220
#define LOOK_AT_START_EASE_RATE 0.05f
#define LOGO_FADE_FIRST_FRAMES_LEFT 63
#define LOGO_CLUB_SPIN_FRAMES_MIN 40
#define LOGO_CLUB_START_ORBIT_RADIUS 240.0f
#define LOGO_CLUB_START_ORBIT_ANGLE 330.0f
#define LOGO_CLUB_START_UNUSED_ANGLE 180.0f
#define LOGO_CLUB_START_ANGLE 360.0f

static s32 f_LastPlacedSlotIndex = 0;
static s32 f_OpeningFrameCount = 0;
static Boolean f_HasLogoStarted = FALSE;
static SoundChannel f_OpeningSoundChannel = 0;
static s32 f_LogoClubPosition[AXIS_COUNT] = {0, 2300, 32};
static f32 f_LogoClubOrbitRadius = LOGO_CLUB_START_ORBIT_RADIUS;
static Boolean f_IsLogoClubSwinging = FALSE;
static Boolean f_AreAmbientSoundsOn = FALSE;
static s32 f_AlternatingSoundTimer = 0;
static s32 f_RepeatingSoundTimer = 0;
static s32 f_RandomSoundTimer = 0;
static AmbientSoundChoice f_AlternatingSound = AMBIENT_SOUND_FIRST;
static AmbientSoundChoice f_RandomSound = AMBIENT_SOUND_NONE;
static s32 f_AlternatingSoundFirstLength = 50;
static s32 f_AlternatingSoundSecondLength = 100;
static s32 f_RepeatingSoundLength = 60;
static s32 f_RandomSoundFirstLength = 30;
static s32 f_RandomSoundSecondLength = 60;
static s32 f_OpeningScene = 0;
static s32 f_OpeningSceneFramesLeft = 0;
static OpeningNameLogo f_OpeningNameLogos[OPENING_NAME_LOGO_COUNT] = {
    {ASSET_MARIO_NAME_LOGO, 160, 32, 88, 208, 1, 32},
    {ASSET_LUIGI_NAME_LOGO, 160, 32, 80, 208, 1, 32},
    {ASSET_PEACH_NAME_LOGO, 160, 32, 84, 208, 1, 32},
    {ASSET_PLUM_NAME_LOGO, 160, 32, 88, 208, 1, 32},
    {ASSET_YOSHI_NAME_LOGO, 160, 32, 88, 208, 1, 32},
    {ASSET_BABY_MARIO_NAME_LOGO, 216, 32, 196, 32, 1, 32},
    {ASSET_DONKEY_KONG_NAME_LOGO, 256, 32, 136, 208, 1, 32},
};
static Boolean f_AreImpactLinesShown = FALSE;
static Boolean f_IsBurstShown = FALSE;
static s32 f_ShotBallScale = 0;
static LookAtEase f_LookAtEaseMode = LOOK_AT_EASE_NONE;
static s32 f_UnusedWordAfterEaseMode UNUSED = 0;
static OpeningBallState f_OpeningBallStates[OPENING_BALL_SLOT_COUNT] = {
    OPENING_BALL_HIDDEN,
    OPENING_BALL_HIDDEN,
};
static s32 f_UnusedWordsAfterBallStates[2] UNUSED = {0, 0};
static f32 f_OpeningBallScales[OPENING_BALL_SLOT_COUNT] = {
    OPENING_BALL_NORMAL_SCALE,
    OPENING_BALL_NORMAL_SCALE,
};
static WideAssetId f_OpeningBallAssets[OPENING_BALL_IMAGE_COUNT] = {
    ASSET_MARIO_BALL_IMAGE,
    ASSET_DONKEY_KONG_BALL_IMAGE,
    ASSET_YOSHI_BALL_IMAGE,
    ASSET_LUIGI_BALL_IMAGE,
    ASSET_SHOT_BALL_IMAGE,
};
static Boolean f_IsDonkeyKongShown = FALSE;
static s32 f_LakituScreenPoint[PLANE_AXIS_COUNT] = {0, 230};
static Boolean f_IsLakituFlying = FALSE;
static s32 f_LakituTrailNewestIndex = 0;
static s32 f_UnusedWordAfterTrailIndex UNUSED = 0;
static UBoolean8 f_IsLogoBallSlot[OPENING_BALL_SLOT_COUNT] = {FALSE, FALSE};
static ShotLaunch f_OpeningFirstShot = {-0x3400, 0, 280, 0, 40, -15, 0, SHOT_MODE_NORMAL};
static ShotLaunch f_OpeningSecondShot =
    {-0x2000, 0, LAUNCH_POWER_MAX, 0, 0, 0, 0, SHOT_MODE_NORMAL};
static ShotLaunch f_OpeningThirdShot = {-0x2300, 0, 240, 0, 0, 20, 0, SHOT_MODE_NORMAL};
static ShotLaunch f_OpeningFourthShot = {-0x2B00, 1, 400, 0, -180, -720, 0, SHOT_MODE_NORMAL};
static SparksState f_OpeningSparksState = SPARKS_HIDDEN;
f32 g_LookAtMarkerScale = 1.0f;
static Boolean f_IsLookAtMarkerShown = FALSE;
static OpeningShotState f_OpeningShotState = OPENING_SHOT_IDLE;
static ShotLaunch f_UnusedSecondShotCopy
    UNUSED = {-0x2000, 0, LAUNCH_POWER_MAX, 0, 0, 0, 0, SHOT_MODE_NORMAL};
static Vtx f_OpeningShotDashVertices[QUAD_VERTEX_COUNT] = {
    {{{-15, 0, 0}, 0, {0, 0}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{15, 0, 0}, 0, {0, 2048}, {0x00, 0x00, 0x00, 0xFF}}},
    {{{15, 0, -1}, 0, {2048, 2048}, {0x00, 0x00, 0xFF, 0x00}}},
    {{{-15, 0, -1}, 0, {2048, 0}, {0xFF, 0x00, 0x00, 0x00}}},
};
static Boolean f_IsOpeningShotPathShown = FALSE;
static s32 f_OpeningShotDashCount = OPENING_SHOT_DASH_COUNT_MAX;
static s32 f_OpeningShotPathDashCount = 198;
static Boolean8 f_IsOpeningShotPathRed = FALSE;
static s32 f_OpeningShotPathScroll = 0;
static f32 f_UnusedStickOffsetX = 0.0f;
static f32 f_UnusedStickOffsetY = 0.0f;
static s32 f_OpeningFrameParity = 0;
static Vtx f_LookAtMarkerVertices[6][QUAD_VERTEX_COUNT] = {
    {
        {{{-50, 0, 5}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
        {{{-50, 0, -5}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
        {{{50, 0, -5}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
        {{{50, 0, 5}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    },
    {
        {{{-50, 5, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
        {{{-50, -5, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
        {{{50, -5, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
        {{{50, 5, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    },
    {
        {{{0, -50, -5}, 0, {0, 0}, {0xFF, 0xFF, 0x00, 0xFF}}},
        {{{0, 50, -5}, 0, {0, 0}, {0xFF, 0xFF, 0x00, 0xFF}}},
        {{{0, 50, 5}, 0, {0, 0}, {0xFF, 0xFF, 0x00, 0xFF}}},
        {{{0, -50, 5}, 0, {0, 0}, {0xFF, 0xFF, 0x00, 0xFF}}},
    },
    {
        {{{-5, -50, 0}, 0, {0, 0}, {0xFF, 0xFF, 0x00, 0xFF}}},
        {{{-5, 50, 0}, 0, {0, 0}, {0xFF, 0xFF, 0x00, 0xFF}}},
        {{{5, 50, 0}, 0, {0, 0}, {0xFF, 0xFF, 0x00, 0xFF}}},
        {{{5, -50, 0}, 0, {0, 0}, {0xFF, 0xFF, 0x00, 0xFF}}},
    },
    {
        {{{-3, 3, -50}, 0, {0, 0}, {0x00, 0xFF, 0xFF, 0xFF}}},
        {{{3, -3, -50}, 0, {0, 0}, {0x00, 0xFF, 0xFF, 0xFF}}},
        {{{3, -3, 50}, 0, {0, 0}, {0x00, 0xFF, 0xFF, 0xFF}}},
        {{{-3, 3, 50}, 0, {0, 0}, {0x00, 0xFF, 0xFF, 0xFF}}},
    },
    {
        {{{-3, -3, -50}, 0, {0, 0}, {0x00, 0xFF, 0xFF, 0xFF}}},
        {{{3, 3, -50}, 0, {0, 0}, {0x00, 0xFF, 0xFF, 0xFF}}},
        {{{3, 3, 50}, 0, {0, 0}, {0x00, 0xFF, 0xFF, 0xFF}}},
        {{{-3, -3, 50}, 0, {0, 0}, {0x00, 0xFF, 0xFF, 0xFF}}},
    },
};
static Vtx f_OpeningBallStripVertices[OPENING_BALL_STRIP_COUNT][QUAD_VERTEX_COUNT] = {
    {
        {{{1, -1, 0}, 0, {0, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{1, -2, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{-1, -2, 0}, 0, {4096, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{-1, -1, 0}, 0, {4096, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    },
    {
        {{{1, 0, 0}, 0, {0, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{1, -1, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{-1, -1, 0}, 0, {4096, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{-1, 0, 0}, 0, {4096, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    },
};
static Boolean f_HasLogoBallLastPosition = FALSE;
static Vtx f_OpeningBallShadowVertices[QUAD_VERTEX_COUNT] = {
    {{{-1, 0, 1}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-1, 0, -1}, 0, {0, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{1, 0, -1}, 0, {2048, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{1, 0, 1}, 0, {2048, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static Vtx f_LogoBallMarioVertices[QUAD_VERTEX_COUNT] = {
    {{{-1, -1, 1}, 0, {1536, 1536}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-1, -1, -1}, 0, {0, 1536}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-1, 1, -1}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-1, 1, 1}, 0, {1536, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static Boolean f_ShouldResetLogoBallRoll = TRUE;
static s32 f_LogoBallFrame = 0;
static Boolean f_HasProjectedLogoBallLastPosition = FALSE;
static Vtx f_ProjectedShadowVertices[QUAD_VERTEX_COUNT] = {
    {{{-1, 0, 1}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-1, 0, -1}, 0, {0, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{1, 0, -1}, 0, {2048, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{1, 0, 1}, 0, {2048, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static Vtx f_ProjectedLogoBallMarioVertices[QUAD_VERTEX_COUNT] = {
    {{{-1, -1, 1}, 0, {1536, 1536}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-1, -1, -1}, 0, {0, 1536}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-1, 1, -1}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-1, 1, 1}, 0, {1536, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static Boolean f_ShouldResetProjectedLogoBallRoll = TRUE;
static s32 f_ProjectedLogoBallFrame = 0;
static s32 f_ImpactLinesFrame = 0;
static s32 f_LakituTrailAlpha = LAKITU_TRAIL_START_ALPHA;
static Vtx f_LakituTrailVertices[2] = {
    {{{-10, 0, 0}, 0, {0, 0}, {0xFF, 0x7F, 0x00, 0xFF}}},
    {{{10, 0, 0}, 0, {3072, 0}, {0xFF, 0xFF, 0xBF, 0xFF}}},
};
static s32 f_LakituAngle = 0;
static s32 f_BurstFrame = BURST_NOT_STARTED;
static s32 f_ShotBallAlpha = OPAQUE_ALPHA;
static Vtx f_ShotBallShadowVertices[QUAD_VERTEX_COUNT] = {
    {{{-1, 0, 1}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-1, 0, -1}, 0, {0, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{1, 0, -1}, 0, {2048, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{1, 0, 1}, 0, {2048, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static f32 f_LookAtEaseRate = LOOK_AT_START_EASE_RATE;
static s32 f_LogoLettersAlpha = OPAQUE_ALPHA;
static s32 f_LogoClubAlpha = OPAQUE_ALPHA;
static s32 f_LogoShineFrame = LOGO_SHINE_FIRST_FRAME;
static s32 f_LogoClubSpinFramesLeft = LOGO_CLUB_SPIN_FRAMES_MIN;
static LogoState f_LogoState = LOGO_DROPPING_LETTERS;
static LogoFadeStage f_LogoFadeStage = LOGO_FADE_NONE;
static s32 f_LogoFadeFramesLeft = LOGO_FADE_FIRST_FRAMES_LEFT;
static Vec2 f_LogoClubScale = {1.0f, 1.0f};
static Degrees f_LogoClubAngle = LOGO_CLUB_START_ANGLE;
static Degrees f_LogoClubOrbitAngle = LOGO_CLUB_START_ORBIT_ANGLE;
static f32 f_UnusedFloatAfterClubOrbit UNUSED = 0.0f;
static Degrees f_LogoClubUnusedAngle = LOGO_CLUB_START_UNUSED_ANGLE;
static Degrees f_LogoClubWobbleAngle = 0.0f;
static Vtx f_LogoLetterVertices[2][QUAD_VERTEX_COUNT] = {
    {
        {{{-24, 48, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{-24, 24, 0}, 0, {0, 1536}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{24, 24, 0}, 0, {3072, 1536}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{24, 48, 0}, 0, {3072, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    },
    {
        {{{-24, 24, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{-24, 0, 0}, 0, {0, 1536}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{24, 0, 0}, 0, {3072, 1536}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{24, 24, 0}, 0, {3072, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    },
};
static Vtx f_LogoClubVertices[LOGO_CLUB_STRIP_COUNT][QUAD_VERTEX_COUNT] = {
    {
        {{{-76, 39, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{-76, 26, 0}, 0, {0, 832}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{76, 26, 0}, 0, {9728, 832}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{76, 39, 0}, 0, {9728, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    },
    {
        {{{-76, 26, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{-76, 13, 0}, 0, {0, 832}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{76, 13, 0}, 0, {9728, 832}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{76, 26, 0}, 0, {9728, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    },
    {
        {{{-76, 13, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{-76, 0, 0}, 0, {0, 832}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{76, 0, 0}, 0, {9728, 832}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{76, 13, 0}, 0, {9728, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    },
    {
        {{{-76, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{-76, -13, 0}, 0, {0, 832}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{76, -13, 0}, 0, {9728, 832}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{76, 0, 0}, 0, {9728, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    },
    {
        {{{-76, -13, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{-76, -26, 0}, 0, {0, 832}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{76, -26, 0}, 0, {9728, 832}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{76, -13, 0}, 0, {9728, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    },
    {
        {{{-76, -26, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{-76, -39, 0}, 0, {0, 832}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{76, -39, 0}, 0, {9728, 832}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{76, -26, 0}, 0, {9728, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    },
};
static Vtx f_OpeningSparkVertices[TRIANGLE_VERTEX_COUNT] = {
    {{{0, 16, 0}, 0, {0, 0}, {0xFF, 0x7F, 0x00, 0xFF}}},
    {{{-1, -48, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xBF, 0xFF}}},
    {{{1, -48, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xBF, 0xFF}}},
};

static u8 f_UnusedBufferBeforeCueSlot[0x10] UNUSED;
static s32 f_CueSlotIndex;
static CoursePoint f_OpeningShotImpactPoint;

static u8 f_UnusedBufferBeforeMarkerMatrices[0x80] UNUSED;
static Mtx f_LookAtMarkerTranslation;
static Mtx f_LookAtMarkerScaling;
static Mtx f_OpeningBallMatrices[OPENING_BALL_SLOT_COUNT][DRAW_BUFFER_COUNT];
static Mtx f_OpeningBallScaleMatrices[OPENING_BALL_SLOT_COUNT];
static Mtx f_OpeningBallShadowMatrices[OPENING_BALL_SLOT_COUNT][DRAW_BUFFER_COUNT];
static Vec3 f_LogoBallLastPosition;
static Mtx f_LogoBallMarioTranslations[DRAW_BUFFER_COUNT];
static Mtx f_LogoBallMarioRolls[DRAW_BUFFER_COUNT];
static Mtx f_LogoBallMarioScale;
static Mtx f_LogoBallMarioOffset;
static u8 f_UnusedBufferBeforeBallRoll[16] UNUSED;
static Mat4 f_LogoBallRoll;
static u8 f_UnusedBytesAfterBallRoll[4] UNUSED;
static Vec3 f_ProjectedBallPosition;
static s32 f_ProjectedBallLeft;
static s32 f_ProjectedBallTop;
static s32 f_ProjectedBallRight;
static s32 f_ProjectedBallBottom;
static s32 f_ProjectedBallTexelStep;
static Vec3 f_ProjectedLogoBallLastPosition;
static Mtx f_ProjectedShadowMatrices[DRAW_BUFFER_COUNT][OPENING_BALL_SLOT_COUNT];
static Mtx f_ProjectedShadowScaleMatrices[OPENING_BALL_SLOT_COUNT];
static Mtx f_ProjectedLogoBallMarioTranslations[DRAW_BUFFER_COUNT];
static Mtx f_ProjectedLogoBallMarioRolls[DRAW_BUFFER_COUNT];
static Mtx f_ProjectedLogoBallMarioScale;
static Mtx f_ProjectedLogoBallMarioOffset;
static u8 f_UnusedBufferBeforeProjectedRoll[16] UNUSED;
static Mat4 f_ProjectedLogoBallRoll;
static s32 f_ProjectedBallExtraPixel;
static Vec3 f_ShotBallPosition;
static u8 f_UnusedBytesAfterShotBall[8] UNUSED;
static Mtx f_ShotBallShadowTranslation;
static Mtx f_ShotBallShadowScaling;
static Mtx f_LogoLetterTranslations[LOGO_LETTER_COUNT][DRAW_BUFFER_COUNT];
static Mtx f_LogoLetterScalings[LOGO_LETTER_COUNT][DRAW_BUFFER_COUNT];
static Mtx f_LogoClubTranslations[DRAW_BUFFER_COUNT];
static Mtx f_LogoClubScalings[DRAW_BUFFER_COUNT];
static Mtx f_LogoClubRotations[DRAW_BUFFER_COUNT];
static u8 f_UnusedBytesAfterClubRotations[8] UNUSED;
static s32 f_LogoLetterSteps[LOGO_LETTER_COUNT];
static s32 f_LogoLetterRises[2];
static f32 f_LogoScaleStep;
static s32 f_LogoClubStepX;
static s32 f_LogoClubStepY;
static Vec2 f_OpeningSparkPositions[SPARK_COUNT];
static Radians f_OpeningSparkAngles[SPARK_COUNT];
static Vec2 f_OpeningSparkVelocities[SPARK_COUNT];
static f32 f_OpeningSparkScale;
static s32 f_OpeningSparkFramesLeft;
static Mtx f_OpeningSparkMatrices[DRAW_BUFFER_COUNT][SPARK_COUNT];
static SoundChannel f_DonkeyKongSoundChannel;
static SoundChannel f_ClosingSoundChannel;
static u8 f_NameSlotImages[OPENING_NAME_SLOT_COUNT][OPENING_NAME_LOGO_SIZE];
static s32 f_NameSlotLogoIndices[OPENING_NAME_SLOT_COUNT];
static s32 f_NameSlotSteps[OPENING_NAME_SLOT_COUNT];
static Boolean f_IsNameSlotShown[OPENING_NAME_SLOT_COUNT];
static u8 f_OpeningEffectImage[OPENING_EFFECT_IMAGE_SIZE];
static u8 f_ShotBallImage[OPENING_BALL_IMAGE_SIZE];
static u8 f_OpeningBallImages[OPENING_BALL_SLOT_COUNT][OPENING_BALL_IMAGE_SIZE];

static Vec3 f_OpeningBallPositions[OPENING_BALL_SLOT_COUNT];
static f32 f_LogoBallFollowRate;

static u8 f_UnusedBytesAfterFollowRate[0x4] UNUSED;
static u8 f_UnusedBufferAfterFollowRate[0x10] UNUSED;

static u8 f_LogoBallImages[LOGO_BALL_IMAGES_BUFFER_SIZE];
static u8 f_LakituImage[LAKITU_IMAGE_BUFFER_SIZE];
static u8 f_LakituTrailImage[LAKITU_TRAIL_IMAGE_BUFFER_SIZE];
static Mtx f_LakituTrailMatrices[LAKITU_TRAIL_SEGMENT_COUNT];
static Vec2 f_LakituTrailStart;
static u8 f_UnusedBufferAfterTrailStart[24] UNUSED;
static u8 f_LogoMarioImage[LOGO_MARIO_IMAGE_BUFFER_SIZE];
static u8 f_LogoLetterImages[LOGO_LETTER_IMAGES_BUFFER_SIZE];
static u8 f_LogoClubImage[LOGO_CLUB_IMAGE_BUFFER_SIZE];
static ScreenPosition f_LogoLetters[LOGO_LETTER_COUNT];
static LogoLetterState f_LogoLetterStates[LOGO_LETTER_COUNT];
static f32 f_LogoLetterSpeeds[LOGO_LETTER_COUNT];
static Vec2 f_LogoLetterScales[LOGO_LETTER_COUNT];
static s32 f_LogoLetterSquashFrames[LOGO_LETTER_COUNT];
static u8 f_LogoShineImage[SHINE_IMAGE_BUFFER_SIZE];
static Vec3 f_OpeningSparksOrigin;
static u8 f_UnusedBufferAfterSparksOrigin[12] UNUSED;
static u8 *f_OpeningCharacterBuffers[2];
static u8 f_UnusedBytesAfterCharacterBuffers[4] UNUSED;
static void *f_OpeningBillboardHeap;
static Mtx f_OpeningShotDashMatrices[DRAW_BUFFER_COUNT][OPENING_SHOT_DASH_COUNT_MAX];

CameraEditor g_CameraEditor;
OpeningCue f_OpeningCue;
BallSimulation f_OpeningShotSimulation;

void AdvanceOpeningSoundChannel(void) {
    f_OpeningSoundChannel++;
    if (f_OpeningSoundChannel == ROTATING_SOUND_CHANNEL_COUNT) {
        f_OpeningSoundChannel = 0;
    }
}

#define LOGO_CLUB_SOUND_CHANNEL 12
#define LOGO_CLUB_SOUND_LOUDEST_VOLUME 48
#define LOGO_CLUB_SOUND_FADE_RATE 0.2f
#define LOGO_CLUB_PIXELS_PER_PAN_STEP 2

void PlayLogoClubSound(void) {
    if (f_IsLogoClubSwinging) {
        PlaySoundWithEffects(
            SOUND_CONTACT_PLAIN,
            LOGO_CLUB_SOUND_CHANNEL,
            LOGO_CLUB_SOUND_LOUDEST_VOLUME -
                (s32)(f_LogoClubOrbitRadius * LOGO_CLUB_SOUND_FADE_RATE),
            NO_REVERB,
            SOUND_FULL_SCALE -
                ((f_LogoClubPosition[AXIS_X] - (SCREEN_WIDTH / 2)) / LOGO_CLUB_PIXELS_PER_PAN_STEP)
        );
    }
}

#define ALTERNATING_SOUND_CHANNEL 13
#define REPEATING_SOUND_CHANNEL 14
#define RANDOM_SOUND_CHANNEL 15
#define ALTERNATING_SOUND_PAN 0x9D
#define REPEATING_SOUND_PAN 0x75
#define RANDOM_SOUND_PAN 0x61
#define AMBIENT_SOUND_FADE_RATE 0.5f
#define AMBIENT_SOUND_CHANCE_MASK 100
#define AMBIENT_SOUND_CHANCE 80
#define RANDOM_SOUND_CHOICE_MASK 3

void PlayOpeningAmbientSounds(void) {
    if (f_AreAmbientSoundsOn == TRUE) {
        if ((u32)(g_CameraEditor.distance * AMBIENT_SOUND_FADE_RATE) < SOUND_FULL_SCALE) {
            if (f_AlternatingSoundTimer == 0 &&
                (rand() & AMBIENT_SOUND_CHANCE_MASK) < AMBIENT_SOUND_CHANCE) {
                if (f_AlternatingSound == AMBIENT_SOUND_SECOND) {
                    f_AlternatingSound = AMBIENT_SOUND_FIRST;
                    f_AlternatingSoundTimer = f_AlternatingSoundFirstLength;
                }
                else {
                    f_AlternatingSound = AMBIENT_SOUND_SECOND;
                    f_AlternatingSoundTimer = f_AlternatingSoundSecondLength;
                }
            }

            if (f_RepeatingSoundTimer == 0 &&
                (rand() & AMBIENT_SOUND_CHANCE_MASK) < AMBIENT_SOUND_CHANCE) {
                f_RepeatingSoundTimer = f_RepeatingSoundLength;
            }

            if (f_RandomSoundTimer == 0 &&
                (rand() & AMBIENT_SOUND_CHANCE_MASK) < AMBIENT_SOUND_CHANCE) {
                switch (rand() & RANDOM_SOUND_CHOICE_MASK) {
                    case 0:
                        f_RandomSound = AMBIENT_SOUND_FIRST;
                        f_RandomSoundTimer = f_RandomSoundFirstLength;
                        break;
                    case 1:
                        f_RandomSound = AMBIENT_SOUND_SECOND;
                        f_RandomSoundTimer = f_RandomSoundSecondLength;
                        break;
                }
            }
        }

        if (f_AlternatingSoundTimer != 0) {
            if (f_AlternatingSound == AMBIENT_SOUND_FIRST &&
                f_AlternatingSoundTimer == f_AlternatingSoundFirstLength) {
                PlaySoundWithEffects(
                    SOUND_AMBIENT_ALTERNATING_FIRST,
                    ALTERNATING_SOUND_CHANNEL,
                    SOUND_FULL_SCALE - (u32)(g_CameraEditor.distance * AMBIENT_SOUND_FADE_RATE),
                    NO_REVERB,
                    ALTERNATING_SOUND_PAN
                );
            }

            if (f_AlternatingSound == AMBIENT_SOUND_SECOND &&
                f_AlternatingSoundTimer == f_AlternatingSoundSecondLength) {
                PlaySoundWithEffects(
                    SOUND_AMBIENT_ALTERNATING_SECOND,
                    ALTERNATING_SOUND_CHANNEL,
                    SOUND_FULL_SCALE - (u32)(g_CameraEditor.distance * AMBIENT_SOUND_FADE_RATE),
                    NO_REVERB,
                    ALTERNATING_SOUND_PAN
                );
            }

            f_AlternatingSoundTimer--;
        }

        if (f_RepeatingSoundTimer != 0) {
            if (f_RepeatingSoundTimer == f_RepeatingSoundLength) {
                PlaySoundWithEffects(
                    SOUND_AMBIENT_REPEATING,
                    REPEATING_SOUND_CHANNEL,
                    SOUND_FULL_SCALE - (u32)(g_CameraEditor.distance * AMBIENT_SOUND_FADE_RATE),
                    NO_REVERB,
                    REPEATING_SOUND_PAN
                );
            }

            f_RepeatingSoundTimer--;
        }

        if (f_RandomSoundTimer != 0) {
            if (f_RandomSound == AMBIENT_SOUND_FIRST &&
                f_RandomSoundTimer == f_RandomSoundFirstLength) {
                PlaySoundWithEffects(
                    SOUND_AMBIENT_RANDOM_FIRST,
                    RANDOM_SOUND_CHANNEL,
                    SOUND_FULL_SCALE - (u32)(g_CameraEditor.distance * AMBIENT_SOUND_FADE_RATE),
                    NO_REVERB,
                    RANDOM_SOUND_PAN
                );
            }

            if (f_RandomSound == AMBIENT_SOUND_SECOND &&
                f_RandomSoundTimer == f_RandomSoundSecondLength) {
                PlaySoundWithEffects(
                    SOUND_AMBIENT_RANDOM_SECOND,
                    RANDOM_SOUND_CHANNEL,
                    SOUND_FULL_SCALE - (u32)(g_CameraEditor.distance * AMBIENT_SOUND_FADE_RATE),
                    NO_REVERB,
                    RANDOM_SOUND_PAN
                );
            }

            if (--f_RandomSoundTimer == 0) {
                f_RandomSound = AMBIENT_SOUND_NONE;
            }
        }
    }
}

void LoadOpeningLogo(void);

#define FIRST_ALLOCATED_CHARACTER_SLOT 1
#define DEPTH_BUFFER_CHARACTER_SLOT 3

void LoadOpening(void) {
    EncodedAsset asset;
    s32 bufferIndex;

    osSyncPrintf("ConstOpening\n");
    f_OpeningBillboardHeap = AllocMemory(OPENING_BILLBOARD_HEAP_SIZE);
    InitBillboardManager(f_OpeningBillboardHeap, OPENING_BILLBOARD_HEAP_SIZE);
    f_OpeningCue = OPENING_CUE_DONE;
    for (bufferIndex = 0; bufferIndex < ARRAY_COUNT(f_OpeningCharacterBuffers); bufferIndex++) {
        f_OpeningCharacterBuffers[bufferIndex] = AllocMemory(GetCharacterBufferSize());
        SetCharacterBuffer(
            bufferIndex + FIRST_ALLOCATED_CHARACTER_SLOT,
            f_OpeningCharacterBuffers[bufferIndex]
        );
    }

    SetCharacterBuffer(DEPTH_BUFFER_CHARACTER_SLOT, (u8 *)g_DepthBuffer);
    g_ShadowTexture = AllocMemory(OpenEncodedAsset(ASSET_SHADOW_TEXTURE, &asset));
    ReadEncodedAsset(g_ShadowTexture, &asset);
    f_AreAmbientSoundsOn = FALSE;
    LoadEffectImages();
    LoadOpeningLogo();
}

void FreeOpening(void) {
    s32 bufferIndex;

    FreeMemory(&g_ShadowTexture);
    for (bufferIndex = 0; bufferIndex < ARRAY_COUNT(f_OpeningCharacterBuffers); bufferIndex++) {
        FreeMemory(&f_OpeningCharacterBuffers[bufferIndex]);
    }

    AssignCharacterBuffers();
    FreeMemory(&f_OpeningBillboardHeap);
    SetFillColor(255, 255, 255);
    FreeEffectImages();
}

void DrawOpeningCharacters(
    CameraMatrices *matrices,
    Gfx **displayListHead,
    s32 frameParity UNUSED
) {
    s32 slotIndex;

    g_DisplayListHead = *displayListHead;
    for (slotIndex = 0; slotIndex < CHARACTER_SLOT_COUNT; slotIndex++) {
        DrawCharacterAndShadow(matrices, OPENING_CAMERA, slotIndex);
    }
    *displayListHead = g_DisplayListHead;
}

void BuildOpeningShotPath(void) {
    CoursePoint start;
    Mat4 dash;
    Mat4 transform;
    s32 step;
    s32 length;

    for (step = 0; step < f_OpeningShotDashCount; step++) {
        start[AXIS_X] = f_OpeningShotSimulation.position[AXIS_X];
        start[AXIS_Y] = f_OpeningShotSimulation.position[AXIS_Y];
        start[AXIS_Z] = f_OpeningShotSimulation.position[AXIS_Z];
        StepBallSimulation(&f_OpeningShotSimulation);
        length = IntLength3D(
            f_OpeningShotSimulation.position[AXIS_X] - start[AXIS_X],
            f_OpeningShotSimulation.position[AXIS_Y] - start[AXIS_Y],
            f_OpeningShotSimulation.position[AXIS_Z] - start[AXIS_Z]
        );
        guTranslateF(
            dash,
            start[AXIS_X] * COURSE_TO_WORLD_SCALE,
            (start[AXIS_Y] - SHOT_PREVIEW_DROP) * COURSE_TO_WORLD_SCALE,
            start[AXIS_Z] * COURSE_TO_WORLD_SCALE
        );
        if (length != 0 && length <= SHOT_PREVIEW_DASH_LENGTH_MAX) {
            guScaleF(dash, 1.0f, 1.0f, length * COURSE_TO_WORLD_SCALE);
            guAlignF(
                transform,
                0.0f,
                f_OpeningShotSimulation.position[AXIS_X] - start[AXIS_X],
                f_OpeningShotSimulation.position[AXIS_Y] - start[AXIS_Y],
                f_OpeningShotSimulation.position[AXIS_Z] - start[AXIS_Z]
            );
            guMtxCatF(dash, transform, dash);
            guTranslateF(
                transform,
                start[AXIS_X] * COURSE_TO_WORLD_SCALE,
                start[AXIS_Y] * COURSE_TO_WORLD_SCALE,
                start[AXIS_Z] * COURSE_TO_WORLD_SCALE
            );
            guMtxCatF(dash, transform, dash);
        }

        guMtxF2L(dash, &f_OpeningShotDashMatrices[g_DrawBufferIndex][step]);
    }
}

void DrawOpeningShotPath(CameraMatrices *matrices UNUSED) {
    s32 step;

    if (!f_IsOpeningShotPathShown) {
        return;
    }

    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetRenderMode(g_DisplayListHead++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
    gSPLoadGeometryMode(g_DisplayListHead++, 0);
    gSPSetGeometryMode(g_DisplayListHead++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetCombineLERP(
        g_DisplayListHead++,
        PRIMITIVE,
        ENVIRONMENT,
        TEXEL0,
        ENVIRONMENT,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        PRIMITIVE,
        ENVIRONMENT,
        TEXEL0,
        ENVIRONMENT,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetAlphaCompare(g_DisplayListHead++, G_AC_THRESHOLD);
    gDPSetBlendColor(g_DisplayListHead++, 0xFF, 0xFF, 0xFF, 0x40);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
    gSPTexture(
        g_DisplayListHead++,
        TEXTURE_SCALE_HALF,
        TEXTURE_SCALE_HALF,
        0,
        G_TX_RENDERTILE,
        G_ON
    );
    gDPLoadTextureBlock(
        g_DisplayListHead++,
        TextureAddress(ImageBody(g_ShotPreviewDashImage)),
        G_IM_FMT_I,
        G_IM_SIZ_8b,
        SHOT_PREVIEW_IMAGE_SIZE,
        SHOT_PREVIEW_IMAGE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        TextureMask(SHOT_PREVIEW_IMAGE_SIZE),
        TextureMask(SHOT_PREVIEW_IMAGE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPSetTileSize(
        g_DisplayListHead++,
        G_TX_RENDERTILE,
        f_OpeningShotPathScroll,
        0,
        f_OpeningShotPathScroll + (SHOT_PREVIEW_IMAGE_SIZE << G_TEXTURE_IMAGE_FRAC),
        SHOT_PREVIEW_IMAGE_SIZE << G_TEXTURE_IMAGE_FRAC
    );
    gDPSetCombineMode(g_DisplayListHead++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    if (f_IsOpeningShotPathRed) {
        gDPSetPrimColor(g_DisplayListHead++, 0, 0, 0xFF, 0x00, 0x00, 0xC8);
    }
    else {
        gDPSetPrimColor(g_DisplayListHead++, 0, 0, 0xFF, 0xFF, 0xFF, 0x80);
    }

    gDPPipeSync(g_DisplayListHead++);
    for (step = 0; step < f_OpeningShotPathDashCount; step++) {
        gSPMatrix(
            g_DisplayListHead++,
            &f_OpeningShotDashMatrices[g_DrawBufferIndex][step],
            G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
        );
        gSPVertex(g_DisplayListHead++, f_OpeningShotDashVertices, QUAD_VERTEX_COUNT, 0);
        gSP2Triangles(g_DisplayListHead++, 0, 1, 2, 0, 0, 2, 3, 0);
        gSPPopMatrix(g_DisplayListHead++, G_MTX_MODELVIEW);
    }

    gDPPipeSync(g_DisplayListHead++);
}

#define MARIO_TEE_SHOT_ANIMATION 0x10
#define MARIO_CHEER_ANIMATION 0x11
#define MARIO_WALK_ANIMATION 0x12
#define MARIO_REACH_BALL_ANIMATION 0x13
#define MARIO_RUN_ANIMATION 0x14
#define MARIO_INSPECT_BALL_ANIMATION 0x15
#define MARIO_STAND_AT_BALL_ANIMATION 0x16
#define MARIO_LOOK_UP_ANIMATION 0x17
#define MARIO_SCRATCH_HEAD_ANIMATION 0x18
#define MARIO_GATHER_ANIMATION 0x19
#define PEACH_CHEER_ANIMATION 0x10
#define PEACH_TEE_SHOT_ANIMATION 0x11
#define PEACH_WALK_ANIMATION 0x12
#define PEACH_WAVE_ANIMATION 0x13
#define PEACH_APPROACH_ANIMATION 0x14
#define PEACH_INSPECT_BALL_ANIMATION 0x15
#define PEACH_COVER_MOUTH_ANIMATION 0x16
#define PEACH_GATHER_ANIMATION 0x17
#define PLUM_CHEER_ANIMATION 0x10
#define PLUM_TEE_SHOT_ANIMATION 0x11
#define PLUM_WALK_ANIMATION 0x12
#define PLUM_GATHER_ANIMATION 0x13
#define LUIGI_CHEER_ANIMATION 0x10
#define LUIGI_TEE_SHOT_ANIMATION 0x11
#define LUIGI_WALK_ANIMATION 0x12
#define LUIGI_SEARCH_ANIMATION 0x13
#define LUIGI_DISMAY_ANIMATION 0x14
#define YOSHI_CARRY_ANIMATION 0x10
#define DONKEY_KONG_CARRY_ANIMATION 0x11
#define BABY_MARIO_RIDE_YOSHI_ANIMATION 0x10
#define BABY_MARIO_RIDE_DONKEY_KONG_ANIMATION 0x11
#define TOAD_WAVE_FLAG_ANIMATION 0x10

#define PlaceCueCharacter(point, x, z, heading) \
    { \
        (point)[0] = (x); \
        (point)[2] = (z); \
        PlaceCharacter(f_CueSlotIndex, (point), DegreesToRadians(heading)); \
    }

Boolean UpdateOpeningCharacters(void) {
    CoursePoint marioTeeShotPoint;
    CoursePoint peachCheerPoint;
    CoursePoint luigiCheerPoint;
    CoursePoint peachTeeShotPoint;
    CoursePoint plumCheerPoint;
    CoursePoint plumTeeShotPoint;
    CoursePoint marioCheerPoint;
    CoursePoint luigiTeeShotPoint;
    CoursePoint plumWalkPoint;
    CoursePoint peachWalkPoint;
    CoursePoint marioWalkPoint;
    CoursePoint luigiWalkPoint;
    CoursePoint marioReachBallPoint;
    CoursePoint peachWavePoint;
    CoursePoint marioRunPoint;
    CoursePoint peachApproachPoint;
    CoursePoint marioInspectBallPoint;
    CoursePoint peachInspectBallPoint;
    CoursePoint marioStandAtBallPoint;
    CoursePoint marioLookUpPoint;
    CoursePoint carrierArrivalPoint;
    CoursePoint babyMarioArrivalPoint;
    CoursePoint peachCoverMouthPoint;
    CoursePoint marioScratchHeadPoint;
    CoursePoint luigiSearchPoint;
    CoursePoint toadWaveFlagPoint;
    CoursePoint luigiDismayPoint;
    CoursePoint peachGatherPoint;
    CoursePoint marioGatherPoint;
    CoursePoint plumGatherPoint;

    switch (f_OpeningCue) {
        case OPENING_CUE_MARIO_TEE_SHOT:
            SetCharacterLights();
            f_CueSlotIndex = 0;
            if (ToggleOpeningCharacter(0, OPENING_MODEL_MARIO)) {
                SetCharacterAnimation(f_CueSlotIndex, MARIO_TEE_SHOT_ANIMATION);
                PlaceCueCharacter(marioTeeShotPoint, 0xF3400, 0x490000, -30);
                f_LastPlacedSlotIndex = f_CueSlotIndex;
            }

            f_OpeningCue = OPENING_CUE_DONE;
            break;
        case OPENING_CUE_PEACH_CHEER:
            f_CueSlotIndex = 1;
            if (ToggleOpeningCharacter(1, OPENING_MODEL_PEACH)) {
                SetCharacterAnimation(f_CueSlotIndex, PEACH_CHEER_ANIMATION);
                PlaceCueCharacter(peachCheerPoint, 0xA0B58, 0x4A28BF, 80);
                f_OpeningCue = OPENING_CUE_DONE;
                f_LastPlacedSlotIndex = f_CueSlotIndex;
            }

            f_OpeningCue++;
            break;
        case OPENING_CUE_LUIGI_CHEER:
            f_CueSlotIndex = 2;
            if (ToggleOpeningCharacter(2, OPENING_MODEL_LUIGI)) {
                SetCharacterAnimation(f_CueSlotIndex, LUIGI_CHEER_ANIMATION);
                PlaceCueCharacter(luigiCheerPoint, 0x1368A0, 0x49D208, 0);
                f_LastPlacedSlotIndex = f_CueSlotIndex;
            }

            f_OpeningCue = OPENING_CUE_DONE;
            break;
        case OPENING_CUE_PEACH_TEE_SHOT:
            FreeCharacterSlot(0);
            FreeCharacterSlot(1);
            FreeCharacterSlot(2);
            f_CueSlotIndex = 3;
            if (ToggleOpeningCharacter(3, OPENING_MODEL_PEACH)) {
                SetCharacterAnimation(f_CueSlotIndex, PEACH_TEE_SHOT_ANIMATION);
                PlaceCueCharacter(peachTeeShotPoint, 0xF3400, 0x490000, -30);
                f_LastPlacedSlotIndex = f_CueSlotIndex;
            }

            f_OpeningCue = OPENING_CUE_DONE;
            break;
        case OPENING_CUE_PLUM_CHEER:
            f_CueSlotIndex = 0;
            if (ToggleOpeningCharacter(0, OPENING_MODEL_PLUM)) {
                SetCharacterAnimation(f_CueSlotIndex, PLUM_CHEER_ANIMATION);
                PlaceCueCharacter(plumCheerPoint, 0xB2A34, 0x4F9800, 10);
                f_LastPlacedSlotIndex = f_CueSlotIndex;
            }

            f_OpeningCue = OPENING_CUE_DONE;
            break;
        case OPENING_CUE_PLUM_TEE_SHOT:
            FreeCharacterSlot(0);
            FreeCharacterSlot(3);
            f_CueSlotIndex = 1;
            if (ToggleOpeningCharacter(1, OPENING_MODEL_PLUM)) {
                SetCharacterAnimation(f_CueSlotIndex, PLUM_TEE_SHOT_ANIMATION);
                PlaceCueCharacter(plumTeeShotPoint, 0xF3400, 0x490000, 0);
                f_LastPlacedSlotIndex = f_CueSlotIndex;
            }

            f_OpeningCue = OPENING_CUE_DONE;
            break;
        case OPENING_CUE_MARIO_CHEER:
            f_CueSlotIndex = 2;
            if (ToggleOpeningCharacter(2, OPENING_MODEL_MARIO)) {
                SetCharacterAnimation(f_CueSlotIndex, MARIO_CHEER_ANIMATION);
                PlaceCueCharacter(marioCheerPoint, 0x1610CF, 0x4AFA34, 0);
                f_LastPlacedSlotIndex = f_CueSlotIndex;
            }

            f_OpeningCue = OPENING_CUE_DONE;
            break;
        case OPENING_CUE_LUIGI_TEE_SHOT:
            FreeCharacterSlot(1);
            FreeCharacterSlot(2);
            f_CueSlotIndex = 0;
            if (ToggleOpeningCharacter(0, OPENING_MODEL_LUIGI)) {
                SetCharacterAnimation(f_CueSlotIndex, LUIGI_TEE_SHOT_ANIMATION);
                PlaceCueCharacter(luigiTeeShotPoint, 0xF3400, 0x490000, -45);
                f_LastPlacedSlotIndex = f_CueSlotIndex;
            }

            f_OpeningCue = OPENING_CUE_DONE;
            break;
        case OPENING_CUE_PEACH_PLUM_WALK:
            FreeCharacterSlot(0);
            f_CueSlotIndex = 1;
            if (ToggleOpeningCharacter(1, OPENING_MODEL_PLUM)) {
                SetCharacterAnimation(f_CueSlotIndex, PLUM_WALK_ANIMATION);
                PlaceCueCharacter(plumWalkPoint, 0x29EB94, 0x26DDDA, 0);
            }

            f_CueSlotIndex = 2;
            if (ToggleOpeningCharacter(2, OPENING_MODEL_PEACH)) {
                SetCharacterAnimation(f_CueSlotIndex, PEACH_WALK_ANIMATION);
                PlaceCueCharacter(peachWalkPoint, 0x29EB94, 0x26DDDA, 0);
                f_LastPlacedSlotIndex = f_CueSlotIndex;
            }

            f_OpeningCue = OPENING_CUE_DONE;
            break;
        case OPENING_CUE_MARIO_LUIGI_WALK:
            FreeCharacterSlot(1);
            FreeCharacterSlot(2);
            f_CueSlotIndex = 0;
            if (ToggleOpeningCharacter(0, OPENING_MODEL_MARIO)) {
                SetCharacterAnimation(f_CueSlotIndex, MARIO_WALK_ANIMATION);
                PlaceCueCharacter(marioWalkPoint, 0x2709F8, 0x1CF4B4, 10);
            }

            f_CueSlotIndex = 3;
            if (ToggleOpeningCharacter(3, OPENING_MODEL_LUIGI)) {
                SetCharacterAnimation(f_CueSlotIndex, LUIGI_WALK_ANIMATION);
                PlaceCueCharacter(luigiWalkPoint, 0x2709F8, 0x1CF4B4, 55);
                f_LastPlacedSlotIndex = f_CueSlotIndex;
            }

            f_OpeningCue = OPENING_CUE_DONE;
            break;
        case OPENING_CUE_MARIO_REACH_BALL:
            FreeCharacterSlot(0);
            f_CueSlotIndex = 1;
            if (ToggleOpeningCharacter(1, OPENING_MODEL_MARIO)) {
                SetCharacterAnimation(f_CueSlotIndex, MARIO_REACH_BALL_ANIMATION);
                PlaceCueCharacter(marioReachBallPoint, 0x2709F8, 0x1CF4B4, 10);
            }

            f_OpeningCue = OPENING_CUE_DONE;
            break;
        case OPENING_CUE_PEACH_WAVE:
            FreeCharacterSlot(1);
            FreeCharacterSlot(3);
            f_CueSlotIndex = 0;
            if (ToggleOpeningCharacter(0, OPENING_MODEL_PEACH)) {
                SetCharacterAnimation(f_CueSlotIndex, PEACH_WAVE_ANIMATION);
                PlaceCueCharacter(peachWavePoint, 0x2CA5D3, 0x22A928, -98);
            }

            f_CueSlotIndex = 2;
            if (ToggleOpeningCharacter(2, OPENING_MODEL_MARIO)) {
                SetCharacterAnimation(f_CueSlotIndex, MARIO_RUN_ANIMATION);
                PlaceCueCharacter(marioRunPoint, 0x2C42D6, 0x22A832, -98);
                f_LastPlacedSlotIndex = f_CueSlotIndex;
            }

            f_OpeningCue = OPENING_CUE_DONE;
            break;
        case OPENING_CUE_PEACH_APPROACH:
            FreeCharacterSlot(0);
            FreeCharacterSlot(2);
            f_CueSlotIndex = 1;
            if (ToggleOpeningCharacter(1, OPENING_MODEL_PEACH)) {
                SetCharacterAnimation(f_CueSlotIndex, PEACH_APPROACH_ANIMATION);
                PlaceCueCharacter(peachApproachPoint, 0x2B1194, 0x249320, -98);
            }

            f_CueSlotIndex = 3;
            if (ToggleOpeningCharacter(3, OPENING_MODEL_MARIO)) {
                SetCharacterAnimation(f_CueSlotIndex, MARIO_INSPECT_BALL_ANIMATION);
                PlaceCueCharacter(marioInspectBallPoint, 0x2B1194, 0x249320, -98);
                f_LastPlacedSlotIndex = f_CueSlotIndex;
            }

            f_OpeningCue = OPENING_CUE_DONE;
            break;
        case OPENING_CUE_BALL_INSPECTION:
            FreeCharacterSlot(1);
            FreeCharacterSlot(3);
            f_CueSlotIndex = 0;
            if (ToggleOpeningCharacter(0, OPENING_MODEL_PEACH)) {
                SetCharacterAnimation(f_CueSlotIndex, PEACH_INSPECT_BALL_ANIMATION);
                PlaceCueCharacter(peachInspectBallPoint, 0x2B1194, 0x249320, -98);
            }

            f_CueSlotIndex = 2;
            if (ToggleOpeningCharacter(2, OPENING_MODEL_MARIO)) {
                SetCharacterAnimation(f_CueSlotIndex, MARIO_STAND_AT_BALL_ANIMATION);
                PlaceCueCharacter(marioStandAtBallPoint, 0x2B1194, 0x249320, -98);
                f_LastPlacedSlotIndex = f_CueSlotIndex;
            }

            f_OpeningCue = OPENING_CUE_DONE;
            break;
        case OPENING_CUE_MARIO_LOOK_UP:
            FreeCharacterSlot(2);
            f_CueSlotIndex = 1;
            ToggleOpeningCharacter(1, OPENING_MODEL_MARIO);
            SetCharacterAnimation(f_CueSlotIndex, MARIO_LOOK_UP_ANIMATION);
            PlaceCueCharacter(marioLookUpPoint, 0x2B1194, 0x249320, -98);
            f_OpeningCue = OPENING_CUE_DONE;
            f_LastPlacedSlotIndex = f_CueSlotIndex;
            break;
        case OPENING_CUE_FREEZE:
            SetAnimationSpeed(0, 0.0f);
            SetAnimationSpeed(1, 0.0f);
            f_OpeningCue = OPENING_CUE_DONE;
            break;
        case OPENING_CUE_UNFREEZE:
            SetAnimationSpeed(0, 1.0f);
            SetAnimationSpeed(1, 1.0f);
            f_OpeningCue = OPENING_CUE_DONE;
            break;
        case OPENING_CUE_BABY_MARIO_ARRIVAL:
            FreeCharacterSlot(0);
            FreeCharacterSlot(1);
            f_CueSlotIndex = 2;
            if (f_IsDonkeyKongShown) {
                ToggleOpeningCharacter(2, OPENING_MODEL_DONKEY_KONG);
            }
            else {
                ToggleOpeningCharacter(2, OPENING_MODEL_YOSHI);
            }

            SetAnimationSpeed(2, 0.0f);
            if (f_IsDonkeyKongShown) {
                SetCharacterAnimation(f_CueSlotIndex, DONKEY_KONG_CARRY_ANIMATION);
            }
            else {
                SetCharacterAnimation(f_CueSlotIndex, YOSHI_CARRY_ANIMATION);
            }

            PlaceCueCharacter(carrierArrivalPoint, 0x202B29, 0xCE7DA, -165);
            f_CueSlotIndex = 3;
            ToggleOpeningCharacter(3, OPENING_MODEL_BABY_MARIO);
            SetAnimationSpeed(3, 0.0f);
            if (f_IsDonkeyKongShown) {
                SetCharacterAnimation(f_CueSlotIndex, BABY_MARIO_RIDE_DONKEY_KONG_ANIMATION);
            }
            else {
                SetCharacterAnimation(f_CueSlotIndex, BABY_MARIO_RIDE_YOSHI_ANIMATION);
            }

            PlaceCueCharacter(babyMarioArrivalPoint, 0x202B29, 0xCE7DA, -165);
            f_OpeningCue = OPENING_CUE_DONE;
            f_LastPlacedSlotIndex = f_CueSlotIndex;
            break;
        case OPENING_CUE_BABY_MARIO_RIDE:
            SetAnimationSpeed(2, 1.0f);
            SetAnimationSpeed(3, 1.0f);
            break;
        case OPENING_CUE_MARIO_PUZZLED:
            f_CueSlotIndex = 0;
            ToggleOpeningCharacter(0, OPENING_MODEL_PEACH);
            SetCharacterAnimation(f_CueSlotIndex, PEACH_COVER_MOUTH_ANIMATION);
            PlaceCueCharacter(peachCoverMouthPoint, 0x2B1194, 0x249320, -98);
            f_CueSlotIndex = 1;
            ToggleOpeningCharacter(1, OPENING_MODEL_MARIO);
            SetCharacterAnimation(f_CueSlotIndex, MARIO_SCRATCH_HEAD_ANIMATION);
            PlaceCueCharacter(marioScratchHeadPoint, 0x2B1194, 0x249320, -98);
            f_OpeningCue = OPENING_CUE_DONE;
            f_LastPlacedSlotIndex = f_CueSlotIndex;
            break;
        case OPENING_CUE_CLEAR_CHARACTERS:
            FreeAllCharacterSlots();
            f_OpeningCue = OPENING_CUE_DONE;
            break;
        case OPENING_CUE_LUIGI_SEARCH:
            f_CueSlotIndex = 0;
            ToggleOpeningCharacter(0, OPENING_MODEL_LUIGI);
            SetCharacterAnimation(f_CueSlotIndex, LUIGI_SEARCH_ANIMATION);
            PlaceCueCharacter(luigiSearchPoint, 0x16308C, 0x183EDA, 170);
            f_OpeningCue = OPENING_CUE_DONE;
            f_LastPlacedSlotIndex = f_CueSlotIndex;
            break;
        case OPENING_CUE_TOAD_FLAG:
            f_CueSlotIndex = 1;
            ToggleOpeningCharacter(1, OPENING_MODEL_TOAD);
            SetCharacterAnimation(f_CueSlotIndex, TOAD_WAVE_FLAG_ANIMATION);
            PlaceCueCharacter(toadWaveFlagPoint, 0xE3F2C, 0x1D98E6, 70);
            f_OpeningCue = OPENING_CUE_DONE;
            f_LastPlacedSlotIndex = f_CueSlotIndex;
            break;
        case OPENING_CUE_LUIGI_DISMAY:
            FreeCharacterSlot(0);
            FreeCharacterSlot(1);
            f_CueSlotIndex = 2;
            ToggleOpeningCharacter(2, OPENING_MODEL_LUIGI);
            SetCharacterAnimation(f_CueSlotIndex, LUIGI_DISMAY_ANIMATION);
            PlaceCueCharacter(luigiDismayPoint, 0x16308C, 0x183EDA, 185);
            f_LastPlacedSlotIndex = f_CueSlotIndex;
            f_CueSlotIndex = 3;
            if (ToggleOpeningCharacter(3, OPENING_MODEL_PEACH)) {
                SetCharacterAnimation(f_CueSlotIndex, PEACH_GATHER_ANIMATION);
                PlaceCueCharacter(peachGatherPoint, 0x16806D, 0x17E632, 200);
            }

            f_OpeningCue++;
            break;
        case OPENING_CUE_GATHERING:
            f_CueSlotIndex = 0;
            if (ToggleOpeningCharacter(0, OPENING_MODEL_MARIO)) {
                SetCharacterAnimation(f_CueSlotIndex, MARIO_GATHER_ANIMATION);
                PlaceCueCharacter(marioGatherPoint, 0x15E14D, 0x18155C, 170);
            }

            f_CueSlotIndex = 1;
            if (ToggleOpeningCharacter(1, OPENING_MODEL_PLUM)) {
                SetCharacterAnimation(f_CueSlotIndex, PLUM_GATHER_ANIMATION);
                PlaceCueCharacter(plumGatherPoint, 0x166EDF, 0x1838C7, 200);
                f_LastPlacedSlotIndex = f_CueSlotIndex;
            }

            f_OpeningCue = OPENING_CUE_DONE;
            break;
        default:
            f_OpeningCue = OPENING_CUE_INVALID;
            break;
    }

    return f_OpeningCue == OPENING_CUES_FINISHED;
}

#define OPENING_SCENE_MARIO_DRIVE 0
#define OPENING_SCENE_MARIO_DRIVE_CHEERS 20
#define OPENING_SCENE_UNREACHED 30
#define OPENING_SCENE_MARIO_DRIVE_FLIGHT 40
#define OPENING_SCENE_PEACH_DRIVE 50
#define OPENING_SCENE_PLUM_CHEER 60
#define OPENING_SCENE_PEACH_DRIVE_FLIGHT 70
#define OPENING_SCENE_PLUM_DRIVE 80
#define OPENING_SCENE_MARIO_CLOSE_UP 90
#define OPENING_SCENE_MARIO_CHEER 100
#define OPENING_SCENE_LUIGI_DRIVE 110
#define OPENING_SCENE_PEACH_PLUM_WALK 120
#define OPENING_SCENE_MARIO_LUIGI_WALK 130
#define OPENING_SCENE_MARIO_FINDS_BALL 140
#define OPENING_SCENE_PEACH_WAVE 150
#define OPENING_SCENE_BALL_ROLLS_IN 160
#define OPENING_SCENE_PEACH_APPROACH 170
#define OPENING_SCENE_BALL_INSPECTION 180
#define OPENING_SCENE_INCOMING_BALL 190
#define OPENING_SCENE_BABY_MARIO_ARRIVAL 200
#define OPENING_SCENE_MARIO_PUZZLED 210
#define OPENING_SCENE_LUIGI_SEARCH 220
#define OPENING_SCENE_OUT_OF_BOUNDS 230
#define OPENING_SCENE_TOAD_FLAG 240
#define OPENING_SCENE_GATHERING 250

#define OPENING_SCENE_LOGO_BALL_MARIO 200
#define LOGO_BALL_MARIO_FRAMES_LEFT 98

#define LOGO_BALL_SLOT 1

#define OPENING_FRAME_LIMIT 9000

#define CLOSING_SOUND_CHANNEL ROTATING_SOUND_CHANNEL_COUNT

void AnimateNameLogos(void);
void LoadNameLogo(s32 logoIndex, s32 slot);
void MoveLakitu(void);

#define OPENING_SCENE_SKIPPED 998
#define SKIP_FADE_WHITE_LEVEL 0xFF
#define SKIP_FADE_FRAME_COUNT 4
#define SKIP_BUTTONS (A_BUTTON | B_BUTTON | START_BUTTON)
#define DEBUG_SKIP_BUTTONS (A_BUTTON | START_BUTTON)
#define OPENING_DEBUG_SKIP_CONTROLLER 0
#define OPENING_SCENE_CONTROLLER 1
#define OPENING_EDITOR_CONTROLLER 2
#define UNUSED_STICK_OFFSET_SCALE 0.002f
#define SCRIPT_SOUND_CHANNEL_COUNT ALTERNATING_SOUND_CHANNEL
#define FRAME_COUNT_TEXT_COLUMN 30
#define FRAME_COUNT_TEXT_ROW 26
#define SCENE_TEXT_COLUMN 4
#define FRAMES_LEFT_TEXT_COLUMN 29
#define SCENE_TEXT_ROW 2

void UpdateOpening(void) {
    Vec3 point;
    Vec3 logoBallPoint;
    Vec3 handPoint;
    s32 slotIndex;
    SoundChannel channel;
    s32 controllerIndex;
    s32 pressedButtons;
    s32 skipButtons;

    if (IsGameFlagSet(GAME_FLAG_EDIT_OPENING_CAMERA)) {
        EditOpeningCamera();
        if (g_ControllerStates[OPENING_EDITOR_CONTROLLER].pressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            ClearGameFlag(GAME_FLAG_EDIT_OPENING_CAMERA);
            f_IsLookAtMarkerShown = FALSE;
            ClearDebugText();
        }
    }
    else {
        if (f_OpeningFrameCount < OPENING_FRAME_LIMIT) {
            f_OpeningFrameCount++;
        }
        else {
            f_OpeningScene = OPENING_SCENE_END;
        }

        if (g_IsDebugEnabled) {
            sprintf(g_PrintBuffer, "%04d", (int)f_OpeningFrameCount);
            PrintDebugText(g_PrintBuffer, FRAME_COUNT_TEXT_COLUMN, FRAME_COUNT_TEXT_ROW);
            sprintf(g_PrintBuffer, "CAMERA:%d    ", (int)f_OpeningScene);
            PrintDebugText(g_PrintBuffer, SCENE_TEXT_COLUMN, SCENE_TEXT_ROW);
            sprintf(g_PrintBuffer, "(%d)    ", (int)f_OpeningSceneFramesLeft);
            PrintDebugText(g_PrintBuffer, FRAMES_LEFT_TEXT_COLUMN, SCENE_TEXT_ROW);
        }

        switch (f_OpeningScene) {
            case OPENING_SCENE_MARIO_DRIVE:
                g_ScreenFadeDuration = 1;
                g_ScreenFadeFramesLeft = 1;
                g_ScreenFadeKind = FADE_IN;
                g_ScreenFadeColor.red = 0;
                g_ScreenFadeColor.green = 0;
                g_ScreenFadeColor.blue = 0;
                f_OpeningSceneFramesLeft = 10;
                f_OpeningScene++;
                break;
            case OPENING_SCENE_MARIO_DRIVE + 1:
                g_ScreenFadeDuration = 1;
                g_ScreenFadeFramesLeft = 1;
                g_ScreenFadeKind = FADE_IN;
                g_ScreenFadeColor.red = 0;
                g_ScreenFadeColor.green = 0;
                g_ScreenFadeColor.blue = 0;
                if (f_OpeningSceneFramesLeft == 0) {
                    f_OpeningSceneFramesLeft = 30;
                    LoadNameLogo(0, 0);
                    f_OpeningCue = OPENING_CUE_MARIO_TEE_SHOT;
                    StartScreenFadeIn(0, 0, 0, 0x1E);
                    PlaceEditorCamera(997.598f, -160.581f, 4594.014f, 973.0f, -130.0f, 4672.0f);
                    ZoomEditorCamera(EDITOR_POINT_LOOK_AT, 38.01875f, 0x3C);
                    TurnEditorCamera(EDITOR_POINT_LOOK_AT, -0.8528241f, 0x28);
                    TiltEditorCamera(EDITOR_POINT_LOOK_AT, -0.324127f, 0x28);
                    f_OpeningScene++;
                    LoadOpeningShotBall();
                    f_OpeningShotSimulation.position[AXIS_X] = 0xF1B96;
                    f_OpeningShotSimulation.position[AXIS_Z] = 0x48CFDC;
                    f_OpeningShotSimulation.position[AXIS_Y] = GetGroundHeight(0xF1B96, 0x48CFDC);
                    f_ShotBallScale = 2;
                    f_OpeningSparksOrigin[AXIS_X] =
                        f_OpeningShotSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE;
                    f_OpeningSparksOrigin[AXIS_Y] =
                        f_OpeningShotSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                    f_OpeningSparksOrigin[AXIS_Z] =
                        f_OpeningShotSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                    ClearEffects();
                    f_OpeningShotImpactPoint[0] = f_OpeningShotSimulation.position[AXIS_X];
                    f_OpeningShotImpactPoint[1] = f_OpeningShotSimulation.position[AXIS_Y];
                    f_OpeningShotImpactPoint[2] = f_OpeningShotSimulation.position[AXIS_Z];
                }

                break;
            case OPENING_SCENE_MARIO_DRIVE + 2:
                if (f_OpeningSceneFramesLeft == 0) {
                    f_IsNameSlotShown[0] = TRUE;
                    PlaySong(SONG_OPENING, 0);
                }

                if (AreEditorMovesDone()) {
                    f_OpeningSceneFramesLeft = 40;
                    f_OpeningScene++;
                }

                break;
            case OPENING_SCENE_MARIO_DRIVE + 3:
                if (f_OpeningSceneFramesLeft == 28) {
                    PlaySound(SOUND_CONTACT_PLAIN, f_OpeningSoundChannel, SOUND_FULL_SCALE);
                    AdvanceOpeningSoundChannel();
                }

                if (f_OpeningSceneFramesLeft == 24) {
                    PlaySoundWithEffects(
                        SOUND_SHOT_SPARKS,
                        f_OpeningSoundChannel,
                        0x4C,
                        NO_REVERB,
                        0xA7
                    );
                    AdvanceOpeningSoundChannel();
                    f_OpeningSparksState = 1;
                    SpawnDrop(f_OpeningShotImpactPoint);
                }

                if (f_OpeningSceneFramesLeft == 24) {
                    f_OpeningShotSimulation.playerIndex = 0;
                    f_OpeningShotSimulation.isHoledOut = FALSE;
                    f_OpeningShotSimulation.state = BALL_STATE_STOPPED;
                    f_OpeningShotSimulation.surface = SURFACE_TEE_UP;
                    LaunchOpeningBallSimulation(&f_OpeningShotSimulation, 0, &f_OpeningFirstShot);
                    f_OpeningShotState = OPENING_SHOT_FLYING;
                    f_ShotBallScale = 1;
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    f_OpeningCue = OPENING_CUE_PEACH_CHEER;
                    f_OpeningScene = OPENING_SCENE_MARIO_DRIVE_CHEERS;
                }

                break;
            case OPENING_SCENE_MARIO_DRIVE_CHEERS:
                PlaceEditorCamera(679.6445f, -98.28f, 4736.7f, 642.835f, -99.0f, 4746.187f);
                ZoomEditorCamera(EDITOR_POINT_LOOK_AT, 18.469883f, 0x1E);
                MoveEditorCamera(EDITOR_POINT_LOOK_AT, 642.835f, -100.5f, 4746.187f, 0x1E);
                f_IsNameSlotShown[0] = FALSE;
                f_OpeningShotState = OPENING_SHOT_IDLE;
                f_ShotBallScale = 0;
                f_OpeningSceneFramesLeft = 30;
                f_OpeningScene++;
                ClearEffects();
                break;
            case OPENING_SCENE_MARIO_DRIVE_CHEERS + 1:
                switch (f_OpeningSceneFramesLeft) {
                    case 25:
                        PlaySound(
                            SOUND_PEACH_OPENING_VOICE,
                            f_OpeningSoundChannel,
                            SOUND_FULL_SCALE
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 23:
                        PlaySoundWithEffects(
                            SOUND_PLUM_OPENING_VOICE,
                            f_OpeningSoundChannel,
                            0x46,
                            NO_REVERB,
                            0xBB
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 20:
                        PlaySoundWithEffects(
                            SOUND_MARIO_CALL_OUT_UP,
                            f_OpeningSoundChannel,
                            0x46,
                            NO_REVERB,
                            0x57
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                }

                if (AreEditorMovesDone()) {
                    StopEditorMoves();
                    f_OpeningScene++;
                }

                break;
            case OPENING_SCENE_MARIO_DRIVE_CHEERS + 2:
                f_OpeningCue = OPENING_CUE_LUIGI_CHEER;
                f_OpeningScene = OPENING_SCENE_MARIO_DRIVE_FLIGHT;
                break;
            case OPENING_SCENE_UNREACHED:
                PlaceEditorCamera(1400.411f, -101.306f, 4781.043f, 1414.02f, -107.0f, 4800.41f);
                f_OpeningSceneFramesLeft = 14;
                f_OpeningScene++;
                break;
            case OPENING_SCENE_UNREACHED + 1:
                if (f_OpeningSceneFramesLeft == 8) {
                    SetAnimationSpeed(2, 0.0f);
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    f_OpeningScene = OPENING_SCENE_MARIO_DRIVE_FLIGHT;
                }

                break;
            case OPENING_SCENE_MARIO_DRIVE_FLIGHT:
                PlaceEditorCamera(1269.768f, -127.806f, 4719.564f, 1179.989f, -111.987f, 4715.451f);
                f_OpeningShotSimulation.position[AXIS_X] = 0xF1B96;
                f_OpeningShotSimulation.position[AXIS_Z] = 0x48CFDC;
                f_OpeningShotSimulation.position[AXIS_Y] = GetGroundHeight(0xF1B96, 0x48CFDC);
                f_OpeningShotSimulation.playerIndex = 0;
                f_OpeningShotSimulation.isHoledOut = FALSE;
                f_OpeningShotSimulation.state = BALL_STATE_STOPPED;
                f_OpeningShotSimulation.surface = SURFACE_TEE_UP;
                LaunchOpeningBallSimulation(&f_OpeningShotSimulation, 0, &f_OpeningFirstShot);
                f_OpeningShotState = OPENING_SHOT_FLYING;
                f_ShotBallScale = 1;
                f_OpeningSparksState = 1;
                f_OpeningSceneFramesLeft = 20;
                f_OpeningScene++;
                break;
            case OPENING_SCENE_MARIO_DRIVE_FLIGHT + 1:
                if (f_OpeningSceneFramesLeft == 14) {
                    PlaySoundWithEffects(
                        SOUND_LUIGI_CALL_OUT_RIGHT,
                        f_OpeningSoundChannel,
                        SOUND_FULL_SCALE,
                        NO_REVERB,
                        SOUND_FULL_SCALE
                    );
                    AdvanceOpeningSoundChannel();
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    f_LookAtEaseMode = LOOK_AT_EASE_FAST;
                    f_OpeningSceneFramesLeft = 50;
                    f_OpeningScene++;
                }

                break;
            case OPENING_SCENE_MARIO_DRIVE_FLIGHT + 2:
                if (f_OpeningSceneFramesLeft == 0) {
                    StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
                    f_OpeningScene = OPENING_SCENE_PEACH_DRIVE;
                    LoadNameLogo(2, 0);
                }

                break;
            case OPENING_SCENE_PEACH_DRIVE:
                PlaceEditorCamera(957.619f, -131.446f, 4629.353f, 973.561f, -133.0f, 4671.789f);
                f_OpeningCue = OPENING_CUE_PEACH_TEE_SHOT;
                f_LookAtEaseMode = LOOK_AT_EASE_NONE;
                f_OpeningSceneFramesLeft = 100;
                f_OpeningShotState = OPENING_SHOT_IDLE;
                f_OpeningShotSimulation.position[AXIS_X] = 0xF1882;
                f_OpeningShotSimulation.position[AXIS_Z] = 0x48D4C4;
                f_OpeningScene++;
                f_OpeningShotSimulation.position[AXIS_Y] = GetGroundHeight(0xF1882, 0x48D4C4);
                f_ShotBallScale = 2;
                f_OpeningShotImpactPoint[0] = f_OpeningShotSimulation.position[AXIS_X];
                f_OpeningShotImpactPoint[1] = f_OpeningShotSimulation.position[AXIS_Y];
                f_OpeningShotImpactPoint[2] = f_OpeningShotSimulation.position[AXIS_Z];
                f_OpeningSparksOrigin[AXIS_X] =
                    f_OpeningShotSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE;
                f_OpeningSparksOrigin[AXIS_Y] =
                    f_OpeningShotSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                f_OpeningSparksOrigin[AXIS_Z] =
                    f_OpeningShotSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                break;
            case OPENING_SCENE_PEACH_DRIVE + 1:
                if (f_OpeningSceneFramesLeft == 24) {
                    PlaySound(SOUND_MENU_START, f_OpeningSoundChannel, SOUND_FULL_SCALE);
                    AdvanceOpeningSoundChannel();
                }

                if (f_OpeningSceneFramesLeft == 20) {
                    PlaySoundWithEffects(
                        SOUND_SHOT_SPARKS,
                        f_OpeningSoundChannel,
                        0x4C,
                        NO_REVERB,
                        0xA7
                    );
                    AdvanceOpeningSoundChannel();
                    f_OpeningSparksState = 1;
                    SpawnDrop(f_OpeningShotImpactPoint);
                }

                if (f_OpeningSceneFramesLeft == 22) {
                    f_OpeningShotSimulation.playerIndex = 0;
                    f_OpeningShotSimulation.isHoledOut = FALSE;
                    f_OpeningShotSimulation.state = BALL_STATE_STOPPED;
                    f_OpeningShotSimulation.surface = SURFACE_TEE_UP;
                    LaunchOpeningBallSimulation(&f_OpeningShotSimulation, 0, &f_OpeningSecondShot);
                    f_OpeningShotState = OPENING_SHOT_FLYING;
                }

                if (f_OpeningSceneFramesLeft == 90) {
                    f_IsNameSlotShown[0] = TRUE;
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    f_OpeningScene = OPENING_SCENE_PLUM_CHEER;
                }

                break;
            case OPENING_SCENE_PLUM_CHEER:
                PlaceEditorCamera(726.63f, -98.049f, 5063.849f, 713.268f, -96.0f, 5094.7f);
                MoveEditorPoint(EDITOR_POINT_LOOK_AT, 713.268f, -102.0f, 5094.7f, 0x28);
                MoveEditorPoint(EDITOR_POINT_EYE, 708.085f, -98.243f, 5073.215f, 0x28);
                f_OpeningSceneFramesLeft = 40;
                f_OpeningScene = OPENING_SCENE_PEACH_DRIVE_FLIGHT;
                f_OpeningCue = OPENING_CUE_PLUM_CHEER;
                f_IsNameSlotShown[0] = FALSE;
                f_OpeningShotState = OPENING_SHOT_IDLE;
                f_ShotBallScale = 0;
                PlaySoundWithEffects(
                    SOUND_PEACH_CALL_OUT_UP,
                    f_OpeningSoundChannel,
                    0x32,
                    NO_REVERB,
                    0x2F
                );
                AdvanceOpeningSoundChannel();
                ClearEffects();
                break;
            case OPENING_SCENE_PEACH_DRIVE_FLIGHT:
                switch (f_OpeningSceneFramesLeft) {
                    case 35:
                        PlaySoundWithEffects(
                            SOUND_MARIO_CALL_OUT_UP,
                            f_OpeningSoundChannel,
                            0x50,
                            NO_REVERB,
                            0xA7
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 33:
                        PlaySound(SOUND_PLUM_CALL_OUT_UP, f_OpeningSoundChannel, SOUND_FULL_SCALE);
                        AdvanceOpeningSoundChannel();
                        break;
                    case 31:
                        PlaySoundWithEffects(
                            SOUND_LUIGI_OPENING_VOICE,
                            f_OpeningSoundChannel,
                            0x50,
                            NO_REVERB,
                            0xBB
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                }

                if (f_OpeningSceneFramesLeft == 20) {
                    f_OpeningShotSimulation.position[AXIS_X] = 0xF1B96;
                    f_OpeningShotSimulation.position[AXIS_Z] = 0x48CFDC;
                    f_OpeningShotSimulation.position[AXIS_Y] = GetGroundHeight(0xF1B96, 0x48CFDC);
                    f_OpeningShotSimulation.playerIndex = 0;
                    f_OpeningShotSimulation.isHoledOut = FALSE;
                    f_OpeningShotSimulation.state = BALL_STATE_STOPPED;
                    f_OpeningShotSimulation.surface = SURFACE_TEE_UP;
                    LaunchOpeningBallSimulation(&f_OpeningShotSimulation, 0, &f_OpeningSecondShot);
                    f_OpeningShotState = OPENING_SHOT_FLYING;
                }

                if (AreEditorMovesDone()) {
                    PlaceEditorCamera(
                        973.156f,
                        -317.049f,
                        3703.677f,
                        1276.871f,
                        -112.538f,
                        4540.245f
                    );
                    f_ShotBallScale = 1;
                    f_LookAtEaseMode = LOOK_AT_EASE_FAST;
                    f_OpeningSceneFramesLeft = 70;
                    f_OpeningScene++;
                    AdvanceOpeningSoundChannel();
                }

                break;
            case OPENING_SCENE_PEACH_DRIVE_FLIGHT + 1:
                if (f_OpeningSceneFramesLeft == 0) {
                    f_OpeningScene = OPENING_SCENE_PLUM_DRIVE;
                    StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
                }

                break;
            case OPENING_SCENE_PLUM_DRIVE:
                PlaceEditorCamera(980.776f, -123.436f, 4636.89f, 971.837f, -133.5f, 4671.701f);
                f_OpeningSceneFramesLeft = 100;
                f_OpeningCue = OPENING_CUE_PLUM_TEE_SHOT;
                f_OpeningScene++;
                LoadNameLogo(3, 0);
                f_OpeningShotState = OPENING_SHOT_IDLE;
                f_LookAtEaseMode = LOOK_AT_EASE_NONE;
                f_OpeningShotSimulation.position[AXIS_X] = 0xF3930;
                f_OpeningShotSimulation.position[AXIS_Z] = 0x48D1D1;
                f_OpeningShotSimulation.position[AXIS_Y] = GetGroundHeight(0xF3930, 0x48D1D1);
                f_ShotBallScale = 2;
                f_OpeningShotImpactPoint[0] = f_OpeningShotSimulation.position[AXIS_X];
                f_OpeningShotImpactPoint[1] = f_OpeningShotSimulation.position[AXIS_Y];
                f_OpeningShotImpactPoint[2] = f_OpeningShotSimulation.position[AXIS_Z];
                f_OpeningSparksOrigin[AXIS_X] =
                    f_OpeningShotSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE;
                f_OpeningSparksOrigin[AXIS_Y] =
                    f_OpeningShotSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                f_OpeningSparksOrigin[AXIS_Z] =
                    f_OpeningShotSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                break;
            case OPENING_SCENE_PLUM_DRIVE + 1:
                if (f_OpeningSceneFramesLeft == 24) {
                    PlaySound(SOUND_MENU_START, f_OpeningSoundChannel, SOUND_FULL_SCALE);
                    AdvanceOpeningSoundChannel();
                }

                if (f_OpeningSceneFramesLeft == 20) {
                    PlaySoundWithEffects(
                        SOUND_SHOT_SPARKS,
                        f_OpeningSoundChannel,
                        0x4C,
                        NO_REVERB,
                        0xA7
                    );
                    AdvanceOpeningSoundChannel();
                    f_OpeningSparksState = 1;
                    SpawnDrop(f_OpeningShotImpactPoint);
                }

                if (f_OpeningSceneFramesLeft == 20) {
                    f_OpeningShotSimulation.playerIndex = 0;
                    f_OpeningShotSimulation.isHoledOut = FALSE;
                    f_OpeningShotSimulation.state = BALL_STATE_STOPPED;
                    f_OpeningShotSimulation.surface = SURFACE_TEE_UP;
                    LaunchOpeningBallSimulation(&f_OpeningShotSimulation, 0, &f_OpeningThirdShot);
                    f_OpeningShotState = OPENING_SHOT_FLYING;
                }

                if (f_OpeningSceneFramesLeft == 90) {
                    f_IsNameSlotShown[0] = TRUE;
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    f_OpeningScene = OPENING_SCENE_MARIO_CLOSE_UP;
                    f_OpeningCue = OPENING_CUE_MARIO_CHEER;
                }

                break;
            case OPENING_SCENE_MARIO_CLOSE_UP:
                PlaceEditorCamera(1402.165f, -101.306f, 4774.711f, 1412.049f, -107.0f, 4798.309f);
                TiltEditorCamera(EDITOR_POINT_LOOK_AT, 0.183044f, 0x32);
                TurnEditorCamera(EDITOR_POINT_LOOK_AT, 1.1519989f, 0x32);
                ZoomEditorCamera(EDITOR_POINT_LOOK_AT, 47.392056f, 0x1E);
                f_OpeningSceneFramesLeft = 50;
                f_OpeningScene = OPENING_SCENE_MARIO_CHEER;
                f_IsNameSlotShown[0] = FALSE;
                f_OpeningShotState = OPENING_SHOT_IDLE;
                f_ShotBallScale = 0;
                ClearEffects();
                break;
            case OPENING_SCENE_MARIO_CHEER:
                switch (f_OpeningSceneFramesLeft) {
                    case 40:
                        PlaySound(
                            SOUND_MARIO_OPENING_VOICE,
                            f_OpeningSoundChannel,
                            SOUND_FULL_SCALE
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 38:
                        PlaySoundWithEffects(
                            SOUND_LUIGI_CALL_OUT_UP,
                            f_OpeningSoundChannel,
                            0x46,
                            NO_REVERB,
                            0xBB
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 35:
                        PlaySoundWithEffects(
                            SOUND_PEACH_OPENING_VOICE,
                            f_OpeningSoundChannel,
                            0x36,
                            NO_REVERB,
                            0x57
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    StopEditorMoves();
                    StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
                    f_OpeningScene++;
                }

                break;
            case OPENING_SCENE_MARIO_CHEER + 1:
                PlaceEditorCamera(957.619f, -131.446f, 4629.353f, 973.561f, -133.0f, 4671.789f);
                TurnEditorCamera(EDITOR_POINT_LOOK_AT, 1.0471976f, 1);
                TiltEditorCamera(EDITOR_POINT_LOOK_AT, 0.34906584f, 1);
                f_OpeningCue = OPENING_CUE_LUIGI_TEE_SHOT;
                f_OpeningScene++;
                LoadNameLogo(1, 0);
                f_OpeningSceneFramesLeft = 10;
                f_OpeningShotSimulation.position[AXIS_X] = 0xF190E;
                f_OpeningShotSimulation.position[AXIS_Z] = 0x48D416;
                f_OpeningShotSimulation.position[AXIS_Y] = GetGroundHeight(0xF190E, 0x48D416);
                f_ShotBallScale = 2;
                f_OpeningShotImpactPoint[0] = f_OpeningShotSimulation.position[AXIS_X];
                f_OpeningShotImpactPoint[1] = f_OpeningShotSimulation.position[AXIS_Y];
                f_OpeningShotImpactPoint[2] = f_OpeningShotSimulation.position[AXIS_Z];
                f_OpeningSparksOrigin[AXIS_X] =
                    f_OpeningShotSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE;
                f_OpeningSparksOrigin[AXIS_Y] =
                    f_OpeningShotSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                f_OpeningSparksOrigin[AXIS_Z] =
                    f_OpeningShotSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                break;
            case OPENING_SCENE_MARIO_CHEER + 2:
                TurnEditorCamera(EDITOR_POINT_LOOK_AT, -1.0471976f, 0x50);
                TiltEditorCamera(EDITOR_POINT_LOOK_AT, -0.34906584f, 0x50);
                f_OpeningScene = OPENING_SCENE_LUIGI_DRIVE;
                break;
            case OPENING_SCENE_LUIGI_DRIVE:
                if (f_OpeningSceneFramesLeft == 0) {
                    f_IsNameSlotShown[0] = TRUE;
                    f_OpeningScene = OPENING_SCENE_LUIGI_DRIVE + 1;
                    f_OpeningSceneFramesLeft = 55;
                }

                break;
            case OPENING_SCENE_LUIGI_DRIVE + 1:
                if (f_OpeningSceneFramesLeft == 4) {
                    PlaySound(SOUND_CONTACT_PLAIN, f_OpeningSoundChannel, SOUND_FULL_SCALE);
                    AdvanceOpeningSoundChannel();
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    f_OpeningShotSimulation.playerIndex = 0;
                    f_OpeningShotSimulation.isHoledOut = FALSE;
                    f_OpeningShotSimulation.state = BALL_STATE_STOPPED;
                    f_OpeningShotSimulation.surface = SURFACE_TEE_UP;
                    LaunchOpeningBallSimulation(&f_OpeningShotSimulation, 0, &f_OpeningFourthShot);
                    f_OpeningShotState = OPENING_SHOT_FLYING;
                    f_ShotBallScale = 1;
                    ZoomEditorCamera(EDITOR_POINT_LOOK_AT, 33.056084f, 0x28);
                    f_OpeningSceneFramesLeft = 5;
                    f_OpeningScene++;
                    PlaySoundWithEffects(
                        SOUND_SHOT_SPARKS,
                        f_OpeningSoundChannel,
                        0x4C,
                        NO_REVERB,
                        0xA7
                    );
                    AdvanceOpeningSoundChannel();
                    f_OpeningSparksState = 1;
                    SpawnDrop(f_OpeningShotImpactPoint);
                }

                break;
            case OPENING_SCENE_LUIGI_DRIVE + 2:
                StepOpeningBallSimulation(&f_OpeningShotSimulation);
                if (AreEditorMovesDone()) {
                    PlaceEditorCamera(
                        934.786f,
                        -128.501f,
                        4716.105f,
                        f_OpeningShotSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE,
                        f_OpeningShotSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE,
                        f_OpeningShotSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE
                    );
                    f_LookAtEaseMode = LOOK_AT_EASE_SLOW;
                    f_OpeningSceneFramesLeft = 140;
                    f_IsNameSlotShown[0] = FALSE;
                    f_OpeningScene++;
                    ClearEffects();
                }

                break;
            case OPENING_SCENE_LUIGI_DRIVE + 3:
                if (f_OpeningSceneFramesLeft == 55) {
                    PlaySoundWithEffects(SOUND_HIT_FOLIAGE, f_OpeningSoundChannel, 0x50, 0xA, 0x9F);
                    SetSoundFrequencyOffset(f_OpeningSoundChannel, -9.0f);
                    AdvanceOpeningSoundChannel();
                }

                if (f_OpeningSceneFramesLeft == 45) {
                    PlaySoundWithEffects(
                        SOUND_HIT_FOLIAGE,
                        f_OpeningSoundChannel,
                        0x50,
                        0x14,
                        0x9F
                    );
                    SetSoundFrequencyOffset(f_OpeningSoundChannel, -10.0f);
                    AdvanceOpeningSoundChannel();
                }

                if (f_OpeningSceneFramesLeft == 35) {
                    PlaySoundWithEffects(
                        SOUND_HIT_FOLIAGE,
                        f_OpeningSoundChannel,
                        0x50,
                        0x1E,
                        0x9F
                    );
                    SetSoundFrequencyOffset(f_OpeningSoundChannel, -11.0f);
                    AdvanceOpeningSoundChannel();
                }

                if (f_OpeningSceneFramesLeft == 25) {
                    PlaySoundWithEffects(
                        SOUND_HIT_FOLIAGE,
                        f_OpeningSoundChannel,
                        0x50,
                        0x28,
                        0x9F
                    );
                    SetSoundFrequencyOffset(f_OpeningSoundChannel, -12.0f);
                    AdvanceOpeningSoundChannel();
                }

                if (f_OpeningSceneFramesLeft == 15) {
                    PlaySound(SOUND_LAND_GREEN, f_OpeningSoundChannel, 0x30);
                    AdvanceOpeningSoundChannel();
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    f_OpeningScene = OPENING_SCENE_PEACH_PLUM_WALK;
                    f_LookAtEaseMode = LOOK_AT_EASE_NONE;
                }

                break;
            case OPENING_SCENE_PEACH_PLUM_WALK:
                if (AreEditorMovesDone()) {
                    StartHudAnimation(HUD_ANIMATION_FADE_OUT);
                    f_OpeningScene++;
                    LoadOpeningBall(PLAIN_BALL, 0);
                    LoadOpeningBall(PLAIN_BALL, 1);
                    f_OpeningBallPositions[0][AXIS_X] = 2709.835f;
                    f_OpeningBallPositions[0][AXIS_Z] = 2465.858f;
                    f_OpeningBallScales[0] = 1.5f;
                    f_OpeningBallPositions[1][AXIS_X] = 2653.68f;
                    f_OpeningBallPositions[1][AXIS_Z] = 2478.326f;
                    f_OpeningBallScales[1] = 1.5f;
                }

                break;
            case OPENING_SCENE_PEACH_PLUM_WALK + 1:
                PlaceEditorCamera(2737.741f, -364.884f, 2368.88f, 2680.325f, -248.5f, 2488.521f);
                f_OpeningCue = OPENING_CUE_PEACH_PLUM_WALK;
                f_OpeningBallStates[0] = OPENING_BALL_ON_GROUND;
                f_OpeningBallStates[1] = OPENING_BALL_ON_GROUND;
                f_OpeningScene++;
                break;
            case OPENING_SCENE_PEACH_PLUM_WALK + 2:
                TiltEditorCamera(EDITOR_POINT_LOOK_AT, -0.22654298f, 0x5A);
                f_OpeningScene++;
                break;
            case OPENING_SCENE_PEACH_PLUM_WALK + 3:
                if (AreEditorMovesDone()) {
                    f_OpeningScene = OPENING_SCENE_MARIO_LUIGI_WALK;
                }

                if (f_OpeningFrameCount == 0x375) {
                    PlaySound(SOUND_YOSHI_REPEATED, f_OpeningSoundChannel, 0x3C);
                    AdvanceOpeningSoundChannel();
                }

                break;
            case OPENING_SCENE_MARIO_LUIGI_WALK:
                PlaceEditorCamera(2413.141f, -271.41f, 1784.364f, 2503.834f, -245.0f, 1850.029f);
                f_OpeningBallStates[1] = OPENING_BALL_HIDDEN;
                f_OpeningBallPositions[0][AXIS_X] = 2483.922f;
                f_OpeningBallPositions[0][AXIS_Z] = 1800.958f;
                f_OpeningBallScales[0] = OPENING_BALL_NORMAL_SCALE;
                MoveEditorCamera(EDITOR_POINT_LOOK_AT, 2489.032f, -255.774f, 1840.637f, 0x50);
                TiltEditorCamera(EDITOR_POINT_LOOK_AT, -0.116571f, 0x76);
                TurnEditorCamera(EDITOR_POINT_LOOK_AT, 1.366719f, 0x76);
                ZoomEditorCamera(EDITOR_POINT_LOOK_AT, 87.87598f, 0x76);
                f_OpeningCue = OPENING_CUE_MARIO_LUIGI_WALK;
                f_OpeningSceneFramesLeft = 118;
                f_OpeningScene++;
                break;
            case OPENING_SCENE_MARIO_LUIGI_WALK + 1:
                switch (f_OpeningSceneFramesLeft) {
                    case 73:
                    case 84:
                    case 94:
                    case 104:
                    case 115:
                        PlaySound(SOUND_BLOOP, f_OpeningSoundChannel, 0x46);
                        AdvanceOpeningSoundChannel();
                        break;
                    case 106:
                        PlaySoundWithEffects(
                            SOUND_GOOMBA,
                            f_OpeningSoundChannel,
                            0x48,
                            -6,
                            SOUND_FULL_SCALE
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 100:
                        PlaySound(SOUND_LONG_OH, f_OpeningSoundChannel, SOUND_FULL_SCALE);
                        AdvanceOpeningSoundChannel();
                        break;
                    case 70:
                        if (f_IsDonkeyKongShown) {
                            PlaySound(SOUND_HELLO_BALL, f_OpeningSoundChannel, SOUND_FULL_SCALE);
                        }
                        else {
                            PlaySound(
                                SOUND_MARIO_ITS_A_ME,
                                f_OpeningSoundChannel,
                                SOUND_FULL_SCALE
                            );
                        }

                        AdvanceOpeningSoundChannel();
                        break;
                }

                if (AreEditorMovesDone()) {
                    f_OpeningScene = OPENING_SCENE_MARIO_FINDS_BALL;
                    f_OpeningSceneFramesLeft = 40;
                }

                break;
            case OPENING_SCENE_MARIO_FINDS_BALL:
                if (f_OpeningSceneFramesLeft == 0) {
                    f_OpeningCue = OPENING_CUE_MARIO_REACH_BALL;
                    f_OpeningScene++;
                    MoveEditorCamera(EDITOR_POINT_LOOK_AT, 2489.636f, -257.5f, 1811.529f, 0x1E);
                    TiltEditorCamera(EDITOR_POINT_LOOK_AT, -0.070194006f, 0x32);
                    TurnEditorCamera(EDITOR_POINT_LOOK_AT, 0.3807652f, 0x32);
                    ZoomEditorCamera(EDITOR_POINT_LOOK_AT, 34.2068f, 0x32);
                    if (f_IsDonkeyKongShown) {
                        PlaySoundWithEffects(
                            SOUND_PEACH_LONG_HEY_MARIO,
                            f_OpeningSoundChannel,
                            SOUND_FULL_SCALE,
                            NO_REVERB,
                            0x3F
                        );
                    }
                    else {
                        PlaySoundWithEffects(
                            SOUND_PEACH_HEY_MARIO,
                            f_OpeningSoundChannel,
                            0x50,
                            NO_REVERB,
                            0x3F
                        );
                    }

                    AdvanceOpeningSoundChannel();
                }

                break;
            case OPENING_SCENE_MARIO_FINDS_BALL + 1:
                if (AreEditorMovesDone()) {
                    f_OpeningScene = OPENING_SCENE_PEACH_WAVE;
                    f_OpeningSceneFramesLeft = 10;
                }

                break;
            case OPENING_SCENE_PEACH_WAVE:
                if (f_OpeningSceneFramesLeft == 0) {
                    StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
                    PlaceEditorCamera(
                        2801.815f,
                        -255.822f,
                        2196.813f,
                        2823.524f,
                        -256.5f,
                        2177.502f
                    );
                    MoveEditorPoint(EDITOR_POINT_LOOK_AT, 2855.966f, -256.5f, 2213.121f, 0x32);
                    MoveEditorPoint(EDITOR_POINT_EYE, 2702.606f, -279.683f, 2250.851f, 0x32);
                    f_OpeningSceneFramesLeft = 90;
                    f_OpeningCue = OPENING_CUE_PEACH_WAVE;
                    g_HoleSetup.conditions.lightPitch = 0x1000;

                    *(s16 *)&g_HoleSetup.conditions.lightYaw = -0x7800;
                    f_OpeningScene++;
                }

                break;
            case OPENING_SCENE_PEACH_WAVE + 1:
                switch (f_OpeningSceneFramesLeft) {
                    case 87:
                        PlaySoundWithEffects(
                            SOUND_BLOOP,
                            f_OpeningSoundChannel,
                            0x50,
                            NO_REVERB,
                            0xA7
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 77:
                        PlaySoundWithEffects(
                            SOUND_BLOOP,
                            f_OpeningSoundChannel,
                            0x4C,
                            NO_REVERB,
                            0x9D
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 67:
                        PlaySoundWithEffects(
                            SOUND_BLOOP,
                            f_OpeningSoundChannel,
                            0x48,
                            NO_REVERB,
                            0x93
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 57:
                        PlaySoundWithEffects(
                            SOUND_BLOOP,
                            f_OpeningSoundChannel,
                            0x3A,
                            NO_REVERB,
                            0x89
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 42:
                        PlaySound(SOUND_YOSHI_EAGLE_SECOND_EFFECT, f_OpeningSoundChannel, 0x46);
                        SetSoundFrequencyOffset(f_OpeningSoundChannel, 7.0f);
                        AdvanceOpeningSoundChannel();
                        break;
                }

                if (f_OpeningSceneFramesLeft >= 23 && f_OpeningSceneFramesLeft < 42) {
                    GetCharacterPointPosition(2, 2, point);
                    g_CurrentShot.ballPosition[AXIS_X] = (s32)point[AXIS_X]
                        << WORLD_TO_COURSE_SHIFT;
                    g_CurrentShot.ballPosition[AXIS_Z] = (s32)point[AXIS_Z]
                        << WORLD_TO_COURSE_SHIFT;
                    g_CurrentShot.ballPosition[AXIS_Y] = GetGroundHeight(
                                                             g_CurrentShot.ballPosition[AXIS_X],
                                                             g_CurrentShot.ballPosition[AXIS_Z]
                                                         ) +
                        15360.0f;
                    SpawnRisingClumps(f_OpeningShotImpactPoint);
                }

                if (f_OpeningSceneFramesLeft == 80) {
                    if (IsDonkeyKongUnlocked()) {
                        if (!(rand() & 0x80)) {
                            f_IsDonkeyKongShown = TRUE;
                        }
                        else {
                            f_IsDonkeyKongShown = FALSE;
                        }
                    }

                    LoadOpeningBall(PLAIN_BALL, 0);
                    f_OpeningBallPositions[0][AXIS_X] = 2723.423f;
                    f_OpeningBallPositions[0][AXIS_Z] = 2246.086f;
                }

                if (f_OpeningSceneFramesLeft == 50) {
                    f_OpeningBallStates[0] = OPENING_BALL_ON_GROUND;
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    TiltEditorCamera(EDITOR_POINT_LOOK_AT, -0.166897f, 0x32);
                    f_OpeningScene++;
                    PlaySound(SOUND_LOOK_AT_CUP, f_OpeningSoundChannel, 0x32);
                    AdvanceOpeningSoundChannel();
                }

                break;
            case OPENING_SCENE_PEACH_WAVE + 2:
                if (AreEditorMovesDone()) {
                    f_OpeningSceneFramesLeft = 20;
                    f_OpeningScene = OPENING_SCENE_BALL_ROLLS_IN;
                }

                break;
            case OPENING_SCENE_BALL_ROLLS_IN:
                if (f_OpeningSceneFramesLeft == 0) {
                    PlaceEditorCamera(
                        2712.031f,
                        -270.705f,
                        2358.303f,
                        2770.045f,
                        -250.5f,
                        2337.548f
                    );
                    f_OpeningCue = OPENING_CUE_PEACH_APPROACH;
                    f_OpeningSceneFramesLeft = 64;
                    f_OpeningScene = OPENING_SCENE_PEACH_APPROACH;
                    f_OpeningBallPositions[0][AXIS_X] = 2755.104f;
                    f_OpeningBallPositions[0][AXIS_Z] = 2341.293f;
                }

                break;
            case OPENING_SCENE_PEACH_APPROACH:
                switch (f_OpeningSceneFramesLeft) {
                    case 53:
                    case 42:
                    case 26:
                        PlaySound(SOUND_BLOOP, f_OpeningSoundChannel, 0x30);
                        AdvanceOpeningSoundChannel();
                        break;
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    PlaceEditorCamera(
                        2670.074f,
                        -270.531f,
                        2375.225f,
                        2692.222f,
                        -248.0f,
                        2366.292f
                    );
                    ZoomEditorCamera(EDITOR_POINT_LOOK_AT, 16.721228f, 0x28);
                    f_OpeningScene++;
                    PlaySound(SOUND_LOOK_AT_CUP, f_OpeningSoundChannel, 0x3C);
                    AdvanceOpeningSoundChannel();
                    g_HoleSetup.conditions.lightPitch = 0x1000;
                    g_HoleSetup.conditions.lightYaw = 0x6800;
                    if (f_IsDonkeyKongShown) {
                        LoadOpeningBall(DONKEY_KONG_FACE_BALL, 1);
                    }
                    else {
                        LoadOpeningBall(YOSHI_FACE_BALL, 1);
                    }

                    f_OpeningBallStates[0] = OPENING_BALL_HIDDEN;
                    f_OpeningBallStates[1] = OPENING_BALL_ON_GROUND;
                    f_OpeningBallPositions[1][AXIS_X] = 2689.998f;
                    f_OpeningBallPositions[1][AXIS_Z] = 2367.191f;
                    f_OpeningBallScales[1] = OPENING_BALL_LARGE_SCALE;
                }

                break;
            case OPENING_SCENE_PEACH_APPROACH + 1:
                if (AreEditorMovesDone()) {
                    f_OpeningScene = OPENING_SCENE_BALL_INSPECTION;
                    f_OpeningSceneFramesLeft = 20;
                }

                break;
            case OPENING_SCENE_BALL_INSPECTION:
                if (f_OpeningSceneFramesLeft == 0) {
                    PlaceEditorCamera(
                        2712.031f,
                        -270.705f,
                        2358.303f,
                        2770.045f,
                        -250.5f,
                        2337.548f
                    );
                    f_OpeningCue = OPENING_CUE_BALL_INSPECTION;
                    f_OpeningSceneFramesLeft = 56;
                    g_CameraEditor.fieldOfView = 45.0f;
                    f_OpeningScene++;
                    LoadOpeningBall(PLAIN_BALL, 0);
                    f_OpeningBallStates[0] = OPENING_BALL_ON_GROUND;
                    f_OpeningBallStates[1] = OPENING_BALL_HIDDEN;
                    f_OpeningBallPositions[0][AXIS_X] = 2755.104f;
                    f_OpeningBallPositions[0][AXIS_Z] = 2341.293f;
                    f_OpeningBallScales[0] = OPENING_BALL_NORMAL_SCALE;
                }

                break;
            case OPENING_SCENE_BALL_INSPECTION + 1:
                if (f_OpeningSceneFramesLeft == 10) {
                    PlaySoundWithEffects(
                        SOUND_BABY_MARIO_CALL_OUT_C_DOWN,
                        f_OpeningSoundChannel,
                        0x40,
                        0x14,
                        0xCF
                    );
                    AdvanceOpeningSoundChannel();
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    f_OpeningCue = OPENING_CUE_MARIO_LOOK_UP;
                    f_OpeningSceneFramesLeft = 30;
                    f_OpeningScene++;
                    LoadOpeningBall(PLAIN_BALL, 1);
                    f_OpeningBallPositions[1][AXIS_X] = 0.0f;
                    f_OpeningBallPositions[1][AXIS_Z] = 0.0f;
                }

                break;
            case OPENING_SCENE_BALL_INSPECTION + 2:
                if (f_OpeningFrameCount == 0x5C5) {
                    PlaySoundWithEffects(
                        SOUND_UH_OH,
                        f_OpeningSoundChannel,
                        SOUND_FULL_SCALE,
                        NO_REVERB,
                        0x89
                    );
                    AdvanceOpeningSoundChannel();
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    f_OpeningCue = OPENING_CUE_FREEZE;
                    f_OpeningScene = OPENING_SCENE_INCOMING_BALL;
                }

                break;
            case OPENING_SCENE_INCOMING_BALL:
                GetCharacterPointPosition(1, 3, logoBallPoint);
                f_OpeningBallStates[1] = OPENING_BALL_IN_AIR;
                f_OpeningBallPositions[1][AXIS_X] = 2319.014f;
                f_OpeningBallPositions[1][AXIS_Y] = -560.0f;
                f_OpeningBallPositions[1][AXIS_Z] = 342.12f;
                f_OpeningBallScales[1] = 2.0f;
                PlaceEditorCamera(2662.345f, -254.25f, 2376.081f, 2493.599f, -910.952f, 665.178f);
                MoveEditorPoint(EDITOR_POINT_EYE, 2671.388f, -257.489f, 2351.288f, 0x28);
                MoveEditorPoint(
                    EDITOR_POINT_LOOK_AT,
                    logoBallPoint[AXIS_X],
                    logoBallPoint[AXIS_Y] - 4.5f,
                    logoBallPoint[AXIS_Z],
                    0x28
                );
                f_IsLogoBallSlot[1] = TRUE;
                f_OpeningScene++;
                f_LogoBallFollowRate = 1.0f;
                break;
            case OPENING_SCENE_INCOMING_BALL + 1:
                f_OpeningBallPositions[1][AXIS_X] +=
                    (g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X] -
                        f_OpeningBallPositions[1][AXIS_X]) *
                    f_LogoBallFollowRate;
                f_OpeningBallPositions[1][AXIS_Y] +=
                    (g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y] -
                        f_OpeningBallPositions[1][AXIS_Y]) *
                    f_LogoBallFollowRate;
                f_OpeningBallPositions[1][AXIS_Z] +=
                    (g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z] -
                        f_OpeningBallPositions[1][AXIS_Z]) *
                    f_LogoBallFollowRate;
                f_LogoBallFollowRate -= 0.02f;
                if (f_LogoBallFollowRate < 0.1f) {
                    f_LogoBallFollowRate = 0.1f;
                }

                if (AreEditorMovesDone()) {
                    f_OpeningCue = OPENING_CUE_UNFREEZE;
                    f_OpeningSceneFramesLeft = 120;
                    f_OpeningScene = OPENING_SCENE_BABY_MARIO_ARRIVAL;
                    f_LookAtEaseMode = LOOK_AT_EASE_SPEEDING_UP;
                }

                break;
            case OPENING_SCENE_BABY_MARIO_ARRIVAL:
                f_LogoBallFollowRate += 0.02f;
                if (f_LogoBallFollowRate > 1.0f) {
                    f_LogoBallFollowRate = 1.0f;
                }

                GetCharacterPointPosition(1, 3, handPoint);
                f_OpeningBallPositions[1][AXIS_X] +=
                    (handPoint[AXIS_X] - f_OpeningBallPositions[1][AXIS_X]) * f_LogoBallFollowRate;
                f_OpeningBallPositions[1][AXIS_Y] +=
                    (handPoint[AXIS_Y] - f_OpeningBallPositions[1][AXIS_Y]) * f_LogoBallFollowRate;
                f_OpeningBallPositions[1][AXIS_Z] +=
                    (handPoint[AXIS_Z] - f_OpeningBallPositions[1][AXIS_Z]) * f_LogoBallFollowRate;
                switch (f_OpeningSceneFramesLeft) {
                    case 97:
                        PlaySoundWithEffects(
                            SOUND_LAND_FAIRWAY,
                            f_OpeningSoundChannel,
                            SOUND_FULL_SCALE,
                            NO_REVERB,
                            0xA7
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 69:
                        PlaySoundWithEffects(
                            SOUND_LAND_FAIRWAY,
                            f_OpeningSoundChannel,
                            SOUND_FULL_SCALE,
                            NO_REVERB,
                            0x93
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 55:
                        PlaySoundWithEffects(
                            SOUND_ROLL,
                            f_OpeningSoundChannel,
                            0x40,
                            0x7F,
                            SOUND_FULL_SCALE
                        );
                        break;
                    case 15:
                        StopSound(f_OpeningSoundChannel);
                        AdvanceOpeningSoundChannel();
                        break;
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
                    PlaceEditorCamera(2878.956f, -391.282f, 2514.85f, 2431.88f, -315.0f, 992.943f);
                    MoveEditorPoint(EDITOR_POINT_EYE, 2051.33f, -258.127f, 862.423f, 0x28);
                    MoveEditorPoint(EDITOR_POINT_LOOK_AT, 2058.79f, -254.0f, 825.963f, 0x28);
                    f_OpeningCue = OPENING_CUE_BABY_MARIO_ARRIVAL;
                    f_OpeningSceneFramesLeft = 20;
                    f_OpeningScene++;
                    LoadNameLogo(5, 0);
                    if (f_IsDonkeyKongShown) {
                        LoadNameLogo(6, 1);
                    }
                    else {
                        LoadNameLogo(4, 1);
                    }

                    f_OpeningBallStates[1] = OPENING_BALL_HIDDEN;
                    f_IsLogoBallSlot[1] = FALSE;
                    f_ShotBallScale = 0;
                    f_LookAtEaseMode = LOOK_AT_EASE_NONE;
                    PlaySound(SOUND_LOOK_AT_CUP, f_OpeningSoundChannel, 0x50);
                    AdvanceOpeningSoundChannel();
                }

                break;
            case OPENING_SCENE_BABY_MARIO_ARRIVAL + 1:
                if (f_OpeningSceneFramesLeft == 0) {
                    f_OpeningCue = OPENING_CUE_BABY_MARIO_RIDE;
                    f_OpeningSceneFramesLeft = 36;
                    f_OpeningScene++;
                }

                break;
            case OPENING_SCENE_BABY_MARIO_ARRIVAL + 2:
                if (f_IsDonkeyKongShown) {
                    if (f_OpeningSceneFramesLeft == 18) {
                        PlaySoundWithEffects(
                            SOUND_DONKEY_KONG_REPEATED,
                            f_OpeningSoundChannel,
                            SOUND_FULL_SCALE,
                            7,
                            SOUND_FULL_SCALE
                        );
                        SetSoundFrequencyOffset(f_OpeningSoundChannel, -4.0f);
                        AdvanceOpeningSoundChannel();
                    }
                }
                else {
                    switch (f_OpeningSceneFramesLeft) {
                        case 19:
                        case 9:
                            PlaySound(SOUND_YOSHI_REPEATED, f_OpeningSoundChannel, 0x43);
                            AdvanceOpeningSoundChannel();
                            break;
                    }
                }

                if (AreEditorMovesDone()) {
                    f_IsNameSlotShown[0] = TRUE;
                    f_IsNameSlotShown[1] = TRUE;
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    if (f_IsDonkeyKongShown) {
                        MoveEditorPoint(EDITOR_POINT_LOOK_AT, 2043.766f, -256.5f, 851.89f, 0x5A);
                        MoveEditorPoint(EDITOR_POINT_EYE, 2032.192f, -262.337f, 875.169f, 0x5A);
                    }
                    else {
                        MoveEditorPoint(EDITOR_POINT_LOOK_AT, 2043.767f, -250.0f, 851.891f, 0x5A);
                        MoveEditorPoint(EDITOR_POINT_EYE, 2025.674f, -258.046f, 888.28f, 0x5A);
                    }

                    f_OpeningSceneFramesLeft = 90;
                    f_OpeningScene++;
                }

                break;
            case OPENING_SCENE_BABY_MARIO_ARRIVAL + 3:
                if (f_IsDonkeyKongShown) {
                    switch (f_OpeningSceneFramesLeft) {
                        case 26:
                        case 46:
                        case 66:
                        case 86:
                            PlaySoundWithEffects(
                                SOUND_DONKEY_KONG_REPEATED,
                                f_OpeningSoundChannel,
                                SOUND_FULL_SCALE,
                                7,
                                SOUND_FULL_SCALE
                            );
                            SetSoundFrequencyOffset(f_OpeningSoundChannel, -4.0f);
                            AdvanceOpeningSoundChannel();
                            break;
                        case 67:
                            PlaySound(SOUND_BOING, f_OpeningSoundChannel, 0x28);
                            AdvanceOpeningSoundChannel();
                            break;
                        case 47:
                            PlaySound(SOUND_GOOMBA, f_OpeningSoundChannel, 0x60);
                            AdvanceOpeningSoundChannel();
                            break;
                        case 31:
                            PlaySound(SOUND_GOOMBA, f_OpeningSoundChannel, 0x30);
                            AdvanceOpeningSoundChannel();
                            break;
                        case 12:
                            PlaySoundWithEffects(
                                SOUND_BABY_MARIO_BIRDIE,
                                f_OpeningSoundChannel,
                                SOUND_FULL_SCALE,
                                NO_REVERB,
                                0x93
                            );
                            AdvanceOpeningSoundChannel();
                            break;
                        case 8:
                            PlaySoundWithEffects(
                                SOUND_DONKEY_KONG_BIRDIE,
                                f_OpeningSoundChannel,
                                SOUND_FULL_SCALE,
                                NO_REVERB,
                                0x6B
                            );
                            AdvanceOpeningSoundChannel();
                            break;
                    }
                }
                else {
                    switch (f_OpeningSceneFramesLeft) {
                        case 69:
                        case 79:
                        case 89:
                            PlaySound(SOUND_YOSHI_REPEATED, f_OpeningSoundChannel, 0x57);
                            AdvanceOpeningSoundChannel();
                            break;
                        case 42:
                            PlaySound(SOUND_YOSHI_REPEATED, f_OpeningSoundChannel, 0x61);
                            AdvanceOpeningSoundChannel();
                            break;
                        case 59:
                            PlaySound(SOUND_BOING, f_OpeningSoundChannel, 0x51);
                            AdvanceOpeningSoundChannel();
                            break;
                        case 34:
                            PlaySound(SOUND_BOING, f_OpeningSoundChannel, 0x5B);
                            AdvanceOpeningSoundChannel();
                            break;
                        case 29:
                            PlaySound(SOUND_GOOMBA, f_OpeningSoundChannel, 0x6B);
                            AdvanceOpeningSoundChannel();
                            break;
                        case 39:
                            PlaySoundWithEffects(
                                SOUND_GOOMBA,
                                f_OpeningSoundChannel,
                                0x30,
                                NO_REVERB,
                                0x65
                            );
                            AdvanceOpeningSoundChannel();
                            break;
                        case 22:
                            PlaySoundWithEffects(
                                SOUND_GOOMBA,
                                f_OpeningSoundChannel,
                                0x18,
                                NO_REVERB,
                                0x5B
                            );
                            AdvanceOpeningSoundChannel();
                            break;
                        case 4:
                            PlaySoundWithEffects(
                                SOUND_YOSHI_CALL_OUT_LEFT,
                                f_OpeningSoundChannel,
                                SOUND_FULL_SCALE,
                                NO_REVERB,
                                0x6B
                            );
                            AdvanceOpeningSoundChannel();
                            break;
                    }
                }

                if (AreEditorMovesDone()) {
                    f_OpeningSceneFramesLeft = 20;
                    f_OpeningScene = OPENING_SCENE_MARIO_PUZZLED;
                }

                break;
            case OPENING_SCENE_MARIO_PUZZLED:
                if (!f_IsDonkeyKongShown && f_OpeningSceneFramesLeft == 16) {
                    PlaySoundWithEffects(
                        SOUND_BABY_MARIO_CALL_OUT_DOWN,
                        f_OpeningSoundChannel,
                        0x60,
                        NO_REVERB,
                        0x93
                    );
                    AdvanceOpeningSoundChannel();
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    PlaceEditorCamera(
                        2708.301f,
                        -264.204f,
                        2343.526f,
                        2770.098f,
                        -260.0f,
                        2337.539f
                    );
                    f_OpeningCue = OPENING_CUE_MARIO_PUZZLED;
                    f_OpeningSceneFramesLeft = 70;
                    f_IsNameSlotShown[0] = FALSE;
                    f_IsNameSlotShown[1] = FALSE;
                    f_OpeningScene++;
                    LoadWipeShape(0xC);
                }

                break;
            case OPENING_SCENE_MARIO_PUZZLED + 1:
                switch (f_OpeningSceneFramesLeft) {
                    case 60:
                        PlaySound(SOUND_AMBIENT_ALTERNATING_FIRST, f_OpeningSoundChannel, 0x60);
                        AdvanceOpeningSoundChannel();

                    case 50:
                        PlaySound(SOUND_AMBIENT_REPEATING, f_OpeningSoundChannel, 0x60);
                        AdvanceOpeningSoundChannel();
                        break;
                    case 40:
                        PlaySound(SOUND_AMBIENT_REPEATING, f_OpeningSoundChannel, 0x60);
                        AdvanceOpeningSoundChannel();
                        break;
                    case 37:
                        PlaySound(SOUND_AMBIENT_ALTERNATING_FIRST, f_OpeningSoundChannel, 0x60);
                        AdvanceOpeningSoundChannel();
                        break;
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    StartWipe(1);
                    f_OpeningScene++;
                }

                break;
            case OPENING_SCENE_MARIO_PUZZLED + 2:
                if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
                    f_OpeningSceneFramesLeft = 20;
                    f_OpeningScene = OPENING_SCENE_LUIGI_SEARCH;
                    f_OpeningCue = OPENING_CUE_CLEAR_CHARACTERS;
                    f_OpeningBallStates[0] = OPENING_BALL_HIDDEN;
                }

                break;
            case OPENING_SCENE_LUIGI_SEARCH:
                if (f_OpeningSceneFramesLeft == 0) {
                    PlaceEditorCamera(
                        1423.923f,
                        -248.221f,
                        1595.031f,
                        1450.829f,
                        -254.0f,
                        1546.087f
                    );
                    MoveEditorPoint(EDITOR_POINT_LOOK_AT, 1420.136f, -250.5f, 1551.713f, 0x32);
                    f_OpeningCue = OPENING_CUE_LUIGI_SEARCH;
                    f_OpeningScene++;
                    StartWipe(2);
                    f_OpeningSceneFramesLeft = 160;
                    LoadOpeningImpactLines();
                }

                break;
            case OPENING_SCENE_LUIGI_SEARCH + 1:
                if (f_OpeningSceneFramesLeft == 30) {
                    f_AreImpactLinesShown = TRUE;
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    f_OpeningScene = OPENING_SCENE_OUT_OF_BOUNDS;
                    LoadOpeningBall(LUIGI_FACE_BALL, 0);
                    f_OpeningBallStates[0] = OPENING_BALL_ON_GROUND;
                    f_OpeningBallPositions[0][AXIS_X] = 932.978f;
                    f_OpeningBallPositions[0][AXIS_Z] = 1884.644f;
                    f_OpeningBallScales[0] = 1.4f;
                }

                switch (f_OpeningSceneFramesLeft) {
                    case 147:
                        PlaySoundWithEffects(
                            SOUND_BLOOP,
                            f_OpeningSoundChannel,
                            0x50,
                            NO_REVERB,
                            0x61
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 137:
                        PlaySoundWithEffects(
                            SOUND_BLOOP,
                            f_OpeningSoundChannel,
                            0x50,
                            NO_REVERB,
                            0x6B
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 127:
                        PlaySoundWithEffects(
                            SOUND_BLOOP,
                            f_OpeningSoundChannel,
                            0x50,
                            NO_REVERB,
                            0x75
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 116:
                        PlaySoundWithEffects(
                            SOUND_BLOOP,
                            f_OpeningSoundChannel,
                            0x50,
                            NO_REVERB,
                            SOUND_FULL_SCALE
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 30:
                        PlaySound(SOUND_CHIME, f_OpeningSoundChannel, 0x46);
                        AdvanceOpeningSoundChannel();
                        break;
                    case 28:
                        if (f_IsDonkeyKongShown) {
                            PlaySound(SOUND_LUIGI_OH_NO, f_OpeningSoundChannel, SOUND_FULL_SCALE);
                            f_DonkeyKongSoundChannel = f_OpeningSoundChannel;
                        }
                        else {
                            PlaySound(SOUND_SHORT_OH, f_OpeningSoundChannel, SOUND_FULL_SCALE);
                        }

                        AdvanceOpeningSoundChannel();
                        break;
                    case 20:
                        if (f_IsDonkeyKongShown) {
                            StopSound(f_DonkeyKongSoundChannel);
                        }

                        break;
                }

                break;
            case OPENING_SCENE_OUT_OF_BOUNDS:
                PlaceEditorCamera(956.687f, -259.45f, 1874.671f, 933.391f, -243.5f, 1884.391f);
                f_OpeningSceneFramesLeft = 20;
                f_OpeningCue = OPENING_CUE_TOAD_FLAG;
                f_AreImpactLinesShown = FALSE;
                g_CameraEditor.fieldOfView = 18.0f;
                f_OpeningScene++;
                break;
            case OPENING_SCENE_OUT_OF_BOUNDS + 1:
                if (f_OpeningSceneFramesLeft == 6) {
                    PlaySoundWithEffects(
                        SOUND_FLAG_SWISH,
                        f_OpeningSoundChannel,
                        0x60,
                        NO_REVERB,
                        0xA7
                    );
                    AdvanceOpeningSoundChannel();
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    MoveEditorPoint(EDITOR_POINT_LOOK_AT, 933.391f, -250.5f, 1884.391f, 0x3C);
                    f_OpeningSceneFramesLeft = 60;
                    f_OpeningScene++;
                }

                break;
            case OPENING_SCENE_OUT_OF_BOUNDS + 2:
                g_CameraEditor.fieldOfView += 0.2666666f;
                switch (f_OpeningSceneFramesLeft) {
                    case 54:
                        PlaySoundWithEffects(
                            SOUND_FLAG_SWISH,
                            f_OpeningSoundChannel,
                            0x60,
                            NO_REVERB,
                            0x57
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 10:
                        PlaySound(SOUND_OUT_OF_BOUNDS, f_OpeningSoundChannel, SOUND_FULL_SCALE);
                        AdvanceOpeningSoundChannel();
                        break;
                    case 36:
                        PlaySoundWithEffects(
                            SOUND_FLAG_SWISH,
                            f_OpeningSoundChannel,
                            0x60,
                            NO_REVERB,
                            0xA7
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                    case 6:
                        PlaySoundWithEffects(
                            SOUND_FLAG_SWISH,
                            f_OpeningSoundChannel,
                            0x60,
                            NO_REVERB,
                            0x57
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                }

                if (AreEditorMovesDone()) {
                    f_OpeningScene = OPENING_SCENE_TOAD_FLAG;
                    f_OpeningSceneFramesLeft = 60;
                }

                break;
            case OPENING_SCENE_TOAD_FLAG:
                switch (f_OpeningSceneFramesLeft) {
                    case 36:
                        PlaySoundWithEffects(
                            SOUND_FLAG_SWISH,
                            f_OpeningSoundChannel,
                            0x60,
                            NO_REVERB,
                            0xA7
                        );
                        AdvanceOpeningSoundChannel();

                    case 6:
                        PlaySoundWithEffects(
                            SOUND_FLAG_SWISH,
                            f_OpeningSoundChannel,
                            0x60,
                            NO_REVERB,
                            0x57
                        );
                        AdvanceOpeningSoundChannel();
                        break;
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    PlaceEditorCamera(
                        1423.703f,
                        -250.799f,
                        1574.041f,
                        1420.135f,
                        -252.0f,
                        1551.713f
                    );
                    f_OpeningScene = OPENING_SCENE_GATHERING;
                    f_OpeningSceneFramesLeft = 80;
                    f_OpeningCue = OPENING_CUE_LUIGI_DISMAY;
                    f_OpeningBallStates[0] = OPENING_BALL_HIDDEN;
                    g_CameraEditor.fieldOfView = 45.0f;
                    LoadOpeningBurst();
                }

                break;
            case OPENING_SCENE_GATHERING:
                if (f_OpeningSceneFramesLeft == 46) {
                    f_IsBurstShown = TRUE;
                    PlaySound(SOUND_BURST, f_OpeningSoundChannel, 0x50);
                    AdvanceOpeningSoundChannel();
                }

                if (f_OpeningSceneFramesLeft == 26) {
                    PlaySound(SOUND_LUIGI_OH_NO, f_OpeningSoundChannel, SOUND_FULL_SCALE);
                    AdvanceOpeningSoundChannel();
                }

                if (f_OpeningSceneFramesLeft == 20) {
                    f_AreAmbientSoundsOn = TRUE;
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    MoveEditorPoint(EDITOR_POINT_EYE, 1404.737f, -250.799f, 1633.633f, 0x50);
                    f_OpeningScene++;
                    LoadLakitu();
                    f_OpeningSceneFramesLeft = 40;
                }

                break;
            case OPENING_SCENE_GATHERING + 1:
                g_CameraEditor.fieldOfView += 0.2f;
                if (g_CameraEditor.fieldOfView > 100.0f) {
                    g_CameraEditor.fieldOfView = 100.0f;
                }

                if (AreEditorMovesDone()) {
                    f_OpeningSceneFramesLeft = 20;
                    f_OpeningScene++;
                }

                break;
            case OPENING_SCENE_GATHERING + 2:
                g_CameraEditor.fieldOfView += 0.2f;
                if (g_CameraEditor.fieldOfView > 100.0f) {
                    g_CameraEditor.fieldOfView = 100.0f;
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    MoveEditorPoint(EDITOR_POINT_EYE, 1409.318f, -400.0f, 1609.259f, 0xDC);
                    f_OpeningSceneFramesLeft = 40;
                    f_OpeningScene++;
                }

                break;
            case OPENING_SCENE_GATHERING + 3:
                g_CameraEditor.fieldOfView += 0.2f;
                if (g_CameraEditor.fieldOfView > 100.0f) {
                    g_CameraEditor.fieldOfView = 100.0f;
                }

                if (f_OpeningSceneFramesLeft == 0) {
                    f_IsLakituFlying = TRUE;
                    f_OpeningSceneFramesLeft = 1000;
                    f_OpeningScene++;
                    PlaySound(SOUND_WIND_FLY_GUY, f_OpeningSoundChannel, 0x3F);
                    AdvanceOpeningSoundChannel();
                    f_AreAmbientSoundsOn = FALSE;
                }

                break;
            case OPENING_SCENE_GATHERING + 4:
                g_CameraEditor.fieldOfView += 0.2f;
                if (g_CameraEditor.fieldOfView > 100.0f) {
                    g_CameraEditor.fieldOfView = 100.0f;
                }

                switch (f_OpeningSceneFramesLeft) {
                    case 969:
                    case 970:
                    case 971:
                    case 972:
                    case 993:
                    case 994:
                    case 995:
                    case 997:
                    case 998:
                        PlaySound(SOUND_FALLING_WHISTLE, f_OpeningSoundChannel, 0x40);
                        AdvanceOpeningSoundChannel();
                        break;
                }

                switch (f_OpeningSceneFramesLeft) {
                    case 936:
                    case 937:
                    case 939:
                    case 940:
                    case 961:
                    case 963:
                    case 965:
                    case 966:
                    case 968:
                        PlaySound(SOUND_BOING, f_OpeningSoundChannel, 0x60);
                        AdvanceOpeningSoundChannel();
                        break;
                }

                PlayLogoClubSound();
                if (f_OpeningSceneFramesLeft == 975) {
                    PlaySoundWithEffects(
                        SOUND_OPENING_CLOSE,
                        CLOSING_SOUND_CHANNEL,
                        SOUND_FULL_SCALE,
                        NO_REVERB,
                        SOUND_FULL_SCALE
                    );
                    f_ClosingSoundChannel = CLOSING_SOUND_CHANNEL;
                }

                break;
            case OPENING_SCENE_SKIPPED:
                StartScreenFadeOut(
                    SKIP_FADE_WHITE_LEVEL,
                    SKIP_FADE_WHITE_LEVEL,
                    SKIP_FADE_WHITE_LEVEL,
                    SKIP_FADE_FRAME_COUNT
                );
                f_OpeningScene = OPENING_SCENE_END;
                break;
            case OPENING_SCENE_END:
                if (IsScreenFadeDone()) {
                    StopEditorMoves();
                    g_TitleScreenEntrance = TITLE_ENTRANCE_FROM_OPENING;
                    g_Scene = SCENE_SWAPPING_MODULES;
                    g_NextScene = SCENE_TITLE_SCREEN;
                    g_SceneModuleToUnload = MODULE_OPENING;
                    g_SceneModuleToLoad = MODULE_TITLE_SCREEN;
                    ClearDebugText();
                    StopWipe();
                    FreeAllCharacterSlots();
                    for (channel = 0; channel < SCRIPT_SOUND_CHANNEL_COUNT; channel++) {
                        if (g_TitleScreenEntrance != TITLE_ENTRANCE_FROM_OPENING ||
                            (channel != f_DonkeyKongSoundChannel &&
                                channel != f_ClosingSoundChannel)) {
                            StopSound(channel);
                        }
                    }

                    if (!f_HasLogoStarted) {
                        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
                    }

                    SetTextSlot(
                        CLEARED_TEXT_X,
                        CLEARED_TEXT_Y,
                        NO_MESSAGE,
                        TEXT_SLOT_LARGE_FONT,
                        0,
                        0,
                        0,
                        0,
                        0,
                        0
                    );
                    return;
                }

                break;
        }
    }

    if (f_OpeningSceneFramesLeft > 0) {
        f_OpeningSceneFramesLeft--;
    }

    UpdateCameraEditor();
    EaseOpeningLookAt();
    g_Cameras[MAIN_CAMERA].mode = CAMERA_MODE_BEHIND_GOLFER;
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] = g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_X];
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] = g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Y];
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] = g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Z];
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] =
        g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X];
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] =
        g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y];
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] =
        g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z];
    g_Cameras[MAIN_CAMERA].view.fieldOfView = g_CameraEditor.fieldOfView;
    if (f_OpeningShotState == OPENING_SHOT_FLYING) {
        f_UnusedStickOffsetX +=
            g_ControllerStates[OPENING_SCENE_CONTROLLER].stickX * UNUSED_STICK_OFFSET_SCALE;
        f_UnusedStickOffsetY +=
            g_ControllerStates[OPENING_SCENE_CONTROLLER].stickY * UNUSED_STICK_OFFSET_SCALE;
        for (slotIndex = 0; slotIndex < CHARACTER_SLOT_COUNT; slotIndex++) {
            if (IsCharacterSlotBusy(slotIndex)) {
                GetCharacterPointPosition(slotIndex, 1, point);
            }
        }

        point[AXIS_X] = f_OpeningShotSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE;
        point[AXIS_Y] = f_OpeningShotSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        point[AXIS_Z] = f_OpeningShotSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        StepOpeningBallSimulation(&f_OpeningShotSimulation);
    }
    else if (f_OpeningShotState == OPENING_SHOT_PLAIN_FLIGHT) {
        StepOpeningBallSimulation(&f_OpeningShotSimulation);
    }

    if (f_OpeningScene != OPENING_SCENE_END &&
        !f_HasLogoStarted &&
        !IsGameFlagSet(GAME_FLAG_EDIT_OPENING_CAMERA)) {
        if (f_OpeningScene != OPENING_SCENE_SKIPPED) {
            pressedButtons = 0;
            for (controllerIndex = 0; controllerIndex != g_ControllerCount; controllerIndex++) {
                pressedButtons |= g_ControllerStates[controllerIndex].pressedButtons;
            }

            if (g_IsDebugEnabled) {
                skipButtons = g_ControllerStates[OPENING_DEBUG_SKIP_CONTROLLER].pressedButtons &
                    DEBUG_SKIP_BUTTONS;
            }
            else {
                skipButtons = pressedButtons & SKIP_BUTTONS;
            }

            if (skipButtons != 0) {
                f_OpeningScene = OPENING_SCENE_SKIPPED;
            }
        }

        if (g_IsDebugEnabled) {
            if (g_ControllerStates[OPENING_EDITOR_CONTROLLER].pressedButtons & A_BUTTON) {
                PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
                SetGameFlag(GAME_FLAG_EDIT_OPENING_CAMERA);
                f_IsLookAtMarkerShown = TRUE;
                f_LookAtEaseMode = LOOK_AT_EASE_NONE;
            }

            if (g_IsDebugEnabled &&
                (g_ControllerStates[OPENING_SCENE_CONTROLLER].pressedButtons & R_TRIG)) {
                f_OpeningSceneFramesLeft = 0;
                StopEditorMoves();
            }
        }
    }

    if (UpdateOpeningCharacters()) {
        f_OpeningScene = OPENING_SCENE_END;
    }

    AnimateAllCharacters();
    AnimateNameLogos();
    MoveLakitu();
    PlayOpeningAmbientSounds();
}

void DrawOpening(CameraMatrices *matrices, Gfx **displayListHead) {
    Mat4 unusedMatrix UNUSED;
    Vec3 unusedPoints[4] UNUSED;
    Gfx *gfx = *displayListHead;

    f_OpeningFrameParity ^= 1;
    if (f_IsLookAtMarkerShown) {
        guTranslate(
            &f_LookAtMarkerTranslation,
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X],
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y],
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z]
        );
        guScale(
            &f_LookAtMarkerScaling,
            g_LookAtMarkerScale,
            g_LookAtMarkerScale,
            g_LookAtMarkerScale
        );
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
        gSPLoadGeometryMode(gfx++, 0);
        gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
        gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_LookAtMarkerTranslation),
            G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_LookAtMarkerScaling),
            G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
        );
        gSPVertex(gfx++, f_LookAtMarkerVertices[0], QUAD_VERTEX_COUNT, 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        gSPVertex(gfx++, f_LookAtMarkerVertices[1], QUAD_VERTEX_COUNT, 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        gSPVertex(gfx++, f_LookAtMarkerVertices[2], QUAD_VERTEX_COUNT, 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        gSPVertex(gfx++, f_LookAtMarkerVertices[3], QUAD_VERTEX_COUNT, 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        gSPVertex(gfx++, f_LookAtMarkerVertices[4], QUAD_VERTEX_COUNT, 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        gSPVertex(gfx++, f_LookAtMarkerVertices[5], QUAD_VERTEX_COUNT, 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
        gDPPipeSync(gfx++);
    }

    DrawOpeningCharacters(matrices, &gfx, f_OpeningFrameParity);
    g_DisplayListHead = gfx;
    DrawOpeningShotPath(matrices);
    gfx = g_DisplayListHead;
    DrawLensFlare(&gfx);
    DrawOpeningImpactLines(&gfx);
    DrawOpeningBurst(&gfx);
    DrawLakitu(&gfx);
    DrawOpeningBalls(&gfx, matrices);
    DrawOpeningShotBall(&gfx);
    DrawOpeningSparks(&gfx);
    DrawNameLogos(&gfx);
    DrawOpeningLogo(&gfx, 0);
    *displayListHead = gfx;
}

void LoadNameLogo(s32 logoIndex, s32 slot) {
    EncodedAsset asset;

    OpenEncodedAsset(f_OpeningNameLogos[logoIndex].assetId, &asset);
    ReadEncodedAsset(f_NameSlotImages[slot], &asset);
    f_NameSlotLogoIndices[slot] = logoIndex;
    f_NameSlotSteps[slot] = 0;
    f_IsNameSlotShown[slot] = FALSE;
}

#define SLOT_LOGO f_OpeningNameLogos[f_NameSlotLogoIndices[slot]]
#define NAME_LOGO_STRIP_COUNT 4
#define NAME_LOGO_STRIP_ROWS 8
#define NAME_LOGO_STRIP_ROWS_SHIFT 3

void DrawNameLogos(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 xEdges[2];
    s32 yEdges[NAME_LOGO_STRIP_COUNT + 1];
    s32 strip;
    s32 slot;
    Gfx *cycleTypeCommand;
    s32 stripHeight;
    s32 texelsOffset;

    for (slot = 0; slot != OPENING_NAME_SLOT_COUNT; slot++) {
        if (f_IsNameSlotShown[slot]) {
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            cycleTypeCommand = gfx++;
            gDPSetCycleType(cycleTypeCommand, G_CYC_1CYCLE);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
            gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            gDPPipeSync(gfx++);
            gDPSetColorDither(gfx++, G_CD_DISABLE);
            gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
            gDPPipeSync(gfx++);
            gDPSetTexturePersp(gfx++, G_TP_NONE);
            gDPPipeSync(gfx++);
            gDPSetTextureFilter(gfx++, G_TF_BILERP);
            gDPPipeSync(gfx++);
            gDPSetAlphaCompare(gfx++, G_AC_NONE);
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_RGBA16);
            gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_NameSlotImages[slot])));
            gDPPipeSync(gfx++);
            for (strip = 0; strip != NAME_LOGO_STRIP_COUNT; strip++) {
                texelsOffset =
                    LARGE_PALETTED_TEXELS_OFFSET + (strip * SLOT_LOGO.width * NAME_LOGO_STRIP_ROWS);
                stripHeight = SLOT_LOGO.shownHeight / NAME_LOGO_STRIP_COUNT;
                xEdges[0] = SLOT_LOGO.x - (SLOT_LOGO.shownWidth / 2);
                xEdges[1] = SLOT_LOGO.x + (SLOT_LOGO.shownWidth / 2);
                yEdges[0] = SLOT_LOGO.y - (SLOT_LOGO.shownHeight / 2);
                yEdges[1] = yEdges[0] + stripHeight;
                yEdges[2] = yEdges[1] + stripHeight;
                yEdges[3] = yEdges[2] + stripHeight;
                yEdges[4] = yEdges[3] + stripHeight;
                if (f_NameSlotSteps[slot] < NAME_LOGO_FLIP_UP) {
                    s32 drawnWidth;
                    s32 drawnHeight;

                    gDPLoadTextureBlock(
                        gfx++,
                        TextureAddress(f_NameSlotImages[slot] + texelsOffset),
                        G_IM_FMT_CI,
                        G_IM_SIZ_8b,
                        SLOT_LOGO.width,
                        SLOT_LOGO.height / NAME_LOGO_STRIP_COUNT,
                        0,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_MIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        NAME_LOGO_STRIP_ROWS_SHIFT,
                        G_TX_NOLOD,
                        G_TX_NOLOD
                    );
                    gDPPipeSync(gfx++);
                    drawnWidth = xEdges[1] - xEdges[0];
                    if (drawnWidth <= 0) {
                        drawnWidth = 1;
                    }

                    drawnHeight = yEdges[NAME_LOGO_STRIP_COUNT - strip] -
                        yEdges[NAME_LOGO_STRIP_COUNT - 1 - strip];
                    if (drawnHeight <= 0) {
                        drawnHeight = 1;
                    }

                    gSPScisTextureRectangle(
                        gfx++,
                        QuarterPixels(xEdges[0]),
                        QuarterPixels(yEdges[NAME_LOGO_STRIP_COUNT - 1 - strip]),
                        QuarterPixels(xEdges[1]),
                        QuarterPixels(yEdges[NAME_LOGO_STRIP_COUNT - strip]),
                        G_TX_RENDERTILE,
                        0,
                        TexelCoordinate(NAME_LOGO_STRIP_ROWS),
                        TexelRunStep(SLOT_LOGO.width, drawnWidth),
                        (SLOT_LOGO.height << 8) / drawnHeight
                    );
                }
                else {
                    s32 drawnWidth;
                    s32 drawnHeight;

                    gDPLoadTextureBlock(
                        gfx++,
                        TextureAddress(f_NameSlotImages[slot] + texelsOffset),
                        G_IM_FMT_CI,
                        G_IM_SIZ_8b,
                        SLOT_LOGO.width,
                        SLOT_LOGO.height / NAME_LOGO_STRIP_COUNT,
                        0,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOLOD
                    );
                    gDPPipeSync(gfx++);
                    drawnWidth = xEdges[1] - xEdges[0];
                    if (drawnWidth <= 0) {
                        drawnWidth = 1;
                    }

                    drawnHeight = yEdges[strip + 1] - yEdges[strip];
                    if (drawnHeight <= 0) {
                        drawnHeight = 1;
                    }

                    gSPScisTextureRectangle(
                        gfx++,
                        QuarterPixels(xEdges[0]),
                        QuarterPixels(yEdges[strip]),
                        QuarterPixels(xEdges[1]),
                        QuarterPixels(yEdges[strip + 1]),
                        G_TX_RENDERTILE,
                        0,
                        0,
                        TexelRunStep(SLOT_LOGO.width, drawnWidth),
                        (SLOT_LOGO.height << 8) / drawnHeight
                    );
                }

                gDPPipeSync(gfx++);
            }
        }
    }

    *displayListHead = gfx;
}

void AnimateNameLogos(void) {
    s32 slot;

    for (slot = 0; slot != OPENING_NAME_SLOT_COUNT; slot++) {
        if (!f_IsNameSlotShown[slot]) {
            continue;
        }

        if (f_NameSlotSteps[slot] < NAME_LOGO_FLIP_DOWN) {
            f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownWidth +=
                f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].width / 8;
            f_NameSlotSteps[slot]++;
            if (f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownWidth >
                f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].width) {
                f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownWidth =
                    f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].width;
            }

            if (f_NameSlotSteps[slot] == NAME_LOGO_UNFOLD + 8) {
                f_NameSlotSteps[slot] = NAME_LOGO_FLIP_DOWN;
            }
        }
        else if (f_NameSlotSteps[slot] < NAME_LOGO_FLIP_UP) {
            f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownHeight -=
                f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].height / 4;
            f_NameSlotSteps[slot]++;
            if (f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownHeight <= 0) {
                f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownHeight = 1;
            }

            if (f_NameSlotSteps[slot] == NAME_LOGO_FLIP_DOWN + 4) {
                f_NameSlotSteps[slot] = NAME_LOGO_FLIP_UP;
            }
        }
        else if (f_NameSlotSteps[slot] < NAME_LOGO_STILL) {
            f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownHeight +=
                f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].height / 4;
            f_NameSlotSteps[slot]++;
            if (f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownHeight >
                f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].height) {
                f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownHeight =
                    f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].height;
            }

            if (f_NameSlotSteps[slot] == NAME_LOGO_FLIP_UP + 4) {
                f_NameSlotSteps[slot] = NAME_LOGO_STILL;
            }
        }
        else {
            f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownWidth =
                f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].width;
            f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownHeight =
                f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].height;
        }
    }
}

void LoadOpeningBall(OpeningBallImage image, s32 slot) {
    EncodedAsset asset;

    OpenEncodedAsset(f_OpeningBallAssets[image], &asset);
    ReadEncodedAsset(f_OpeningBallImages[slot], &asset);
    f_OpeningBallStates[slot] = OPENING_BALL_HIDDEN;
    f_OpeningBallScales[slot] = OPENING_BALL_NORMAL_SCALE;
}

#define LOGO_BALL_FRAME_COUNT 3
#define LOGO_BALL_FRAME_SIZE (OPENING_BALL_IMAGE_WIDTH * OPENING_BALL_IMAGE_WIDTH)
#define LOGO_BALL_FRAME_ANGLE 4.0f
#define LOGO_BALL_START_ROLL (-40.0f)
#define LOGO_BALL_START_PITCH 0.0f
#define LOGO_BALL_START_YAW (-30.0f)
#define LOGO_MARIO_IMAGE_SIZE 24

#define OPENING_BALL_MODEL_SCALE 0.7f
#define OPENING_BALL_IN_AIR_SHADOW_DROP 0.25f
#define OPENING_BALL_LARGE_SHADOW_DROP 1.0f
#define PROJECTED_BALL_LARGE_SHADOW_DROP 2.0f
#define OPENING_BALL_LARGE_LIFT 0.5f
#define LOGO_BALL_MARIO_DROP 1.5f
#define PROJECTED_LOGO_BALL_MARIO_DROP 1.7f
#define LOGO_BALL_MARIO_SCALE 0.55f
#define PROJECTED_LOGO_BALL_MARIO_SCALE 0.6f
#define LOGO_BALL_MARIO_OFFSET_Y (-0.3f)
#define PROJECTED_LOGO_BALL_MARIO_OFFSET_Y (-0.38f)

#define PROJECTED_BALL_WIDTH_PER_SCALE 1.5f
#define PROJECTED_BALL_HALF_SIZE_MIN 2
#define PROJECTED_BALL_HALF_SIZE_LIMIT 256
#define PROJECTED_BALL_DROP 2

void DrawOpeningBalls(Gfx **displayListHead, CameraMatrices *matrices) {
    Vec3 movement;
    Mat4 turn;
    Vec3 position;
    ScreenPoint ballPoint;
    Mat4 projectedTurn;
    Gfx *gfx;
    s32 slot;
    s32 stripIndex;
    s32 halfSize;
    f32 screenScale;
    f32 rollAngle;

    if (f_IsLogoBallSlot[LOGO_BALL_SLOT] == FALSE) {
        gfx = *displayListHead;
        for (slot = 0; slot < OPENING_BALL_SLOT_COUNT; slot++) {
            if (f_OpeningBallStates[slot] != OPENING_BALL_HIDDEN) {
                position[AXIS_X] = f_OpeningBallPositions[slot][AXIS_X];
                position[AXIS_Z] = f_OpeningBallPositions[slot][AXIS_Z];
                position[AXIS_Y] = GetGroundHeight(
                                       position[AXIS_X] * WORLD_TO_COURSE_SCALE,
                                       position[AXIS_Z] * WORLD_TO_COURSE_SCALE
                                   ) *
                    COURSE_TO_WORLD_SCALE;
                gDPPipeSync(gfx++);
                gDPPipeSync(gfx++);
                gDPSetCycleType(gfx++, G_CYC_1CYCLE);
                if (f_OpeningBallStates[slot] == OPENING_BALL_IN_AIR) {
                    guTranslate(
                        &f_OpeningBallShadowMatrices[slot][g_DrawBufferIndex],
                        position[AXIS_X],
                        position[AXIS_Y] - OPENING_BALL_IN_AIR_SHADOW_DROP,
                        position[AXIS_Z]
                    );
                }
                else if (f_OpeningBallScales[slot] == OPENING_BALL_LARGE_SCALE) {
                    guTranslate(
                        &f_OpeningBallShadowMatrices[slot][g_DrawBufferIndex],
                        position[AXIS_X],
                        position[AXIS_Y] - OPENING_BALL_LARGE_SHADOW_DROP,
                        position[AXIS_Z]
                    );
                }
                else {
                    guTranslate(
                        &f_OpeningBallShadowMatrices[slot][g_DrawBufferIndex],
                        position[AXIS_X],
                        position[AXIS_Y],
                        position[AXIS_Z]
                    );
                }

                guScale(
                    &f_OpeningBallScaleMatrices[slot],
                    f_OpeningBallScales[slot] * OPENING_BALL_MODEL_SCALE,
                    f_OpeningBallScales[slot] * OPENING_BALL_MODEL_SCALE,
                    f_OpeningBallScales[slot] * OPENING_BALL_MODEL_SCALE
                );
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
                gDPSetCombineLERP(
                    gfx++,
                    0,
                    0,
                    0,
                    0,
                    TEXEL0,
                    0,
                    PRIMITIVE,
                    0,
                    0,
                    0,
                    0,
                    0,
                    TEXEL0,
                    0,
                    PRIMITIVE,
                    0
                );
                gDPPipeSync(gfx++);
                gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
                gDPSetBlendColor(gfx++, 255, 255, 255, 16);
                gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 127);
                gDPPipeSync(gfx++);
                gDPSetAlphaDither(gfx++, G_AD_PATTERN);
                gDPPipeSync(gfx++);
                gDPSetColorDither(gfx++, G_CD_MAGICSQ);
                gDPPipeSync(gfx++);
                gDPSetTexturePersp(gfx++, G_TP_PERSP);
                gDPPipeSync(gfx++);
                gDPSetTextureLUT(gfx++, G_TT_NONE);
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress(ImageBody(g_ShadowTexture)),
                    G_IM_FMT_I,
                    G_IM_SIZ_8b,
                    SHADOW_TEXTURE_SIZE,
                    SHADOW_TEXTURE_SIZE,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                gDPPipeSync(gfx++);
                gDPSetTextureLOD(gfx++, G_TL_TILE);
                gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
                gSPLoadGeometryMode(gfx++, 0);
                gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
                gSPMatrix(
                    gfx++,
                    PhysicalAddress(&f_OpeningBallShadowMatrices[slot][g_DrawBufferIndex]),
                    G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW
                );
                gSPMatrix(
                    gfx++,
                    PhysicalAddress(&f_OpeningBallScaleMatrices[slot]),
                    G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
                );
                gSPVertex(gfx++, f_OpeningBallShadowVertices, QUAD_VERTEX_COUNT, 0);
                gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
                gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
                gDPPipeSync(gfx++);
                if (f_OpeningBallStates[slot] == OPENING_BALL_IN_AIR) {
                    position[AXIS_Y] = f_OpeningBallPositions[slot][AXIS_Y];
                }

                if (f_OpeningBallScales[slot] == OPENING_BALL_LARGE_SCALE) {
                    position[AXIS_Y] += OPENING_BALL_LARGE_LIFT;
                }

                guTranslate(
                    &f_OpeningBallMatrices[slot][g_DrawBufferIndex],
                    position[AXIS_X],
                    position[AXIS_Y],
                    position[AXIS_Z]
                );
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
                gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                gDPPipeSync(gfx++);
                gDPSetAlphaDither(gfx++, G_AD_PATTERN);
                gDPPipeSync(gfx++);
                gDPSetColorDither(gfx++, G_CD_MAGICSQ);
                gDPPipeSync(gfx++);
                gDPSetTexturePersp(gfx++, G_TP_PERSP);
                gDPPipeSync(gfx++);
                gDPSetTextureFilter(gfx++, G_TF_BILERP);
                gDPPipeSync(gfx++);
                gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                gDPPipeSync(gfx++);
                gDPSetTextureLOD(gfx++, G_TL_TILE);
                gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
                if (f_IsLogoBallSlot[slot]) {
                    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_LogoBallImages)));
                }
                else {
                    gDPLoadTLUT_pal256(
                        gfx++,
                        TextureAddress(ImagePalette(f_OpeningBallImages[slot]))
                    );
                }

                for (stripIndex = 0; stripIndex < OPENING_BALL_STRIP_COUNT; stripIndex++) {
                    gDPPipeSync(gfx++);
                    gSPLoadGeometryMode(gfx++, 0);
                    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
                    gSPMatrix(
                        gfx++,
                        PhysicalAddress(&f_OpeningBallMatrices[slot][g_DrawBufferIndex]),
                        G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW
                    );
                    gSPMatrix(
                        gfx++,
                        PhysicalAddress(&matrices->billboard),
                        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
                    );
                    gSPMatrix(
                        gfx++,
                        PhysicalAddress(&f_OpeningBallScaleMatrices[slot]),
                        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
                    );
                    if (f_IsLogoBallSlot[slot]) {
                        gDPLoadTextureBlock(
                            gfx++,
                            TextureAddress(
                                LargeImageTexels(f_LogoBallImages) +
                                ((stripIndex * OPENING_BALL_STRIP_SIZE) +
                                    (f_LogoBallFrame * LOGO_BALL_FRAME_SIZE))
                            ),
                            G_IM_FMT_CI,
                            G_IM_SIZ_8b,
                            OPENING_BALL_IMAGE_WIDTH,
                            OPENING_BALL_STRIP_HEIGHT,
                            0,
                            G_TX_NOMIRROR | G_TX_WRAP,
                            G_TX_NOMIRROR | G_TX_WRAP,
                            G_TX_NOMASK,
                            G_TX_NOMASK,
                            G_TX_NOLOD,
                            G_TX_NOLOD
                        );
                    }
                    else {
                        gDPLoadTextureBlock(
                            gfx++,
                            TextureAddress(
                                LargeImageTexels(f_OpeningBallImages[slot]) +
                                (stripIndex * OPENING_BALL_STRIP_SIZE)
                            ),
                            G_IM_FMT_CI,
                            G_IM_SIZ_8b,
                            OPENING_BALL_IMAGE_WIDTH,
                            OPENING_BALL_STRIP_HEIGHT,
                            0,
                            G_TX_NOMIRROR | G_TX_WRAP,
                            G_TX_NOMIRROR | G_TX_WRAP,
                            G_TX_NOMASK,
                            G_TX_NOMASK,
                            G_TX_NOLOD,
                            G_TX_NOLOD
                        );
                    }

                    gDPPipeSync(gfx++);
                    gSPVertex(gfx++, f_OpeningBallStripVertices[stripIndex], QUAD_VERTEX_COUNT, 0);
                    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
                }

                if (f_IsLogoBallSlot[slot]) {
                    Gfx *cycleTypeCommand;

                    if (f_ShouldResetLogoBallRoll) {
                        guMtxIdentF(f_LogoBallRoll);
                        f_ShouldResetLogoBallRoll = FALSE;
                        guRotateRPYF(
                            f_LogoBallRoll,
                            LOGO_BALL_START_ROLL,
                            LOGO_BALL_START_PITCH,
                            LOGO_BALL_START_YAW
                        );
                    }

                    if (f_HasLogoBallLastPosition) {
                        movement[AXIS_X] =
                            f_OpeningBallPositions[slot][AXIS_X] - f_LogoBallLastPosition[AXIS_X];
                        movement[AXIS_Y] =
                            f_OpeningBallPositions[slot][AXIS_Y] - f_LogoBallLastPosition[AXIS_Y];
                        movement[AXIS_Z] =
                            f_OpeningBallPositions[slot][AXIS_Z] - f_LogoBallLastPosition[AXIS_Z];
                    }
                    else {
                        f_HasLogoBallLastPosition = TRUE;
                        movement[AXIS_X] = 0.0f;
                        movement[AXIS_Y] = 0.0f;
                        movement[AXIS_Z] = 0.0f;
                    }

                    rollAngle = sqrtf(
                        (movement[AXIS_X] * movement[AXIS_X]) +
                        (movement[AXIS_Y] * movement[AXIS_Y]) +
                        (movement[AXIS_Z] * movement[AXIS_Z])
                    );
                    rollAngle *= (f32)RADIANS_TO_DEGREES;
                    f_LogoBallLastPosition[AXIS_X] = f_OpeningBallPositions[slot][AXIS_X];
                    f_LogoBallLastPosition[AXIS_Y] = f_OpeningBallPositions[slot][AXIS_Y];
                    f_LogoBallLastPosition[AXIS_Z] = f_OpeningBallPositions[slot][AXIS_Z];
                    rollAngle = fabs(rollAngle);
                    if (rollAngle > LOGO_BALL_FRAME_ANGLE) {
                        f_LogoBallFrame++;
                        if (f_LogoBallFrame == LOGO_BALL_FRAME_COUNT) {
                            f_LogoBallFrame = 0;
                        }
                    }

                    guScale(
                        &f_LogoBallMarioScale,
                        LOGO_BALL_MARIO_SCALE,
                        LOGO_BALL_MARIO_SCALE,
                        LOGO_BALL_MARIO_SCALE
                    );
                    if (movement[AXIS_Z] != 0.0f || movement[AXIS_X] != 0.0f) {
                        guRotateF(turn, rollAngle, -movement[AXIS_Z], 0.0f, movement[AXIS_X]);
                        guMtxCatF(f_LogoBallRoll, turn, f_LogoBallRoll);
                        ConvertMatrixToFixed(
                            f_LogoBallRoll,
                            &f_LogoBallMarioRolls[g_DrawBufferIndex]
                        );
                    }

                    guTranslate(&f_LogoBallMarioOffset, 0.0f, LOGO_BALL_MARIO_OFFSET_Y, 0.0f);
                    guTranslate(
                        &f_LogoBallMarioTranslations[g_DrawBufferIndex],
                        f_OpeningBallPositions[slot][AXIS_X],
                        f_OpeningBallPositions[slot][AXIS_Y] - LOGO_BALL_MARIO_DROP,
                        f_OpeningBallPositions[slot][AXIS_Z]
                    );
                    gDPPipeSync(gfx++);
                    cycleTypeCommand = gfx++;
                    gDPSetCycleType(cycleTypeCommand, G_CYC_1CYCLE);
                    {
                        Gfx *renderModeCommand;

                        gDPPipeSync(gfx++);
                        renderModeCommand = gfx++;
                        gDPSetRenderMode(renderModeCommand, G_RM_XLU_SURF, G_RM_XLU_SURF2);
                    }
                    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                    gDPPipeSync(gfx++);
                    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
                    gDPPipeSync(gfx++);
                    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
                    gDPPipeSync(gfx++);
                    gDPSetTexturePersp(gfx++, G_TP_PERSP);
                    gDPPipeSync(gfx++);
                    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_LogoMarioImage)));
                    gDPLoadTextureBlock(
                        gfx++,
                        TextureAddress(LargeImageTexels(f_LogoMarioImage)),
                        G_IM_FMT_CI,
                        G_IM_SIZ_8b,
                        LOGO_MARIO_IMAGE_SIZE,
                        LOGO_MARIO_IMAGE_SIZE,
                        0,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOLOD
                    );
                    gDPPipeSync(gfx++);
                    gDPSetTextureLOD(gfx++, G_TL_TILE);
                    gSPTexture(
                        gfx++,
                        TEXTURE_SCALE_HALF,
                        TEXTURE_SCALE_HALF,
                        0,
                        G_TX_RENDERTILE,
                        G_ON
                    );
                    gSPLoadGeometryMode(gfx++, 0);
                    gSPSetGeometryMode(gfx++, G_SHADE | G_CULL_FRONT | G_SHADING_SMOOTH);
                    gSPMatrix(
                        gfx++,
                        PhysicalAddress(&f_LogoBallMarioTranslations[g_DrawBufferIndex]),
                        G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW
                    );
                    gSPMatrix(
                        gfx++,
                        PhysicalAddress(&f_LogoBallMarioRolls[g_DrawBufferIndex]),
                        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
                    );
                    gSPMatrix(
                        gfx++,
                        PhysicalAddress(&f_LogoBallMarioScale),
                        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
                    );
                    gSPMatrix(
                        gfx++,
                        PhysicalAddress(&f_LogoBallMarioOffset),
                        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
                    );
                    if (f_OpeningScene >= OPENING_SCENE_LOGO_BALL_MARIO &&
                        f_OpeningSceneFramesLeft < LOGO_BALL_MARIO_FRAMES_LEFT) {
                        gSPVertex(gfx++, f_LogoBallMarioVertices, QUAD_VERTEX_COUNT, 0);
                        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
                    }

                    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
                    gDPPipeSync(gfx++);
                }

                gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
                gDPPipeSync(gfx++);
            }
        }

        *displayListHead = gfx;
    }
    else {
        Gfx *gfx;
        s32 stripIndex;

        gfx = *displayListHead;
        for (slot = 0; slot != OPENING_BALL_SLOT_COUNT; slot++) {
            if (f_OpeningBallStates[slot] != OPENING_BALL_HIDDEN) {
                f_ProjectedBallPosition[AXIS_X] = f_OpeningBallPositions[slot][AXIS_X];
                f_ProjectedBallPosition[AXIS_Z] = f_OpeningBallPositions[slot][AXIS_Z];
                if (f_OpeningBallStates[slot] == OPENING_BALL_IN_AIR) {
                    f_ProjectedBallPosition[AXIS_Y] = f_OpeningBallPositions[slot][AXIS_Y];
                }
                else {
                    f_ProjectedBallPosition[AXIS_Y] =
                        GetGroundHeight(
                            f_ProjectedBallPosition[AXIS_X] * WORLD_TO_COURSE_SCALE,
                            f_ProjectedBallPosition[AXIS_Z] * WORLD_TO_COURSE_SCALE
                        ) *
                        COURSE_TO_WORLD_SCALE;
                }

                if (ProjectPoint(
                        f_ProjectedBallPosition[AXIS_X],
                        f_ProjectedBallPosition[AXIS_Y],
                        f_ProjectedBallPosition[AXIS_Z],
                        &ballPoint
                    ) < 0) {
                    screenScale = ballPoint.scaleX;
                    if (screenScale < 0.0f) {
                        screenScale = -screenScale;
                    }

                    halfSize =
                        screenScale * (PROJECTED_BALL_WIDTH_PER_SCALE * f_OpeningBallScales[slot]);
                    if (halfSize & 1) {
                        f_ProjectedBallExtraPixel = 1;
                    }
                    else {
                        f_ProjectedBallExtraPixel = 0;
                    }

                    halfSize /= 2;
                    f_ProjectedBallLeft = ballPoint.x - halfSize;
                    f_ProjectedBallRight = ballPoint.x + halfSize;
                    if (f_OpeningBallStates[slot] == OPENING_BALL_IN_AIR) {
                        if (halfSize < PROJECTED_BALL_HALF_SIZE_MIN) {
                            halfSize = PROJECTED_BALL_HALF_SIZE_MIN;
                        }
                    }

                    f_ProjectedBallPosition[AXIS_Y] =
                        GetGroundHeight(
                            f_ProjectedBallPosition[AXIS_X] * WORLD_TO_COURSE_SCALE,
                            f_ProjectedBallPosition[AXIS_Z] * WORLD_TO_COURSE_SCALE
                        ) *
                        COURSE_TO_WORLD_SCALE;
                    if (f_OpeningBallScales[slot] == OPENING_BALL_LARGE_SCALE) {
                        f_ProjectedBallPosition[AXIS_Y] -= PROJECTED_BALL_LARGE_SHADOW_DROP;
                    }

                    if (f_OpeningBallStates[slot] == OPENING_BALL_IN_AIR) {
                        guTranslate(
                            &f_ProjectedShadowMatrices[g_DrawBufferIndex][slot],
                            f_ProjectedBallPosition[AXIS_X],
                            f_ProjectedBallPosition[AXIS_Y] - OPENING_BALL_IN_AIR_SHADOW_DROP,
                            f_ProjectedBallPosition[AXIS_Z]
                        );
                    }
                    else {
                        guTranslate(
                            &f_ProjectedShadowMatrices[g_DrawBufferIndex][slot],
                            f_ProjectedBallPosition[AXIS_X],
                            f_ProjectedBallPosition[AXIS_Y],
                            f_ProjectedBallPosition[AXIS_Z]
                        );
                    }

                    guScale(
                        &f_ProjectedShadowScaleMatrices[slot],
                        f_OpeningBallScales[slot],
                        f_OpeningBallScales[slot],
                        f_OpeningBallScales[slot]
                    );
                    gDPPipeSync(gfx++);
                    gDPPipeSync(gfx++);
                    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
                    gDPPipeSync(gfx++);
                    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
                    gDPSetCombineLERP(
                        gfx++,
                        0,
                        0,
                        0,
                        0,
                        TEXEL0,
                        0,
                        PRIMITIVE,
                        0,
                        0,
                        0,
                        0,
                        0,
                        TEXEL0,
                        0,
                        PRIMITIVE,
                        0
                    );
                    gDPPipeSync(gfx++);
                    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
                    gDPSetBlendColor(gfx++, 255, 255, 255, 16);
                    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 127);
                    gDPPipeSync(gfx++);
                    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
                    gDPPipeSync(gfx++);
                    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
                    gDPPipeSync(gfx++);
                    gDPSetTexturePersp(gfx++, G_TP_PERSP);
                    gDPPipeSync(gfx++);
                    gDPSetTextureLUT(gfx++, G_TT_NONE);
                    gDPLoadTextureBlock(
                        gfx++,
                        TextureAddress(ImageBody(g_ShadowTexture)),
                        G_IM_FMT_I,
                        G_IM_SIZ_8b,
                        SHADOW_TEXTURE_SIZE,
                        SHADOW_TEXTURE_SIZE,
                        0,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOLOD
                    );
                    gDPPipeSync(gfx++);
                    gDPSetTextureLOD(gfx++, G_TL_TILE);
                    gSPTexture(
                        gfx++,
                        TEXTURE_SCALE_HALF,
                        TEXTURE_SCALE_HALF,
                        0,
                        G_TX_RENDERTILE,
                        G_ON
                    );
                    gSPLoadGeometryMode(gfx++, 0);
                    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
                    gSPMatrix(
                        gfx++,
                        PhysicalAddress(&f_ProjectedShadowMatrices[g_DrawBufferIndex][slot]),
                        G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW
                    );
                    gSPMatrix(
                        gfx++,
                        PhysicalAddress(&f_ProjectedShadowScaleMatrices[slot]),
                        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
                    );
                    gSPVertex(gfx++, f_ProjectedShadowVertices, QUAD_VERTEX_COUNT, 0);
                    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
                    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
                    gDPPipeSync(gfx++);
                    gDPPipeSync(gfx++);
                    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
                    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                    gDPPipeSync(gfx++);
                    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
                    gSPTexture(
                        gfx++,
                        TEXTURE_SCALE_HALF,
                        TEXTURE_SCALE_HALF,
                        0,
                        G_TX_RENDERTILE,
                        G_ON
                    );
                    gDPPipeSync(gfx++);
                    gDPSetTexturePersp(gfx++, G_TP_NONE);
                    gDPPipeSync(gfx++);
                    gDPSetTextureFilter(gfx++, G_TF_BILERP);
                    gDPPipeSync(gfx++);
                    gDPSetAlphaCompare(gfx++, G_AC_NONE);
                    gDPPipeSync(gfx++);
                    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                    if (f_IsLogoBallSlot[slot]) {
                        gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_LogoBallImages)));
                    }
                    else {
                        gDPLoadTLUT_pal256(
                            gfx++,
                            TextureAddress(ImagePalette(f_OpeningBallImages[slot]))
                        );
                    }

                    gDPPipeSync(gfx++);
                    for (stripIndex = 0; stripIndex != OPENING_BALL_STRIP_COUNT; stripIndex++) {
                        if (f_IsLogoBallSlot[slot]) {
                            gDPLoadTextureBlock(
                                gfx++,
                                TextureAddress(
                                    LargeImageTexels(f_LogoBallImages) +
                                    ((stripIndex * OPENING_BALL_STRIP_SIZE) +
                                        (f_ProjectedLogoBallFrame * LOGO_BALL_FRAME_SIZE))
                                ),
                                G_IM_FMT_CI,
                                G_IM_SIZ_8b,
                                OPENING_BALL_IMAGE_WIDTH,
                                OPENING_BALL_STRIP_HEIGHT,
                                0,
                                G_TX_NOMIRROR | G_TX_WRAP,
                                G_TX_NOMIRROR | G_TX_WRAP,
                                G_TX_NOMASK,
                                G_TX_NOMASK,
                                G_TX_NOLOD,
                                G_TX_NOLOD
                            );
                        }
                        else {
                            gDPLoadTextureBlock(
                                gfx++,
                                TextureAddress(
                                    LargeImageTexels(f_OpeningBallImages[slot]) +
                                    (stripIndex * OPENING_BALL_STRIP_SIZE)
                                ),
                                G_IM_FMT_CI,
                                G_IM_SIZ_8b,
                                OPENING_BALL_IMAGE_WIDTH,
                                OPENING_BALL_STRIP_HEIGHT,
                                0,
                                G_TX_NOMIRROR | G_TX_WRAP,
                                G_TX_NOMIRROR | G_TX_WRAP,
                                G_TX_NOMASK,
                                G_TX_NOMASK,
                                G_TX_NOLOD,
                                G_TX_NOLOD
                            );
                        }

                        gDPPipeSync(gfx++);
                        gDPPipeSync(gfx++);
                        if (halfSize > 0 && halfSize < PROJECTED_BALL_HALF_SIZE_LIMIT) {
                            if (f_IsLogoBallSlot[slot]) {
                                f_ProjectedBallLeft =
                                    ballPoint.x - halfSize - f_ProjectedBallExtraPixel;
                                f_ProjectedBallRight = ballPoint.x + halfSize;
                                f_ProjectedBallTop = ballPoint.y -
                                    (halfSize * (OPENING_BALL_STRIP_COUNT - stripIndex)) -
                                    f_ProjectedBallExtraPixel;
                                f_ProjectedBallBottom =
                                    ballPoint.y + (stripIndex * halfSize) - halfSize;
                            }
                            else {
                                f_ProjectedBallLeft =
                                    ballPoint.x - halfSize - f_ProjectedBallExtraPixel;
                                f_ProjectedBallRight = ballPoint.x + halfSize;
                                f_ProjectedBallTop = ballPoint.y -
                                    (halfSize * (OPENING_BALL_STRIP_COUNT - stripIndex)) -
                                    f_ProjectedBallExtraPixel + PROJECTED_BALL_DROP;
                                f_ProjectedBallBottom = ballPoint.y + (stripIndex * halfSize) -
                                    halfSize + PROJECTED_BALL_DROP;
                            }

                            f_ProjectedBallTexelStep = OPENING_BALL_STRIP_HEIGHT * TEXEL_STEP_ONE /
                                (halfSize + f_ProjectedBallExtraPixel);
                            gSPScisTextureRectangle(
                                gfx++,
                                QuarterPixels(f_ProjectedBallLeft),
                                QuarterPixels(f_ProjectedBallTop),
                                QuarterPixels(f_ProjectedBallRight),
                                QuarterPixels(f_ProjectedBallBottom),
                                G_TX_RENDERTILE,
                                0,
                                0,
                                f_ProjectedBallTexelStep,
                                f_ProjectedBallTexelStep
                            );
                        }

                        gDPPipeSync(gfx++);
                    }

                    if (f_IsLogoBallSlot[slot]) {
                        if (f_ShouldResetProjectedLogoBallRoll) {
                            guMtxIdentF(f_ProjectedLogoBallRoll);
                            f_ShouldResetProjectedLogoBallRoll = FALSE;
                            guRotateRPYF(
                                f_ProjectedLogoBallRoll,
                                LOGO_BALL_START_ROLL,
                                LOGO_BALL_START_PITCH,
                                LOGO_BALL_START_YAW
                            );
                        }

                        if (f_HasProjectedLogoBallLastPosition) {
                            movement[AXIS_X] = f_OpeningBallPositions[slot][AXIS_X] -
                                f_ProjectedLogoBallLastPosition[AXIS_X];
                            movement[AXIS_Y] = f_OpeningBallPositions[slot][AXIS_Y] -
                                f_ProjectedLogoBallLastPosition[AXIS_Y];
                            movement[AXIS_Z] = f_OpeningBallPositions[slot][AXIS_Z] -
                                f_ProjectedLogoBallLastPosition[AXIS_Z];
                        }
                        else {
                            f_HasProjectedLogoBallLastPosition = TRUE;
                            movement[AXIS_X] = 0.0f;
                            movement[AXIS_Y] = 0.0f;
                            movement[AXIS_Z] = 0.0f;
                        }

                        rollAngle = sqrtf(
                            (movement[AXIS_X] * movement[AXIS_X]) +
                            (movement[AXIS_Y] * movement[AXIS_Y]) +
                            (movement[AXIS_Z] * movement[AXIS_Z])
                        );
                        rollAngle *= (f32)RADIANS_TO_DEGREES;
                        f_ProjectedLogoBallLastPosition[AXIS_X] =
                            f_OpeningBallPositions[slot][AXIS_X];
                        f_ProjectedLogoBallLastPosition[AXIS_Y] =
                            f_OpeningBallPositions[slot][AXIS_Y];
                        f_ProjectedLogoBallLastPosition[AXIS_Z] =
                            f_OpeningBallPositions[slot][AXIS_Z];
                        rollAngle = fabs(rollAngle);
                        if (rollAngle > LOGO_BALL_FRAME_ANGLE) {
                            f_ProjectedLogoBallFrame++;
                            if (f_ProjectedLogoBallFrame == LOGO_BALL_FRAME_COUNT) {
                                f_ProjectedLogoBallFrame = 0;
                            }
                        }

                        guScale(
                            &f_ProjectedLogoBallMarioScale,
                            PROJECTED_LOGO_BALL_MARIO_SCALE,
                            PROJECTED_LOGO_BALL_MARIO_SCALE,
                            PROJECTED_LOGO_BALL_MARIO_SCALE
                        );
                        if (movement[AXIS_Z] != 0.0f || movement[AXIS_X] != 0.0f) {
                            guRotateF(
                                projectedTurn,
                                rollAngle,
                                -movement[AXIS_Z],
                                0.0f,
                                movement[AXIS_X]
                            );
                            guMtxCatF(
                                f_ProjectedLogoBallRoll,
                                projectedTurn,
                                f_ProjectedLogoBallRoll
                            );
                            ConvertMatrixToFixed(
                                f_ProjectedLogoBallRoll,
                                &f_ProjectedLogoBallMarioRolls[g_DrawBufferIndex]
                            );
                        }

                        guTranslate(
                            &f_ProjectedLogoBallMarioOffset,
                            0.0f,
                            PROJECTED_LOGO_BALL_MARIO_OFFSET_Y,
                            0.0f
                        );
                        guTranslate(
                            &f_ProjectedLogoBallMarioTranslations[g_DrawBufferIndex],
                            f_OpeningBallPositions[slot][AXIS_X],
                            f_OpeningBallPositions[slot][AXIS_Y] - PROJECTED_LOGO_BALL_MARIO_DROP,
                            f_OpeningBallPositions[slot][AXIS_Z]
                        );
                        gDPPipeSync(gfx++);
                        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
                        gDPPipeSync(gfx++);
                        gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
                        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                        gDPPipeSync(gfx++);
                        gDPSetAlphaDither(gfx++, G_AD_PATTERN);
                        gDPPipeSync(gfx++);
                        gDPSetColorDither(gfx++, G_CD_MAGICSQ);
                        gDPPipeSync(gfx++);
                        gDPSetTexturePersp(gfx++, G_TP_PERSP);
                        gDPPipeSync(gfx++);
                        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                        gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_LogoMarioImage)));
                        gDPLoadTextureBlock(
                            gfx++,
                            TextureAddress(LargeImageTexels(f_LogoMarioImage)),
                            G_IM_FMT_CI,
                            G_IM_SIZ_8b,
                            LOGO_MARIO_IMAGE_SIZE,
                            LOGO_MARIO_IMAGE_SIZE,
                            0,
                            G_TX_NOMIRROR | G_TX_WRAP,
                            G_TX_NOMIRROR | G_TX_WRAP,
                            G_TX_NOMASK,
                            G_TX_NOMASK,
                            G_TX_NOLOD,
                            G_TX_NOLOD
                        );
                        gDPPipeSync(gfx++);
                        gDPSetTextureLOD(gfx++, G_TL_TILE);
                        gSPTexture(
                            gfx++,
                            TEXTURE_SCALE_HALF,
                            TEXTURE_SCALE_HALF,
                            0,
                            G_TX_RENDERTILE,
                            G_ON
                        );
                        gSPLoadGeometryMode(gfx++, 0);
                        gSPSetGeometryMode(gfx++, G_SHADE | G_CULL_FRONT | G_SHADING_SMOOTH);
                        gSPMatrix(
                            gfx++,
                            PhysicalAddress(
                                &f_ProjectedLogoBallMarioTranslations[g_DrawBufferIndex]
                            ),
                            G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW
                        );
                        gSPMatrix(
                            gfx++,
                            PhysicalAddress(&f_ProjectedLogoBallMarioRolls[g_DrawBufferIndex]),
                            G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
                        );
                        gSPMatrix(
                            gfx++,
                            PhysicalAddress(&f_ProjectedLogoBallMarioScale),
                            G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
                        );
                        gSPMatrix(
                            gfx++,
                            PhysicalAddress(&f_ProjectedLogoBallMarioOffset),
                            G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
                        );
                        gSPVertex(gfx++, f_ProjectedLogoBallMarioVertices, QUAD_VERTEX_COUNT, 0);
                        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
                        gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
                        gDPPipeSync(gfx++);
                    }
                }
            }
        }

        *displayListHead = gfx;
    }
}

#define ASSET_IMPACT_LINES_IMAGE 0x908

void LoadOpeningImpactLines(void) {
    EncodedAsset asset;

    OpenEncodedAsset(ASSET_IMPACT_LINES_IMAGE, &asset);
    ReadEncodedAsset(f_OpeningEffectImage, &asset);
}

#define IMPACT_LINES_LEFT 206
#define IMPACT_LINES_BOTTOM 88
#define IMPACT_LINES_SIZE 64
#define IMPACT_LINES_STRIP_HEIGHT 32
#define IMPACT_LINES_STRIP_COUNT 2
#define IMPACT_LINES_FIRST_BLINK_FRAME 3
#define IMPACT_LINES_SECOND_BLINK_FRAME 9
#define IMPACT_LINES_BLINK_FRAME_COUNT 3
#define IMPACT_LINES_FRAME_COUNT 19
#define IsOutsideBlink(frame, blinkFrame) \
    ((frame) < (blinkFrame) || (frame) >= (blinkFrame) + IMPACT_LINES_BLINK_FRAME_COUNT)

void DrawOpeningImpactLines(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 frame;
    s32 stripIndex;
    s32 texelOffset;
    s32 row;
    s32 left;
    s32 bottom;
    s32 top;
    s32 bottomEdge;
    s32 offset;

    if (f_AreImpactLinesShown) {
        frame = f_ImpactLinesFrame++;
        if (IsOutsideBlink(frame, IMPACT_LINES_FIRST_BLINK_FRAME) &&
            IsOutsideBlink(frame, IMPACT_LINES_SECOND_BLINK_FRAME) &&
            f_ImpactLinesFrame < IMPACT_LINES_FRAME_COUNT) {
            gDPPipeSync(gfx++);
            AppendSyncedRdpState(gfx, gDPSetCycleType, G_CYC_1CYCLE);
            AppendSyncedRdpState(gfx, gDPSetRenderMode, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
            gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            AppendSyncedRdpState(gfx, gDPSetColorDither, G_CD_DISABLE);
            gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
            AppendSyncedRdpState(gfx, gDPSetTexturePersp, G_TP_NONE);
            AppendSyncedRdpState(gfx, gDPSetTextureFilter, G_TF_BILERP);
            AppendSyncedRdpState(gfx, gDPSetAlphaCompare, G_AC_NONE);
            AppendSyncedRdpState(gfx, gDPSetTextureLUT, G_TT_RGBA16);
            gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_OpeningEffectImage)));
            gDPPipeSync(gfx++);

            for (stripIndex = 0, row = IMPACT_LINES_STRIP_COUNT - 1;
                stripIndex != IMPACT_LINES_STRIP_COUNT;
                stripIndex++) {
                left = IMPACT_LINES_LEFT;

                bottomEdge = IMPACT_LINES_BOTTOM;
                offset = row * IMPACT_LINES_STRIP_HEIGHT;
                bottom = bottomEdge;
                bottom -= offset;
                offset += IMPACT_LINES_STRIP_HEIGHT;
                top = bottomEdge - offset;
                texelOffset = LARGE_PALETTED_TEXELS_OFFSET +
                    (stripIndex * IMPACT_LINES_SIZE * IMPACT_LINES_STRIP_HEIGHT);
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress(f_OpeningEffectImage + texelOffset),
                    G_IM_FMT_I,
                    G_IM_SIZ_8b,
                    IMPACT_LINES_SIZE,
                    IMPACT_LINES_STRIP_HEIGHT,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                gDPPipeSync(gfx++);
                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels(left),
                    QuarterPixels(top),
                    QuarterPixels(left + IMPACT_LINES_SIZE),
                    QuarterPixels(bottom),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                gDPPipeSync(gfx++);

                row--;
            }

            *displayListHead = gfx;
        }
    }
}

#define ASSET_LAKITU_IMAGE 0x64B

void LoadLakitu(void) {
    EncodedAsset asset;
    s32 pass;
    s32 segmentIndex;

    OpenEncodedAsset(ASSET_LAKITU_IMAGE, &asset);
    ReadEncodedAsset(f_LakituImage, &asset);
    OpenEncodedAsset(ASSET_STREAK_IMAGE, &asset);
    ReadEncodedAsset(f_LakituTrailImage, &asset);

    for (pass = 0; pass < 2; pass++) {
        for (segmentIndex = 0; segmentIndex < LAKITU_TRAIL_SEGMENT_COUNT; segmentIndex++) {
            guTranslate(
                &f_LakituTrailMatrices[segmentIndex],
                f_LakituScreenPoint[AXIS_X] - (SCREEN_WIDTH / 2),
                (SCREEN_HEIGHT / 2) - f_LakituScreenPoint[AXIS_Y],
                -LAKITU_TRAIL_DEPTH
            );
        }
    }

    f_LakituTrailStart[AXIS_X] = f_LakituScreenPoint[AXIS_X] - SCREEN_WIDTH;
    f_LakituTrailStart[AXIS_Y] = (SCREEN_HEIGHT / 2) - f_LakituScreenPoint[AXIS_Y];
}

#define LAKITU_IMAGE_SIZE 32
#define LAKITU_DRAWN_SIZE 48
#define LAKITU_SCALE TexelRunStep(LAKITU_IMAGE_SIZE, LAKITU_DRAWN_SIZE)
#define LAKITU_TRAIL_BATCH_SEGMENT_COUNT 16
#define LAKITU_TRAIL_DRAWN_SEGMENT_COUNT 31
#define LAKITU_TRAIL_ROWS_PER_SEGMENT 8
#define LAKITU_TRAIL_RIGHT_S (TexelCoordinate(STREAK_IMAGE_WIDTH) - 1)
#define LakituTrailT(segment) TexelCoordinate((segment) * LAKITU_TRAIL_ROWS_PER_SEGMENT)
#define LakituTrailSecondBatchSlot(segment) \
    (((LAKITU_TRAIL_BATCH_SEGMENT_COUNT - 2) * 2) - \
        (((segment) - LAKITU_TRAIL_BATCH_SEGMENT_COUNT) * 2))
#define LAKITU_TRAIL_FADE_STEP 4

void DrawLakitu(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    Vec3 translation;
    Vec3 rotation;
    Mat4 matrix;
    s32 segmentIndex;
    s32 drawnIndex;

    if (f_IsLakituFlying) {
        gDPPipeSync(gfx++);
        AppendSyncedRdpState(gfx, gDPSetCycleType, G_CYC_1CYCLE);
        AppendSyncedRdpState(gfx, gDPSetRenderMode, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPPipeSync(gfx++);
        gDPSetColorDither(gfx++, G_CD_DISABLE);
        gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
        AppendSyncedRdpState(gfx, gDPSetTexturePersp, G_TP_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_LakituImage)));
        gDPPipeSync(gfx++);
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(LargeImageTexels(f_LakituImage)),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            LAKITU_IMAGE_SIZE,
            LAKITU_IMAGE_SIZE,
            0,
            G_TX_MIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            TextureMask(LAKITU_IMAGE_SIZE),
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(f_LakituScreenPoint[AXIS_X] - (LAKITU_DRAWN_SIZE / 2)),
            QuarterPixels(f_LakituScreenPoint[AXIS_Y] - (LAKITU_DRAWN_SIZE / 2)),
            QuarterPixels(f_LakituScreenPoint[AXIS_X] + (LAKITU_DRAWN_SIZE / 2)),
            QuarterPixels(f_LakituScreenPoint[AXIS_Y] + (LAKITU_DRAWN_SIZE / 2)),
            G_TX_RENDERTILE,
            TexelCoordinate(LAKITU_IMAGE_SIZE),
            0,
            LAKITU_SCALE,
            LAKITU_SCALE
        );
        gDPPipeSync(gfx++);
        rotation[AXIS_X] = 0.0f;
        rotation[AXIS_Y] = 0.0f;
        rotation[AXIS_Z] = atan2(
            f_LakituTrailStart[AXIS_X] - (f32)(f_LakituScreenPoint[AXIS_X] - (SCREEN_WIDTH / 2)),
            f_LakituTrailStart[AXIS_Y] - (f32)((SCREEN_HEIGHT / 2) - f_LakituScreenPoint[AXIS_Y])
        );
        translation[AXIS_X] = f_LakituScreenPoint[AXIS_X] - (SCREEN_WIDTH / 2);
        translation[AXIS_Y] = (SCREEN_HEIGHT / 2) - f_LakituScreenPoint[AXIS_Y];
        translation[AXIS_Z] = -LAKITU_TRAIL_DEPTH;
        BuildRotationMatrix(matrix, rotation, translation);
        ConvertMatrixToFixed(matrix, &f_LakituTrailMatrices[f_LakituTrailNewestIndex]);
        f_LakituTrailStart[AXIS_X] = f_LakituScreenPoint[AXIS_X] - (SCREEN_WIDTH / 2);
        f_LakituTrailStart[AXIS_Y] = (SCREEN_HEIGHT / 2) - f_LakituScreenPoint[AXIS_Y];
        gSPLoadGeometryMode(gfx++, 0);
        gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
        AppendSyncedRdpState(gfx, gDPSetRenderMode, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        AppendSyncedRdpState(gfx, gDPSetTextureLUT, G_TT_NONE);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_PERSP);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, f_LakituTrailAlpha);
        gDPSetEnvColor(gfx++, 255, 255, 255, OPAQUE_ALPHA);
        gDPSetCombineLERP(
            gfx++,
            0,
            0,
            0,
            PRIMITIVE,
            PRIMITIVE,
            0,
            TEXEL0,
            0,
            0,
            0,
            0,
            PRIMITIVE,
            PRIMITIVE,
            0,
            TEXEL0,
            0
        );
        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(ImageBody(f_LakituTrailImage)),
            G_IM_FMT_I,
            STREAK_IMAGE_WIDTH,
            STREAK_IMAGE_HEIGHT,
            0,
            G_TX_MIRROR | G_TX_WRAP,
            G_TX_MIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            TextureMask(STREAK_IMAGE_HEIGHT),
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&g_ScreenOrthoMatrix),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
        );
        gDPPipeSync(gfx++);
        segmentIndex = f_LakituTrailNewestIndex;
        for (drawnIndex = 0; drawnIndex != LAKITU_TRAIL_BATCH_SEGMENT_COUNT; drawnIndex++) {
            gSPMatrix(
                gfx++,
                PhysicalAddress(&f_LakituTrailMatrices[segmentIndex]),
                G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
            );
            gSPVertex(gfx++, f_LakituTrailVertices, 2, drawnIndex * 2);
            gSPModifyVertex(gfx++, drawnIndex * 2, G_MWO_POINT_ST, LakituTrailT(drawnIndex));
            gSPModifyVertex(
                gfx++,
                (drawnIndex * 2) + 1,
                G_MWO_POINT_ST,
                TextureCoordinateWord(LAKITU_TRAIL_RIGHT_S, LakituTrailT(drawnIndex))
            );
            segmentIndex--;
            if (segmentIndex < 0) {
                segmentIndex = LAKITU_TRAIL_SEGMENT_COUNT - 1;
            }
        }

        for (drawnIndex = 0; drawnIndex != LAKITU_TRAIL_BATCH_SEGMENT_COUNT - 1; drawnIndex++) {
            gSP2Triangles(
                gfx++,
                (drawnIndex * 2) + 1,
                drawnIndex * 2,
                (drawnIndex * 2) + 2,
                0,
                (drawnIndex * 2) + 1,
                (drawnIndex * 2) + 2,
                (drawnIndex * 2) + 3,
                0
            );
        }

        for (drawnIndex = LAKITU_TRAIL_BATCH_SEGMENT_COUNT;
            drawnIndex != LAKITU_TRAIL_DRAWN_SEGMENT_COUNT;
            drawnIndex++) {
            gSPMatrix(
                gfx++,
                PhysicalAddress(&f_LakituTrailMatrices[segmentIndex]),
                G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
            );
            gSPVertex(gfx++, f_LakituTrailVertices, 2, LakituTrailSecondBatchSlot(drawnIndex));
            gSPModifyVertex(
                gfx++,
                LakituTrailSecondBatchSlot(drawnIndex),
                G_MWO_POINT_ST,
                LakituTrailT(drawnIndex)
            );
            gSPModifyVertex(
                gfx++,
                LakituTrailSecondBatchSlot(drawnIndex) + 1,
                G_MWO_POINT_ST,
                TextureCoordinateWord(LAKITU_TRAIL_RIGHT_S, LakituTrailT(drawnIndex))
            );
            segmentIndex--;
            if (segmentIndex < 0) {
                segmentIndex = LAKITU_TRAIL_SEGMENT_COUNT - 1;
            }
        }

        for (drawnIndex = 0; drawnIndex != LAKITU_TRAIL_BATCH_SEGMENT_COUNT - 1; drawnIndex++) {
            gSP2Triangles(
                gfx++,
                (drawnIndex * 2) + 1,
                drawnIndex * 2,
                (drawnIndex * 2) + 2,
                0,
                (drawnIndex * 2) + 1,
                (drawnIndex * 2) + 2,
                (drawnIndex * 2) + 3,
                0
            );
        }

        gDPPipeSync(gfx++);
        if (++f_LakituTrailNewestIndex >= LAKITU_TRAIL_SEGMENT_COUNT) {
            f_LakituTrailNewestIndex = 0;
        }

        f_LakituTrailAlpha -= LAKITU_TRAIL_FADE_STEP;
        if (f_LakituTrailAlpha < 0) {
            f_LakituTrailAlpha = 0;
        }

        *displayListHead = gfx;
    }
}

#define LAKITU_TURN_STEP 12
#define LAKITU_LAST_ANGLE 540
#define LAKITU_DRIFT_X 8
#define LAKITU_DRIFT_Y 2
#define LAKITU_LOOP_RADIUS_X 20.0
#define LAKITU_LOOP_RADIUS_Y 16.0

void MoveLakitu(void) {
    Radians angle;

    if (f_IsLakituFlying) {
        angle = DegreesToRadians(f_LakituAngle);
        f_LakituAngle += LAKITU_TURN_STEP;
        if (f_LakituAngle > LAKITU_LAST_ANGLE) {
            f_IsLakituFlying = FALSE;
        }

        f_LakituScreenPoint[AXIS_X] += LAKITU_DRIFT_X + (s32)(cos(angle) * LAKITU_LOOP_RADIUS_X);
        f_LakituScreenPoint[AXIS_Y] -= LAKITU_DRIFT_Y + (s32)(sin(angle) * LAKITU_LOOP_RADIUS_Y);
    }
}

#define ASSET_BURST_IMAGE 0x93D

void LoadOpeningBurst(void) {
    EncodedAsset asset;

    OpenEncodedAsset(ASSET_BURST_IMAGE, &asset);
    ReadEncodedAsset(f_OpeningEffectImage, &asset);
}

#define BURST_LEFT 227
#define BURST_TOP 55
#define BURST_RIGHT 255
#define BURST_BOTTOM 81
#define BURST_IMAGE_SIZE 32
#define BURST_FRAME_COUNT 4

void DrawOpeningBurst(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 left = BURST_LEFT;
    s32 top = BURST_TOP;
    s32 right = BURST_RIGHT;
    s32 bottom = BURST_BOTTOM;
    s32 width;
    s32 height;

    if (f_IsBurstShown && ++f_BurstFrame < BURST_FRAME_COUNT) {
        gDPPipeSync(gfx++);
        AppendSyncedRdpState(gfx, gDPSetCycleType, G_CYC_1CYCLE);
        AppendSyncedRdpState(
            gfx,
            gDPSetRenderMode,
            OPAQUE_ALPHA_COVERAGE_RENDER_MODE,
            OPAQUE_ALPHA_COVERAGE_RENDER_MODE2
        );
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        AppendSyncedRdpState(gfx, gDPSetColorDither, G_CD_DISABLE);
        gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
        AppendSyncedRdpState(gfx, gDPSetTexturePersp, G_TP_NONE);
        AppendSyncedRdpState(gfx, gDPSetTextureFilter, G_TF_BILERP);
        AppendSyncedRdpState(gfx, gDPSetAlphaCompare, G_AC_NONE);
        AppendSyncedRdpState(gfx, gDPSetTextureLUT, G_TT_RGBA16);
        gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_OpeningEffectImage)));
        gDPPipeSync(gfx++);
        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(ImageTexels(f_OpeningEffectImage)),
            G_IM_FMT_CI,
            BURST_IMAGE_SIZE,
            BURST_IMAGE_SIZE,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        width = right - left;
        height = bottom - top;
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(left),
            QuarterPixels(top),
            QuarterPixels(right),
            QuarterPixels(bottom),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(BURST_IMAGE_SIZE, width),
            TexelRunStep(BURST_IMAGE_SIZE, height)
        );
        gDPPipeSync(gfx++);
        *displayListHead = gfx;
    }
}

void LoadOpeningShotBall(void) {
    EncodedAsset asset;

    OpenEncodedAsset(ASSET_SHOT_BALL_IMAGE, &asset);
    ReadEncodedAsset(f_ShotBallImage, &asset);
}

#define SHOT_BALL_IMAGE_SIZE 64
#define SHOT_BALL_STRIP_HEIGHT 32
#define SHOT_BALL_STRIP_COUNT 2
#define SHOT_BALL_STRIP_ROWS_FIXED 0x7FA0
#define SHOT_BALL_HALF_WIDTH_PER_SCALE 2
#define ShotBallHalfWidth() (f_ShotBallScale * SHOT_BALL_HALF_WIDTH_PER_SCALE)
#define SHOT_BALL_LIFT 0.1f
#define SHOT_BALL_FAR_DISTANCE 400.0f
#define SHOT_BALL_DOT_SIZE 3
#define SHOT_BALL_FADE_RATE 0.04
#define SHOT_BALL_SHADOW_SCALE 0.5

void DrawOpeningShotBall(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    ScreenPoint ballPoint;
    s32 stripIndex;
    s32 size;
    s32 left;
    s32 top;
    s32 right;
    s32 bottom;
    s32 stripHeight;
    Gfx *renderModeCommand;

    if (f_ShotBallScale != 0) {
        f_ShotBallPosition[AXIS_X] =
            f_OpeningShotSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE;
        f_ShotBallPosition[AXIS_Z] =
            f_OpeningShotSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        f_ShotBallPosition[AXIS_Y] =
            (f_OpeningShotSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE) + SHOT_BALL_LIFT;
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gSPLoadGeometryMode(gfx++, 0);
        gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
        gDPPipeSync(gfx++);
        if (ProjectPoint(
                f_ShotBallPosition[AXIS_X],
                f_ShotBallPosition[AXIS_Y],
                f_ShotBallPosition[AXIS_Z],
                &ballPoint
            ) < 0) {
            gDPPipeSync(gfx++);
            gDPSetDepthSource(gfx++, G_ZS_PRIM);
            gDPSetPrimDepth(gfx++, ballPoint.depth, 0);
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            f_ShotBallPosition[AXIS_Y] = GetGroundHeight(
                                             f_ShotBallPosition[AXIS_X] * WORLD_TO_COURSE_SCALE,
                                             f_ShotBallPosition[AXIS_Z] * WORLD_TO_COURSE_SCALE
                                         ) *
                COURSE_TO_WORLD_SCALE;
            guTranslate(
                &f_ShotBallShadowTranslation,
                f_ShotBallPosition[AXIS_X],
                f_ShotBallPosition[AXIS_Y],
                f_ShotBallPosition[AXIS_Z]
            );
            guScale(
                &f_ShotBallShadowScaling,
                f_ShotBallScale * SHOT_BALL_SHADOW_SCALE,
                f_ShotBallScale * (f32)SHOT_BALL_SHADOW_SCALE,
                f_ShotBallScale * (f32)SHOT_BALL_SHADOW_SCALE
            );
            gDPPipeSync(gfx++);
            renderModeCommand = gfx++;
            gDPSetRenderMode(renderModeCommand, G_RM_CLD_SURF, G_RM_CLD_SURF2);
            gDPSetCombineLERP(
                gfx++,
                0,
                0,
                0,
                PRIMITIVE,
                TEXEL0,
                0,
                PRIMITIVE,
                0,
                0,
                0,
                0,
                PRIMITIVE,
                TEXEL0,
                0,
                PRIMITIVE,
                0
            );
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
            gDPSetBlendColor(gfx++, 255, 255, 255, 16);
            gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 127);
            AppendSyncedRdpState(gfx, gDPSetAlphaDither, G_AD_PATTERN);
            AppendSyncedRdpState(gfx, gDPSetColorDither, G_CD_MAGICSQ);
            AppendSyncedRdpState(gfx, gDPSetTexturePersp, G_TP_PERSP);
            AppendSyncedRdpState(gfx, gDPSetTextureLUT, G_TT_NONE);
            gDPPipeSync(gfx++);
            gDPSetTextureLOD(gfx++, G_TL_TILE);
            gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
            gDPPipeSync(gfx++);
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(ImageBody(g_ShadowTexture)),
                G_IM_FMT_I,
                G_IM_SIZ_8b,
                SHADOW_TEXTURE_SIZE,
                SHADOW_TEXTURE_SIZE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gSPMatrix(
                gfx++,
                PhysicalAddress(&f_ShotBallShadowTranslation),
                G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
            );
            gSPMatrix(
                gfx++,
                PhysicalAddress(&f_ShotBallShadowScaling),
                G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
            );
            gSPVertex(gfx++, f_ShotBallShadowVertices, QUAD_VERTEX_COUNT, 0);
            gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
            gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            AppendSyncedRdpState(gfx, gDPSetRenderMode, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
            gDPSetCombineLERP(
                gfx++,
                0,
                0,
                0,
                TEXEL0,
                PRIMITIVE,
                0,
                TEXEL0,
                0,
                0,
                0,
                0,
                TEXEL0,
                PRIMITIVE,
                0,
                TEXEL0,
                0
            );
            gDPPipeSync(gfx++);
            gDPSetColorDither(gfx++, G_CD_DISABLE);
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
            gDPSetBlendColor(gfx++, 255, 255, 255, 2);
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, f_ShotBallAlpha);
            gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
            gDPPipeSync(gfx++);
            gDPSetTexturePersp(gfx++, G_TP_NONE);
            AppendSyncedRdpState(gfx, gDPSetTextureFilter, G_TF_BILERP);
            AppendSyncedRdpState(gfx, gDPSetAlphaCompare, G_AC_NONE);
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_RGBA16);
            gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_ShotBallImage)));
            gDPPipeSync(gfx++);
            for (stripIndex = 0; stripIndex != SHOT_BALL_STRIP_COUNT; stripIndex++) {
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress(
                        LargeImageTexels(f_ShotBallImage) +
                        (stripIndex * SHOT_BALL_IMAGE_SIZE * SHOT_BALL_STRIP_HEIGHT)
                    ),
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    SHOT_BALL_IMAGE_SIZE,
                    SHOT_BALL_STRIP_HEIGHT,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                if (g_CameraEditor.distance > SHOT_BALL_FAR_DISTANCE) {
                    size = ShotBallHalfWidth();
                    left = ballPoint.x - size;
                    right = left + SHOT_BALL_DOT_SIZE;
                    top = ballPoint.y - ((SHOT_BALL_STRIP_COUNT - stripIndex) * size);
                    bottom = top + SHOT_BALL_DOT_SIZE;
                    f_ShotBallAlpha =
                        OPAQUE_ALPHA - ((u32)g_CameraEditor.distance * SHOT_BALL_FADE_RATE);
                    if (f_ShotBallAlpha < 0) {
                        f_ShotBallAlpha = 0;
                    }
                }
                else {
                    f_ShotBallAlpha = OPAQUE_ALPHA;
                    left = ballPoint.x - ShotBallHalfWidth();
                    right = ballPoint.x + ShotBallHalfWidth();
                    stripHeight = ShotBallHalfWidth();
                    top = ballPoint.y - ((SHOT_BALL_STRIP_COUNT - stripIndex) * stripHeight);
                    bottom = ballPoint.y + (stripIndex * stripHeight) - stripHeight;
                }

                gSPTextureRectangle(
                    gfx++,
                    QuarterPixels(left),
                    QuarterPixels(top),
                    QuarterPixels(right),
                    QuarterPixels(bottom),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TexelRunStep(SHOT_BALL_IMAGE_SIZE, right - left),
                    SHOT_BALL_STRIP_ROWS_FIXED / (bottom - top)
                );
                gDPPipeSync(gfx++);
            }

            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            gDPSetDepthSource(gfx++, G_ZS_PIXEL);
            gDPPipeSync(gfx++);
        }

        *displayListHead = gfx;
    }
}

void EaseOpeningLookAt(void) {
    switch (f_LookAtEaseMode) {
        case LOOK_AT_EASE_FAST:
            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X] +=
                ((f_OpeningShotSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE) -
                    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X]) *
                0.1f;
            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y] +=
                ((f_OpeningShotSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE) -
                    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y]) *
                0.1f;
            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z] +=
                ((f_OpeningShotSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE) -
                    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z]) *
                0.1f;
            MeasureEditorCamera();
            break;
        case LOOK_AT_EASE_SLOW:
            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X] +=
                ((f_OpeningShotSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE) -
                    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X]) *
                0.01f;
            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y] +=
                ((f_OpeningShotSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE) -
                    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y]) *
                0.01f;
            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z] +=
                ((f_OpeningShotSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE) -
                    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z]) *
                0.01f;
            MeasureEditorCamera();
            break;
        case LOOK_AT_EASE_SPEEDING_UP:
            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X] +=
                ((f_OpeningBallPositions[1][AXIS_X] + 38.1f) -
                    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X]) *
                f_LookAtEaseRate;
            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y] +=
                ((f_OpeningBallPositions[1][AXIS_Y] - 4.5f) -
                    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y]) *
                f_LookAtEaseRate;
            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z] +=
                ((f_OpeningBallPositions[1][AXIS_Z] - 4.795f) -
                    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z]) *
                f_LookAtEaseRate;
            MeasureEditorCamera();
            f_LookAtEaseRate += 0.002f;
            if (f_LookAtEaseRate > 0.1f) {
                f_LookAtEaseRate = 0.1f;
            }

            break;
    }
}

void LoadOpeningLogo(void) {
    EncodedAsset asset;
    LogoLetterIndex letterIndex;

    OpenEncodedAsset(ASSET_LOGO_LETTER_IMAGES, &asset);
    ReadEncodedAsset(f_LogoLetterImages, &asset);
    OpenEncodedAsset(ASSET_LOGO_CLUB_IMAGE, &asset);
    ReadEncodedAsset(f_LogoClubImage, &asset);
    for (letterIndex = 0; letterIndex < LOGO_LETTER_COUNT; letterIndex++) {
        f_LogoLetterStates[letterIndex] = LOGO_LETTER_WAITING;
        f_LogoLetterSpeeds[letterIndex] = -14.0f;
        f_LogoLetters[letterIndex].x = LOGO_LETTER_HIDDEN;
        f_LogoLetters[letterIndex].y = LOGO_LETTER_HIDDEN;
        f_LogoLetterScales[letterIndex][AXIS_X] = 0.1f;
        f_LogoLetterScales[letterIndex][AXIS_Y] = 0.1f;
        f_LogoLetterSquashFrames[letterIndex] = 0;
    }

    f_LogoLetters[LOGO_LETTER_M].depth = LOGO_M_DEPTH;
    f_LogoLetters[LOGO_LETTER_A].depth = LOGO_A_DEPTH;
    f_LogoLetters[LOGO_LETTER_R].depth = LOGO_R_DEPTH;
    f_LogoLetters[LOGO_LETTER_I].depth = LOGO_I_DEPTH;
    f_LogoLetters[LOGO_LETTER_FIRST_O].depth = LOGO_FIRST_O_DEPTH;
    f_LogoLetters[LOGO_LETTER_G].depth = LOGO_G_DEPTH;
    f_LogoLetters[LOGO_LETTER_SECOND_O].depth = LOGO_SECOND_O_DEPTH;
    f_LogoLetters[LOGO_LETTER_L].depth = LOGO_L_DEPTH;
    f_LogoLetters[LOGO_LETTER_F].depth = LOGO_F_DEPTH;
    OpenEncodedAsset(ASSET_LOGO_MARIO_IMAGE, &asset);
    ReadEncodedAsset(f_LogoMarioImage, &asset);
    OpenEncodedAsset(ASSET_LOGO_BALL_IMAGES, &asset);
    ReadEncodedAsset(f_LogoBallImages, &asset);
    OpenEncodedAsset(ASSET_SHINE_IMAGE, &asset);
    ReadEncodedAsset(f_LogoShineImage, &asset);
}

#define LOGO_FADE_GRAY_LEVEL 128
#define LOGO_FADE_WHITENING_FRAMES 24
#define LOGO_FADE_WHITENING_STEP 5.3333f
#define LOGO_FADE_WHITE_LEVEL 0xFF
#define LOGO_FADE_WHITE_DURATION 2
#define LOGO_FADE_OUT_STEP 24
#define LOGO_GRAYING_DURATION 64

#define LOGO_LETTER_GROUND_Y 125
#define LOGO_LETTER_L_GROUND_Y 122
#define LOGO_LETTER_BOUNCINESS 0.2f
#define LOGO_LETTER_BOUNCE_SPEED_MIN 1.0f
#define LOGO_LETTER_GRAVITY 0.5f
#define LOGO_LETTER_GROWTH 0.04f
#define LOGO_LETTER_SQUASH_FRAMES 10
#define LOGO_LETTER_SQUASH_STEP 0.03f
#define LOGO_LETTER_SQUASHED_FRAMES 2
#define LOGO_LETTER_STRETCH_FRAMES 5
#define LOGO_LETTER_STRETCH_STEP 0.06f

#define LOGO_CLUB_ORBIT_CENTER_X 165
#define LOGO_CLUB_ORBIT_CENTER_Y 141
#define LOGO_CLUB_FIRST_WOBBLE 40.0f
#define LOGO_CLUB_WOBBLE 100.0f
#define LOGO_CLUB_ORBIT_RADIUS_STEP 4.0f
#define LOGO_CLUB_ORBIT_ANGLE_STEP 5.5f
#define LOGO_CLUB_SPIN_STEP 23.0f
#define LOGO_CLUB_UNUSED_ANGLE_STEP 3.0f
#define LOGO_CLUB_WOBBLE_ANGLE_STEP 6.0f

#define LOGO_SHINE_ORIGIN_TEXEL 64
#define LOGO_SHINE_ORIGIN_FRAME 16
#define LOGO_SHINE_SPEED 18
#define LOGO_SHINE_TEXEL_STEP 0x28F
#define LOGO_SHINE_LEFT 10
#define LOGO_SHINE_TOP 60
#define LOGO_SHINE_RIGHT 630
#define LOGO_SHINE_BOTTOM 120
#define LOGO_SHINE_MASK_LEFT 8
#define LOGO_SHINE_MASK_RIGHT 312
#define LOGO_SHINE_MASK_DEPTH 4
#define LOGO_SHINE_LETTER_DEPTH 40
#define LOGO_SHINE_DEPTH 36

#define LOGO_LETTER_HALF_COUNT 2
#define LOGO_LETTER_WIDTH 48
#define LOGO_LETTER_HALF_HEIGHT 24
#define LOGO_LETTER_DEPTH_OFFSET 64
#define LOGO_CLUB_WIDTH 152
#define LOGO_CLUB_STRIP_HEIGHT 13
#define LOGO_CLUB_DEPTH 92
#define LOGO_ALPHA_THRESHOLD 0x50
#define LOGO_FADING_ALPHA_THRESHOLD 10

#define LOGO_SHRINK_FRAMES 10
#define LOGO_SHRUNK_Y 70
#define LOGO_SHRUNK_L_Y 68
#define LOGO_SHRUNK_SCALE 0.72f
#define LOGO_SHRUNK_CLUB_X 160
#define LOGO_SHRUNK_CLUB_Y 188

#define LOGO_Z (-300.0f)

void DrawOpeningLogo(Gfx **displayListHead, Boolean isDrawn) {
    Gfx *gfx = *displayListHead;
    s32 texelsOffset;
    s32 letterTexelsOffset;
    LogoLetterIndex logoLetter;
    LogoLetterIndex letterIndex;
    s32 j;
    Radians angle;
    Radians orbitAngle;
    f32 wobble;

    switch (f_LogoFadeStage) {
        case LOGO_FADE_GRAYING:
            g_ScreenFadeFramesLeft = f_LogoFadeFramesLeft;
            f_LogoFadeFramesLeft--;
            if (f_LogoFadeFramesLeft == LOGO_FADE_WHITENING_FRAMES) {
                f_LogoFadeStage++;
            }

            break;
        case LOGO_FADE_GRAY:
            g_ScreenFadeFramesLeft = f_LogoFadeFramesLeft;
            if (f_LogoShineFrame > LOGO_SHINE_FIRST_FRAME) {
                f_LogoFadeStage++;
            }

            break;
        case LOGO_FADE_WHITENING:
            g_ScreenFadeFramesLeft = f_LogoFadeFramesLeft;
            g_ScreenFadeColor.red = (s32)((LOGO_FADE_WHITENING_FRAMES - f_LogoFadeFramesLeft) *
                                        LOGO_FADE_WHITENING_STEP) +
                LOGO_FADE_GRAY_LEVEL;
            g_ScreenFadeColor.green = g_ScreenFadeColor.red;
            g_ScreenFadeColor.blue = g_ScreenFadeColor.green;
            f_LogoFadeFramesLeft--;
            if (f_LogoFadeFramesLeft == 0) {
                f_LogoFadeStage++;
            }

            break;
        case LOGO_FADE_WHITE:
            g_ScreenFadeDuration = LOGO_FADE_WHITE_DURATION;
            g_ScreenFadeColor.red = LOGO_FADE_WHITE_LEVEL;
            g_ScreenFadeColor.green = g_ScreenFadeColor.red;
            g_ScreenFadeColor.blue = g_ScreenFadeColor.green;
            g_ScreenFadeKind = FADE_OUT;
            g_ScreenFadeFramesLeft = 0;
            if (f_LogoState == LOGO_SHRINKING) {
                f_LogoFadeStage++;
            }

            break;
        case LOGO_FADE_LOGO_OUT:
            g_ScreenFadeDuration = LOGO_FADE_WHITE_DURATION;
            g_ScreenFadeKind = FADE_OUT;
            g_ScreenFadeFramesLeft = 0;
            g_ScreenFadeColor.red = LOGO_FADE_WHITE_LEVEL;
            g_ScreenFadeColor.green = g_ScreenFadeColor.red;
            g_ScreenFadeColor.blue = g_ScreenFadeColor.green;
            f_LogoLettersAlpha -= LOGO_FADE_OUT_STEP;
            f_LogoClubAlpha -= LOGO_FADE_OUT_STEP;
            if (f_LogoLettersAlpha < 0) {
                f_LogoLettersAlpha = 0;
                f_LogoClubAlpha = 0;
                f_OpeningScene = OPENING_SCENE_END;
            }

            break;
    }

    if (f_LogoLetterStates[LOGO_LETTER_M] == LOGO_LETTER_WAITING &&
        f_LakituScreenPoint[AXIS_X] > LOGO_M_DROP_X) {
        f_LogoLetters[LOGO_LETTER_M].x = LOGO_M_DROP_X;
        f_LogoLetters[LOGO_LETTER_M].y = f_LakituScreenPoint[AXIS_Y];
        f_LogoLetterStates[LOGO_LETTER_M] = LOGO_LETTER_FALLING;
        g_ScreenFadeDuration = LOGO_GRAYING_DURATION;
        g_ScreenFadeFramesLeft = g_ScreenFadeDuration;
        g_ScreenFadeKind = FADE_OUT;
        g_ScreenFadeColor.red = LOGO_FADE_GRAY_LEVEL;
        g_ScreenFadeColor.green = g_ScreenFadeColor.red;
        g_ScreenFadeColor.blue = g_ScreenFadeColor.green;
        f_LogoFadeStage = LOGO_FADE_GRAYING;
        f_HasLogoStarted = TRUE;
    }

    if (f_LogoLetterStates[LOGO_LETTER_A] == LOGO_LETTER_WAITING &&
        f_LakituScreenPoint[AXIS_X] > LOGO_A_DROP_X) {
        f_LogoLetters[LOGO_LETTER_A].x = LOGO_A_DROP_X;
        f_LogoLetters[LOGO_LETTER_A].y = f_LakituScreenPoint[AXIS_Y];
        f_LogoLetterStates[LOGO_LETTER_A] = LOGO_LETTER_FALLING;
    }

    if (f_LogoLetterStates[LOGO_LETTER_R] == LOGO_LETTER_WAITING &&
        f_LakituScreenPoint[AXIS_X] > LOGO_R_DROP_X) {
        f_LogoLetters[LOGO_LETTER_R].x = LOGO_R_DROP_X;
        f_LogoLetters[LOGO_LETTER_R].y = f_LakituScreenPoint[AXIS_Y];
        f_LogoLetterStates[LOGO_LETTER_R] = LOGO_LETTER_FALLING;
    }

    if (f_LogoLetterStates[LOGO_LETTER_I] == LOGO_LETTER_WAITING &&
        f_LakituScreenPoint[AXIS_X] > LOGO_I_DROP_X) {
        f_LogoLetters[LOGO_LETTER_I].x = LOGO_I_DROP_X;
        f_LogoLetters[LOGO_LETTER_I].y = f_LakituScreenPoint[AXIS_Y];
        f_LogoLetterStates[LOGO_LETTER_I] = LOGO_LETTER_FALLING;
    }

    if (f_LogoLetterStates[LOGO_LETTER_FIRST_O] == LOGO_LETTER_WAITING &&
        f_LakituScreenPoint[AXIS_X] > LOGO_FIRST_O_DROP_X) {
        f_LogoLetters[LOGO_LETTER_FIRST_O].x = LOGO_FIRST_O_DROP_X;
        f_LogoLetters[LOGO_LETTER_FIRST_O].y = f_LakituScreenPoint[AXIS_Y];
        f_LogoLetterStates[LOGO_LETTER_FIRST_O] = LOGO_LETTER_FALLING;
    }

    if (f_LogoLetterStates[LOGO_LETTER_G] == LOGO_LETTER_WAITING &&
        f_LakituScreenPoint[AXIS_X] > LOGO_G_DROP_X) {
        f_LogoLetters[LOGO_LETTER_G].x = LOGO_G_DROP_X;
        f_LogoLetters[LOGO_LETTER_G].y = f_LakituScreenPoint[AXIS_Y];
        f_LogoLetterStates[LOGO_LETTER_G] = LOGO_LETTER_FALLING;
    }

    if (f_LogoLetterStates[LOGO_LETTER_SECOND_O] == LOGO_LETTER_WAITING &&
        f_LakituScreenPoint[AXIS_X] > LOGO_SECOND_O_DROP_X) {
        f_LogoLetters[LOGO_LETTER_SECOND_O].x = LOGO_SECOND_O_DROP_X;
        f_LogoLetters[LOGO_LETTER_SECOND_O].y = f_LakituScreenPoint[AXIS_Y];
        f_LogoLetterStates[LOGO_LETTER_SECOND_O] = LOGO_LETTER_FALLING;
    }

    if (f_LogoLetterStates[LOGO_LETTER_L] == LOGO_LETTER_WAITING &&
        f_LakituScreenPoint[AXIS_X] > LOGO_L_DROP_X) {
        f_LogoLetters[LOGO_LETTER_L].x = LOGO_L_DROP_X;
        f_LogoLetters[LOGO_LETTER_L].y = f_LakituScreenPoint[AXIS_Y];
        f_LogoLetterStates[LOGO_LETTER_L] = LOGO_LETTER_FALLING;
    }

    if (f_LogoLetterStates[LOGO_LETTER_F] == LOGO_LETTER_WAITING &&
        f_LakituScreenPoint[AXIS_X] > LOGO_F_DROP_X) {
        f_LogoLetters[LOGO_LETTER_F].x = LOGO_F_DROP_X;
        f_LogoLetters[LOGO_LETTER_F].y = f_LakituScreenPoint[AXIS_Y];
        f_LogoLetterStates[LOGO_LETTER_F] = LOGO_LETTER_FALLING;
        f_LogoState = LOGO_CLUB_STARTING;
    }

    if (isDrawn == TRUE) {
        for (logoLetter = 0; logoLetter < LOGO_LETTER_COUNT; logoLetter++) {
            guTranslate(
                &f_LogoLetterTranslations[logoLetter][g_DrawBufferIndex],
                f_LogoLetters[logoLetter].x - (SCREEN_WIDTH / 2.0f),
                (SCREEN_HEIGHT - f_LogoLetters[logoLetter].y) - (SCREEN_HEIGHT / 2.0f),
                f_LogoLetters[logoLetter].depth + LOGO_Z
            );
            guScale(
                &f_LogoLetterScalings[logoLetter][g_DrawBufferIndex],
                f_LogoLetterScales[logoLetter][AXIS_X],
                f_LogoLetterScales[logoLetter][AXIS_Y],
                1.0f
            );
        }

        guTranslate(
            &f_LogoClubTranslations[g_DrawBufferIndex],
            f_LogoClubPosition[AXIS_X] - (SCREEN_WIDTH / 2.0f),
            f_LogoClubPosition[AXIS_Y] - (SCREEN_HEIGHT / 2.0f),
            f_LogoClubPosition[AXIS_Z] + LOGO_Z
        );
        guRotate(&f_LogoClubRotations[g_DrawBufferIndex], f_LogoClubAngle, 0.0f, 0.0f, 1.0f);
        guScale(
            &f_LogoClubScalings[g_DrawBufferIndex],
            f_LogoClubScale[AXIS_X],
            f_LogoClubScale[AXIS_Y],
            1.0f
        );
        gDPPipeSync(gfx++);
        AppendSyncedRdpState(gfx, gDPSetCycleType, G_CYC_1CYCLE);
        gSPLoadGeometryMode(gfx++, 0);
        gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
        gDPPipeSync(gfx++);
        gDPSetDepthSource(gfx++, G_ZS_PRIM);
        gDPSetCombineLERP(
            gfx++,
            0,
            0,
            0,
            TEXEL0,
            PRIMITIVE,
            0,
            TEXEL0,
            0,
            0,
            0,
            0,
            TEXEL0,
            PRIMITIVE,
            0,
            TEXEL0,
            0
        );
        AppendSyncedRdpState(gfx, gDPSetAlphaCompare, G_AC_THRESHOLD);
        if (f_LogoFadeStage == LOGO_FADE_LOGO_OUT) {
            gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, LOGO_FADING_ALPHA_THRESHOLD);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_AA_ZB_XLU_SURF | Z_UPD, G_RM_AA_ZB_XLU_SURF2 | Z_UPD);
        }
        else {
            gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, LOGO_ALPHA_THRESHOLD);
            AppendSyncedRdpState(gfx, gDPSetRenderMode, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
        }

        AppendSyncedRdpState(gfx, gDPSetAlphaDither, G_AD_DISABLE);
        AppendSyncedRdpState(gfx, gDPSetColorDither, G_CD_MAGICSQ);
        gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
        AppendSyncedRdpState(gfx, gDPSetTexturePersp, G_TP_PERSP);
        AppendSyncedRdpState(gfx, gDPSetTextureFilter, G_TF_BILERP);
        AppendSyncedRdpState(gfx, gDPSetTextureLUT, G_TT_RGBA16);
        AppendSyncedRdpState(gfx, gDPSetTextureLOD, G_TL_TILE);
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, f_LogoLettersAlpha);
        gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_LogoLetterImages)));
        gDPPipeSync(gfx++);
        for (logoLetter = 0; logoLetter < LOGO_LETTER_COUNT; logoLetter++) {
            for (j = 0; j < LOGO_LETTER_HALF_COUNT; j++) {
                letterTexelsOffset = ((j + (logoLetter * LOGO_LETTER_HALF_COUNT)) *
                                         LOGO_LETTER_WIDTH * LOGO_LETTER_HALF_HEIGHT) +
                    LARGE_PALETTED_TEXELS_OFFSET;
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress(f_LogoLetterImages + letterTexelsOffset),
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    LOGO_LETTER_WIDTH,
                    LOGO_LETTER_HALF_HEIGHT,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                gSPMatrix(
                    gfx++,
                    PhysicalAddress(&g_ScreenOrthoMatrix),
                    G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH
                );
                gSPMatrix(
                    gfx++,
                    PhysicalAddress(&f_LogoLetterTranslations[logoLetter][g_DrawBufferIndex]),
                    G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH
                );
                gSPMatrix(
                    gfx++,
                    PhysicalAddress(&f_LogoLetterScalings[logoLetter][g_DrawBufferIndex]),
                    G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH
                );
                gDPSetPrimDepth(
                    gfx++,
                    f_LogoLetters[logoLetter].depth + LOGO_LETTER_DEPTH_OFFSET,
                    0
                );
                gDPPipeSync(gfx++);
                gSPVertex(gfx++, f_LogoLetterVertices[j], QUAD_VERTEX_COUNT, 0);
                gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
                gDPPipeSync(gfx++);
            }
        }

        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, f_LogoClubAlpha);
        gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_LogoClubImage)));
        gDPPipeSync(gfx++);
        for (j = 0; j < LOGO_CLUB_STRIP_COUNT; j++) {
            texelsOffset =
                (j * LOGO_CLUB_WIDTH * LOGO_CLUB_STRIP_HEIGHT) + LARGE_PALETTED_TEXELS_OFFSET;
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(f_LogoClubImage + texelsOffset),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                LOGO_CLUB_WIDTH,
                LOGO_CLUB_STRIP_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gSPMatrix(
                gfx++,
                PhysicalAddress(&g_ScreenOrthoMatrix),
                G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH
            );
            gSPMatrix(
                gfx++,
                PhysicalAddress(&f_LogoClubTranslations[g_DrawBufferIndex]),
                G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH
            );
            gSPMatrix(
                gfx++,
                PhysicalAddress(&f_LogoClubRotations[g_DrawBufferIndex]),
                G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH
            );
            gSPMatrix(
                gfx++,
                PhysicalAddress(&f_LogoClubScalings[g_DrawBufferIndex]),
                G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH
            );
            gDPSetPrimDepth(gfx++, LOGO_CLUB_DEPTH, 0);
            gDPPipeSync(gfx++);
            gSPVertex(gfx++, f_LogoClubVertices[j], QUAD_VERTEX_COUNT, 0);
            gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
            gDPPipeSync(gfx++);
        }
    }

    if ((f_LogoState == LOGO_SHINING) & (isDrawn == TRUE)) {
        gDPPipeSync(gfx++);
        gDPSetDepthSource(gfx++, G_ZS_PRIM);
        gDPSetPrimDepth(gfx++, LOGO_SHINE_MASK_DEPTH, 0);
        gDPSetCombineLERP(gfx++, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(
            gfx++,
            GBL_c1(G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
            G_RM_AA_ZB_OPA_SURF2
        );
        gSPTextureRectangle(
            gfx++,
            QuarterPixels(LOGO_SHINE_MASK_LEFT),
            QuarterPixels(LOGO_SHINE_TOP),
            QuarterPixels(LOGO_SHINE_MASK_RIGHT),
            QuarterPixels(LOGO_SHINE_BOTTOM),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPSetPrimDepth(gfx++, LOGO_SHINE_LETTER_DEPTH, 0);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(
            gfx++,
            AA_EN | Z_UPD | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | CVG_X_ALPHA | ALPHA_CVG_SEL |
                GBL_c1(G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
            AA_EN | Z_UPD | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | CVG_X_ALPHA | ALPHA_CVG_SEL |
                GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)
        );
        gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_LogoLetterImages)));
        for (logoLetter = 0; logoLetter < LOGO_LETTER_COUNT; logoLetter++) {
            for (j = 0; j < LOGO_LETTER_HALF_COUNT; j++) {
                texelsOffset = ((j + (logoLetter * LOGO_LETTER_HALF_COUNT)) * LOGO_LETTER_WIDTH *
                                   LOGO_LETTER_HALF_HEIGHT) +
                    LARGE_PALETTED_TEXELS_OFFSET;
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress(f_LogoLetterImages + texelsOffset),
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    LOGO_LETTER_WIDTH,
                    LOGO_LETTER_HALF_HEIGHT,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                gDPPipeSync(gfx++);
                gSPMatrix(
                    gfx++,
                    PhysicalAddress(&g_ScreenOrthoMatrix),
                    G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH
                );
                gSPMatrix(
                    gfx++,
                    PhysicalAddress(&f_LogoLetterTranslations[logoLetter][g_DrawBufferIndex]),
                    G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH
                );
                gSPMatrix(
                    gfx++,
                    PhysicalAddress(&f_LogoLetterScalings[logoLetter][g_DrawBufferIndex]),
                    G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH
                );
                gDPPipeSync(gfx++);
                gSPVertex(gfx++, f_LogoLetterVertices[j], QUAD_VERTEX_COUNT, 0);
                gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
                gDPPipeSync(gfx++);
            }
        }

        if (isDrawn == TRUE) {
            gDPSetPrimDepth(gfx++, LOGO_SHINE_DEPTH, 0);
            AppendSyncedRdpState(gfx, gDPSetRenderMode, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
            gDPSetCombineLERP(
                gfx++,
                0,
                0,
                0,
                PRIMITIVE,
                PRIMITIVE,
                0,
                TEXEL0,
                0,
                0,
                0,
                0,
                PRIMITIVE,
                PRIMITIVE,
                0,
                TEXEL0,
                0
            );
            AppendSyncedRdpState(gfx, gDPSetTextureLUT, G_TT_NONE);
            AppendSyncedRdpState(gfx, gDPSetAlphaDither, G_AD_PATTERN);
            AppendSyncedRdpState(gfx, gDPSetAlphaCompare, G_AC_NONE);
            gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
            AppendSyncedRdpState(gfx, gDPSetTexturePersp, G_TP_NONE);
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(ImageBody(f_LogoShineImage)),
                G_IM_FMT_I,
                G_IM_SIZ_8b,
                SHINE_SIZE,
                SHINE_SIZE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
        }

        f_LogoShineFrame++;
        if (f_LogoShineFrame > LOGO_SHINE_LAST_FRAME) {
            f_LogoState = LOGO_SHRINK_STARTING;
        }

        if (isDrawn == TRUE &&
            f_LogoShineFrame > LOGO_SHINE_FIRST_SHOWN_FRAME &&
            f_LogoShineFrame < LOGO_SHINE_LAST_SHOWN_FRAME) {
            gSPTextureRectangle(
                gfx++,
                QuarterPixels(LOGO_SHINE_LEFT),
                QuarterPixels(LOGO_SHINE_TOP),
                QuarterPixels(LOGO_SHINE_RIGHT),
                QuarterPixels(LOGO_SHINE_BOTTOM),
                G_TX_RENDERTILE,
                TexelCoordinate(
                    LOGO_SHINE_ORIGIN_TEXEL -
                    (f_LogoShineFrame - LOGO_SHINE_ORIGIN_FRAME) * LOGO_SHINE_SPEED
                ),
                0,
                LOGO_SHINE_TEXEL_STEP,
                LOGO_SHINE_TEXEL_STEP
            );
            gDPPipeSync(gfx++);
        }
    }

    AppendSyncedRdpState(gfx, gDPSetDepthSource, G_ZS_PIXEL);
    *displayListHead = gfx;

    for (logoLetter = 0; logoLetter < LOGO_LETTER_COUNT; logoLetter++) {
        if (f_LogoLetterStates[logoLetter] == LOGO_LETTER_FALLING) {
            if (f_LogoLetterSpeeds[logoLetter] > 0.0f) {
                if (logoLetter != LOGO_LETTER_L) {
                    if (f_LogoLetters[logoLetter].y + (s32)f_LogoLetterSpeeds[logoLetter] >=
                        LOGO_LETTER_GROUND_Y) {
                        f_LogoLetterSpeeds[logoLetter] *= LOGO_LETTER_BOUNCINESS;
                        f_LogoLetters[logoLetter].y = LOGO_LETTER_GROUND_Y - 1;
                        if (f_LogoLetterSpeeds[logoLetter] <= LOGO_LETTER_BOUNCE_SPEED_MIN) {
                            f_LogoLetterSpeeds[logoLetter] = 0.0f;
                            f_LogoLetterStates[logoLetter] = LOGO_LETTER_LANDED;
                            continue;
                        }

                        f_LogoLetterSpeeds[logoLetter] = -f_LogoLetterSpeeds[logoLetter];
                        f_LogoLetterStates[logoLetter] = LOGO_LETTER_SQUASHING;
                        f_LogoLetterSquashFrames[logoLetter] = LOGO_LETTER_SQUASH_FRAMES;
                    }
                }
                else {
                    if (f_LogoLetters[logoLetter].y + (s32)f_LogoLetterSpeeds[logoLetter] >=
                        LOGO_LETTER_L_GROUND_Y) {
                        f_LogoLetterSpeeds[logoLetter] *= LOGO_LETTER_BOUNCINESS;
                        f_LogoLetters[logoLetter].y = LOGO_LETTER_L_GROUND_Y - 1;
                        if (f_LogoLetterSpeeds[logoLetter] <= LOGO_LETTER_BOUNCE_SPEED_MIN) {
                            f_LogoLetterSpeeds[logoLetter] = 0.0f;
                            f_LogoLetterStates[logoLetter] = LOGO_LETTER_LANDED;
                            continue;
                        }

                        f_LogoLetterSpeeds[logoLetter] = -f_LogoLetterSpeeds[logoLetter];
                        f_LogoLetterStates[logoLetter] = LOGO_LETTER_SQUASHING;
                        f_LogoLetterSquashFrames[logoLetter] = LOGO_LETTER_SQUASH_FRAMES;
                    }
                }
            }

            f_LogoLetters[logoLetter].y += (s32)f_LogoLetterSpeeds[logoLetter];
            f_LogoLetterSpeeds[logoLetter] += LOGO_LETTER_GRAVITY;
            f_LogoLetterScales[logoLetter][AXIS_X] += LOGO_LETTER_GROWTH;
            f_LogoLetterScales[logoLetter][AXIS_Y] += LOGO_LETTER_GROWTH;
            if (f_LogoLetterScales[logoLetter][AXIS_X] > 1.0f) {
                f_LogoLetterScales[logoLetter][AXIS_X] = 1.0f;
            }

            if (f_LogoLetterScales[logoLetter][AXIS_Y] > 1.0f) {
                f_LogoLetterScales[logoLetter][AXIS_Y] = 1.0f;
            }
        }
        else if (f_LogoLetterStates[logoLetter] == LOGO_LETTER_SQUASHING) {
            f_LogoLetterScales[logoLetter][AXIS_X] += LOGO_LETTER_SQUASH_STEP;
            f_LogoLetterScales[logoLetter][AXIS_Y] -= LOGO_LETTER_SQUASH_STEP;
            f_LogoLetterSquashFrames[logoLetter]--;
            if (f_LogoLetterSquashFrames[logoLetter] == 0) {
                f_LogoLetterStates[logoLetter] = LOGO_LETTER_SQUASHED;
                f_LogoLetterSquashFrames[logoLetter] = LOGO_LETTER_SQUASHED_FRAMES;
            }
        }
        else if (f_LogoLetterStates[logoLetter] == LOGO_LETTER_SQUASHED) {
            f_LogoLetterSquashFrames[logoLetter]--;
            if (f_LogoLetterSquashFrames[logoLetter] == 0) {
                f_LogoLetterStates[logoLetter] = LOGO_LETTER_STRETCHING;
                f_LogoLetterSquashFrames[logoLetter] = LOGO_LETTER_STRETCH_FRAMES;
            }
        }
        else if (f_LogoLetterStates[logoLetter] == LOGO_LETTER_STRETCHING) {
            f_LogoLetterScales[logoLetter][AXIS_X] -= LOGO_LETTER_STRETCH_STEP;
            f_LogoLetterScales[logoLetter][AXIS_Y] += LOGO_LETTER_STRETCH_STEP;
            f_LogoLetterSquashFrames[logoLetter]--;
            if (f_LogoLetterSquashFrames[logoLetter] == 0) {
                f_LogoLetterStates[logoLetter] = LOGO_LETTER_FALLING;
            }
        }
    }

    if (f_LogoState == LOGO_CLUB_STARTING) {
        f32 wobble;

        angle = f_LogoClubOrbitAngle * DEGREES_TO_RADIANS;
        sinf(f_LogoClubUnusedAngle * DEGREES_TO_RADIANS);
        wobble = -sinf(f_LogoClubWobbleAngle * DEGREES_TO_RADIANS) * LOGO_CLUB_FIRST_WOBBLE;
        f_LogoState = LOGO_CLUB_CIRCLING;
        f_LogoClubPosition[AXIS_X] =
            (s32)(-cosf(angle) * (f_LogoClubOrbitRadius + wobble)) + LOGO_CLUB_ORBIT_CENTER_X;
        f_LogoClubPosition[AXIS_Y] =
            (s32)(-sinf(angle) * (f_LogoClubOrbitRadius + wobble)) + LOGO_CLUB_ORBIT_CENTER_Y;
        f_IsLogoClubSwinging = TRUE;
    }

    if (f_LogoState == LOGO_CLUB_CIRCLING) {
        orbitAngle = f_LogoClubOrbitAngle * DEGREES_TO_RADIANS;
        sinf(f_LogoClubUnusedAngle * DEGREES_TO_RADIANS);
        wobble = -sinf(f_LogoClubWobbleAngle * DEGREES_TO_RADIANS) * LOGO_CLUB_WOBBLE;
        f_LogoClubPosition[AXIS_X] =
            (s32)(-cosf(orbitAngle) * (f_LogoClubOrbitRadius + wobble)) + LOGO_CLUB_ORBIT_CENTER_X;
        f_LogoClubPosition[AXIS_Y] =
            (s32)(-sinf(orbitAngle) * (f_LogoClubOrbitRadius + wobble)) + LOGO_CLUB_ORBIT_CENTER_Y;
        f_LogoClubOrbitRadius -= LOGO_CLUB_ORBIT_RADIUS_STEP;
        f_LogoClubOrbitAngle += LOGO_CLUB_ORBIT_ANGLE_STEP;
        f_LogoClubAngle -= LOGO_CLUB_SPIN_STEP;
        f_LogoClubUnusedAngle += LOGO_CLUB_UNUSED_ANGLE_STEP;
        f_LogoClubWobbleAngle += LOGO_CLUB_WOBBLE_ANGLE_STEP;
        if (f_LogoClubOrbitRadius < 0.0f) {
            f_LogoClubOrbitRadius = 0.0f;
            f_LogoState = LOGO_CLUB_SPINNING;
        }
    }

    if (f_LogoState == LOGO_CLUB_SPINNING) {
        f_LogoClubAngle -= LOGO_CLUB_SPIN_STEP;
        f_LogoClubSpinFramesLeft--;
        if (f_LogoClubSpinFramesLeft < 0) {
            f_LogoClubSpinFramesLeft = 0;
            if (f_LogoClubAngle < 0.0f) {
                f_LogoClubAngle = 0.0f;
                f_LogoState = LOGO_SHINING;
                f_IsLogoClubSwinging = FALSE;
            }
        }
    }

    if (f_LogoState == LOGO_SHRINK_STARTING) {
        f_LogoState = LOGO_SHRINKING;

        f_LogoLetterSteps[LOGO_LETTER_M] =
            (LOGO_M_SHRUNK_X - f_LogoLetters[LOGO_LETTER_M].x) / LOGO_SHRINK_FRAMES;
        f_LogoLetterSteps[LOGO_LETTER_A] =
            (LOGO_A_SHRUNK_X - f_LogoLetters[LOGO_LETTER_A].x) / LOGO_SHRINK_FRAMES;
        f_LogoLetterSteps[LOGO_LETTER_R] =
            (LOGO_R_SHRUNK_X - f_LogoLetters[LOGO_LETTER_R].x) / LOGO_SHRINK_FRAMES;
        f_LogoLetterSteps[LOGO_LETTER_I] =
            (LOGO_I_SHRUNK_X - f_LogoLetters[LOGO_LETTER_I].x) / LOGO_SHRINK_FRAMES;
        f_LogoLetterSteps[LOGO_LETTER_FIRST_O] =
            (LOGO_FIRST_O_SHRUNK_X - f_LogoLetters[LOGO_LETTER_FIRST_O].x) / LOGO_SHRINK_FRAMES;
        f_LogoLetterSteps[LOGO_LETTER_G] =
            (LOGO_G_SHRUNK_X - f_LogoLetters[LOGO_LETTER_G].x) / LOGO_SHRINK_FRAMES;
        f_LogoLetterSteps[LOGO_LETTER_SECOND_O] =
            (LOGO_SECOND_O_SHRUNK_X - f_LogoLetters[LOGO_LETTER_SECOND_O].x) / LOGO_SHRINK_FRAMES;
        f_LogoLetterSteps[LOGO_LETTER_L] =
            (LOGO_L_SHRUNK_X - f_LogoLetters[LOGO_LETTER_L].x) / LOGO_SHRINK_FRAMES;
        f_LogoLetterSteps[LOGO_LETTER_F] =
            (LOGO_F_SHRUNK_X - f_LogoLetters[LOGO_LETTER_F].x) / LOGO_SHRINK_FRAMES;
        f_LogoLetterRises[LOGO_LETTER_M] =
            (LOGO_SHRUNK_Y - f_LogoLetters[LOGO_LETTER_M].y) / LOGO_SHRINK_FRAMES;
        f_LogoLetterRises[LOGO_LETTER_A] =
            (LOGO_SHRUNK_L_Y - f_LogoLetters[LOGO_LETTER_L].y) / LOGO_SHRINK_FRAMES;
        f_LogoScaleStep = (LOGO_SHRUNK_SCALE - f_LogoLetterScales[LOGO_LETTER_M][AXIS_X]) *
            (1.0f / LOGO_SHRINK_FRAMES);
        f_LogoClubStepX = (LOGO_SHRUNK_CLUB_X - f_LogoClubPosition[AXIS_X]) / LOGO_SHRINK_FRAMES;
        f_LogoClubStepY = (LOGO_SHRUNK_CLUB_Y - f_LogoClubPosition[AXIS_Y]) / LOGO_SHRINK_FRAMES;
    }

    if (f_LogoState == LOGO_SHRINKING) {
        for (letterIndex = 0; letterIndex < LOGO_LETTER_COUNT; letterIndex++) {
            if (letterIndex != LOGO_LETTER_L) {
                f_LogoLetters[letterIndex].x += f_LogoLetterSteps[letterIndex];
                f_LogoLetters[letterIndex].y += f_LogoLetterRises[LOGO_LETTER_M];
                f_LogoLetterScales[letterIndex][AXIS_X] += f_LogoScaleStep;
                f_LogoLetterScales[letterIndex][AXIS_Y] += f_LogoScaleStep;
            }
        }

        f_LogoLetters[LOGO_LETTER_L].x += f_LogoLetterSteps[LOGO_LETTER_L];
        f_LogoLetters[LOGO_LETTER_L].y += f_LogoLetterRises[LOGO_LETTER_A];
        f_LogoLetterScales[LOGO_LETTER_L][AXIS_X] += f_LogoScaleStep;
        f_LogoLetterScales[LOGO_LETTER_L][AXIS_Y] += f_LogoScaleStep;
        f_LogoClubPosition[AXIS_X] += f_LogoClubStepX;
        f_LogoClubPosition[AXIS_Y] += f_LogoClubStepY;
        f_LogoClubScale[AXIS_X] += f_LogoScaleStep;
        f_LogoClubScale[AXIS_Y] += f_LogoScaleStep;
        if (f_LogoLetterScales[LOGO_LETTER_M][AXIS_X] < LOGO_SHRUNK_SCALE) {
            f_LogoState = LOGO_SHRUNK;
        }
    }
}

#define SPARK_FRAME_COUNT 10
#define SPARK_SCALE_DEPTH 48.0f
#define SPARK_ANGLE_STEP (M_PI * (2.0f / SPARK_COUNT))
#define SPARK_ANGLE_JITTER_RANGE 20
#define SPARK_SPEED_MIN 10.0f
#define SPARK_SPEED_RANGE 16
#define SPARK_DRAG 0.98
#define SPARK_DEPTH (-100.0f)

void DrawOpeningSparks(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    ScreenPoint origin;
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 matrix;
    s32 depth;
    s32 sparkIndex;
    f32 speed;
    Gfx *syncCommand;
    Gfx *cycleTypeCommand;

    if (f_OpeningSparksState == SPARKS_STARTING) {
        depth = ProjectPoint(
            f_OpeningSparksOrigin[AXIS_X],
            f_OpeningSparksOrigin[AXIS_Y],
            f_OpeningSparksOrigin[AXIS_Z],
            &origin
        );
        if (depth < 0) {
            f_OpeningSparkScale = SPARK_SCALE_DEPTH / (f32)-depth;
            for (sparkIndex = 0; sparkIndex < SPARK_COUNT; sparkIndex++) {
                f_OpeningSparkPositions[sparkIndex][AXIS_X] = origin.x - (SCREEN_WIDTH / 2);
                f_OpeningSparkPositions[sparkIndex][AXIS_Y] = (SCREEN_HEIGHT / 2) - origin.y;
                f_OpeningSparkAngles[sparkIndex] = ((f32)sparkIndex * SPARK_ANGLE_STEP) +
                    (f32)DegreesToRadians(
                        (guRandom() % SPARK_ANGLE_JITTER_RANGE) - (SPARK_ANGLE_JITTER_RANGE / 2)
                    );
                speed = ((guRandom() % SPARK_SPEED_RANGE) * f_OpeningSparkScale) + SPARK_SPEED_MIN;
                f_OpeningSparkVelocities[sparkIndex][AXIS_X] =
                    sinf(f_OpeningSparkAngles[sparkIndex]) * speed * f_OpeningSparkScale;
                f_OpeningSparkVelocities[sparkIndex][AXIS_Y] =
                    -cosf(f_OpeningSparkAngles[sparkIndex]) * speed * f_OpeningSparkScale;
                f_OpeningSparksState = SPARKS_FLYING;
            }

            f_OpeningSparkFramesLeft = SPARK_FRAME_COUNT;
        }
        else {
            f_OpeningSparksState = SPARKS_HIDDEN;
        }
    }

    if (f_OpeningSparksState == SPARKS_FLYING) {
        syncCommand = gfx++;
        gDPPipeSync(syncCommand);
        gDPPipeSync(gfx++);
        cycleTypeCommand = gfx++;
        gDPSetCycleType(cycleTypeCommand, G_CYC_1CYCLE);
        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_DISABLE);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_OFF);
        gSPLoadGeometryMode(gfx++, 0);
        gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        gDPSetCombineLERP(
            gfx++,
            PRIMITIVE,
            0,
            SHADE,
            0,
            0,
            0,
            0,
            SHADE,
            PRIMITIVE,
            0,
            SHADE,
            0,
            0,
            0,
            0,
            SHADE
        );
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_NONE);
        gDPPipeSync(gfx++);
        gSPMatrix(
            gfx++,
            PhysicalAddress(&g_ScreenOrthoMatrix),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
        );
        for (sparkIndex = 0; sparkIndex < SPARK_COUNT; sparkIndex++) {
            f_OpeningSparkPositions[sparkIndex][AXIS_X] +=
                f_OpeningSparkVelocities[sparkIndex][AXIS_X];
            f_OpeningSparkPositions[sparkIndex][AXIS_Y] +=
                f_OpeningSparkVelocities[sparkIndex][AXIS_Y];
            f_OpeningSparkVelocities[sparkIndex][AXIS_X] *= SPARK_DRAG;
            f_OpeningSparkVelocities[sparkIndex][AXIS_Y] *= SPARK_DRAG;
            translation[AXIS_X] = f_OpeningSparkPositions[sparkIndex][AXIS_X];
            translation[AXIS_Y] = f_OpeningSparkPositions[sparkIndex][AXIS_Y];
            translation[AXIS_Z] = SPARK_DEPTH;
            rotation[AXIS_X] = 0.0f;
            rotation[AXIS_Y] = 0.0f;
            rotation[AXIS_Z] = f_OpeningSparkAngles[sparkIndex];
            scale[AXIS_X] = f_OpeningSparkScale;
            scale[AXIS_Y] = f_OpeningSparkScale;
            scale[AXIS_Z] = f_OpeningSparkScale;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(matrix, &f_OpeningSparkMatrices[g_DrawBufferIndex][sparkIndex]);
            gSPMatrix(
                gfx++,
                PhysicalAddress(&f_OpeningSparkMatrices[g_DrawBufferIndex][sparkIndex]),
                G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
            );
            gSPVertex(gfx++, f_OpeningSparkVertices, TRIANGLE_VERTEX_COUNT, 0);
            gSP1Triangle(gfx++, 0, 1, 2, 0);
        }

        if (--f_OpeningSparkFramesLeft == 0) {
            f_OpeningSparksState = SPARKS_HIDDEN;
        }
    }

    *displayListHead = gfx;
}
