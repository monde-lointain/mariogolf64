#ifndef ROUND_HUD_H
#define ROUND_HUD_H

#include <ultra64.h>

#include "camera.h"
#include "character.h"
#include "language.h"
#include "math_util.h"
#include "player.h"

enum ShotEffect {
    SHOT_EFFECT_NONE = -1,
    SHOT_EFFECT_LONG_SHOT,
    SHOT_EFFECT_MAX_POWER
};

enum LandingReaction {
    LANDING_REACTION_NONE = -1,
    LANDING_REACTION_REST = 1,
    LANDING_REACTION_SURFACE_ONLY
};

enum HeightComparison {
    HEIGHT_ABOVE,
    HEIGHT_BELOW,
    HEIGHT_LEVEL
};

#define NO_SCORECARD_PAGE (-1)

#define HUD_FLAG_AIMING 0x1
#define HUD_FLAG_LIGHT_CLUB_HEAD 0x2
#define HUD_FLAG_NO_IMPACT_POINT 0x4
#define HUD_FLAG_ANIMATE_OFFSET_MARKER 0x8
#define HUD_FLAG_SETTING_POWER 0x10
#define HUD_FLAG_SETTING_IMPACT 0x20
#define HUD_FLAG_TRANSLUCENT_GAUGE 0x40
#define HUD_FLAG_SWINGING 0x80

typedef struct RoundHud {
    s8 scorecardPage;
    s8 unusedScorecardValue;
    s8 unusedHoleEndValue;
    s8 otherUnusedScorecardValue;
    u8 unusedBeforeLetterbox;
    s8 isLetterboxShown;
    s8 isIgnoredPausePartShown;
    s8 isWindIndicatorShown;
    s8 isHoleInfoShown;
    s8 isLieIconShown;
    s8 isClubHeadIconShown;
    s8 isUnreadItemShown;
    s8 isClubPanelShown;
    s8 isProgressShown;
    u8 unusedBeforeDistanceSign;
    s8 isDistanceSignShown;
    s8 greenGridState;
    u8 unusedAfterGreenGrid;
    s8 isPinFlagShown;
    s8 isShotLengthShown;
    u8 unusedBeforeHoleIntro[2];
    s8 isHoleIntroTextShown;
    s8 isBallTrailShown;
    s8 ballHeightYardsDirection;
    s8 ballHeightMetersDirection;
    s8 ballHeightFeetDirection;
    u8 unusedAfterBallHeights;
    s32 ballHeightYards;
    f32 ballHeightMeters;
    f32 ballHeightFeet;
    s32 cupDistance;
    s32 stopwatchFrameCount;
    u8 unusedBeforeSkins[0x78 - 0x30];
    s16 skinsTotals[PLAYER_COUNT_MAX];
    BinaryAngle clubHeadTilt;
    BinaryAngle lieTilt;
    u8 isIgnoredTurnPartShown;
    u8 isPuttingClubChosen;
    u8 unusedAfterClubChoice;
    s8 shooterIndex;
    u8 flags;
    u8 areLieArrowsShown;
    u8 areWindSpeedArrowsShown;
    u8 areWindDirectionArrowsShown;
} RoundHud;

extern RoundHud g_RoundHud;
#define MATRIX_BUFFER_COUNT 2
extern s32 g_MatrixBufferIndex;
extern s32 g_ShotPanelOffsetX;

enum BallTrailStateValue {
    BALL_TRAIL_OFF,
    BALL_TRAIL_RECORDING,
    BALL_TRAIL_HELD
};

typedef s32 BallTrailState;
extern BallTrailState g_BallTrailState;

void PrepareHoleDisplay(void);
void InitRoundHud(void);
void LoadRoundHud(void);
void DrawInsetViewFrame(void);
void DrawBallTrail(Gfx **displayListHead, CameraMatrices *matrices);
void DrawHoleIntroImages(Gfx **displayListHead);
void LoadPlayerFace(s32 playerIndex, CharacterId character, s32 paletteIndex);
void EndHoleIntro(void);
void DrawColorTest(Gfx **displayListHead);
void StartHoleIntro(void);
void StartShotPanel(void);
void LoadSceneryImages(void);
void FreeSceneryImages(void);
void InitScenery(void);
void FreeScenery(void);
void LoadHoleIntroImages(void);
void FreeHoleIntroImages(void);
void FreeRoundHudTextures(void);
u32 GetWarmthTint(s32 warmth, s32 alpha);
void ResetShotOffsetMarker(void);
void SetShotOffsetMarker(s32 x, s32 y);
void SwapMatrixBuffers(void);
void ResetBallTrail(void);
void ScrollWindArrow(void);
void AdjustHudOffset(void);
#define RING_TEXTURE_SIZE 64
extern u8 *g_RingTexels;
extern void *g_PlayerTagImages;
extern void *g_LeaderLineImage;
extern void *g_DistanceSignImage;
extern void *g_PinFlagImages;
#define SHOT_PREVIEW_IMAGE_SIZE 32
extern void *g_ShotPreviewDashImage;

void StartLandingReaction(s32 reaction);
void StartShotEffect(s32 effect);
void DrawHoleFlag(Boolean isBackDrawn);

void DrawGreenGrid(Gfx **displayListHead, CameraMatrices *matrices);
void DrawHoleIntroText(Gfx **displayListHead);
void DrawRoundHud(CameraView *view);
void DrawShotEffect(Gfx **displayListHead);

#endif
