/*
 * The round's HUD: the shot panel, the wind, the green's grid, the ball's trail
 * and the hole intro.
 */

#ifndef ROUND_HUD_H
#define ROUND_HUD_H

#include <ultra64.h>

#include "camera/camera.h"
#include "characters/character.h"
#include "math/math_util.h"
#include "round/player.h"
#include "system/asset.h"
#include "system/c_language.h"

// The effects over a shot in flight that StartShotEffect starts
enum ShotEffect {
    SHOT_EFFECT_NONE = -1,
    SHOT_EFFECT_LONG_SHOT,
    SHOT_EFFECT_MAX_POWER // Started by a maximum power impact
};

typedef s32 ShotEffect;

// The landing reactions that StartLandingReaction starts
enum LandingReaction {
    LANDING_REACTION_NONE = -1,
    // Shows the surface and how far the ball rests from the cup
    LANDING_REACTION_REST = 1,
    // Shows only the surface's name, in the water or off the course
    LANDING_REACTION_SURFACE_ONLY
};

typedef s32 LandingReaction;

// RoundHud.ballHeight*Direction values
enum HeightComparison {
    HEIGHT_ABOVE,
    HEIGHT_BELOW,
    HEIGHT_LEVEL
};

typedef s8 HeightComparison;

// RoundHud.scorecardPage while the scorecard is closed
#define NO_SCORECARD_PAGE (-1)

// RoundHud.flags, which UpdateTurns sets for each step of a shot
// Read by no routine; set while aiming
#define HUD_FLAG_AIMING 0x1
// Lights the club head icon fully, otherwise it is dimmed
#define HUD_FLAG_LIGHT_CLUB_HEAD 0x2
// Read by no routine; set while aiming without an impact point to show: on the
// green, for shots with SHOT_FLAG_NO_IMPACT_POINT, and in the letterbox views,
// where free look also turns the impact point off
#define HUD_FLAG_NO_IMPACT_POINT 0x4
// Animates the shot offset marker
#define HUD_FLAG_ANIMATE_OFFSET_MARKER 0x8
// Read by no routine; set while the power is set
#define HUD_FLAG_SETTING_POWER 0x10
// Read by no routine; set while the impact is set
#define HUD_FLAG_SETTING_IMPACT 0x20
// Set in free look, so the course shows through
#define HUD_FLAG_TRANSLUCENT_GAUGE 0x40
// Read by no routine; set as the swing starts
#define HUD_FLAG_SWINGING 0x80
typedef u8 UHudFlags8;

// RoundHud.greenGridState values
enum GreenGridState {
    GREEN_GRID_HIDDEN,
    GREEN_GRID_SHOWN, // Samples the ground and scrolls
    GREEN_GRID_FADING
};

typedef s32 GreenGridState;
typedef s8 GreenGridState8;

// The HUD's state for a round, which FreeScenery clears
typedef struct RoundHud {
    s8 scorecardPage; // The scorecard page that is shown
    // Only ever cleared, as the scorecard closes
    s8 unusedScorecardValue;
    // Only ever cleared, as the scorecard opens at a hole's end
    s8 unusedHoleEndValue;
    // Only ever cleared, as the scorecard closes
    s8 otherUnusedScorecardValue;
    u8 unusedBeforeLetterbox;
    Boolean8 isLetterboxShown; // DrawLetterbox draws its bars only while this is set
    // Cleared as a pause screen closes, while the HUD's other parts show again;
    // nothing reads it
    Boolean8 isIgnoredPausePartShown;
    Boolean8 isWindIndicatorShown;
    Boolean8 isHoleInfoShown; // The hole's model, yardage and par
    Boolean8 isLieIconShown;
    Boolean8 isClubHeadIconShown;
    Boolean8 isUnreadItemShown; // Set with the others; read by none
    Boolean8 isClubPanelShown;
    // The scoreboard, the distance left to the cup, the ring count and the stopwatch
    Boolean8 isProgressShown;
    u8 unusedBeforeDistanceSign;
    Boolean8 isDistanceSignShown; // At g_CurrentShot.targetPoint
    // Whether the grid over the ground shows; B toggles it while aiming
    GreenGridState8 greenGridState;
    u8 unusedAfterGreenGrid;
    Boolean8 isPinFlagShown;
    Boolean8 isShotLengthShown; // How far the ball has flown
    u8 unusedBeforeHoleIntro[2];
    // Whether the course's name and the hole's number show before the flyover
    Boolean8 isHoleIntroTextShown;
    Boolean8 isBallTrailShown;
    // The height of the ball above the cup, in yards, meters and feet, and for
    // each whether the ball is above, below or level with it
    HeightComparison ballHeightYardsDirection;
    HeightComparison ballHeightMetersDirection;
    HeightComparison ballHeightFeetDirection;
    u8 unusedAfterBallHeights;
    s32 ballHeightYards;
    f32 ballHeightMeters;
    f32 ballHeightFeet;
    s32 cupDistance;
    s32 stopwatchFrameCount; // The Speed Golf stopwatch of the player taking a turn
    u8 unusedBeforeSkins[0x78 - 0x30];
    s16 skinsTotals[PLAYER_COUNT_MAX]; // The skins each player has won, as TallySkins counts them
    BinaryAngle clubHeadTilt; // The icons tilt by half
    BinaryAngle lieTilt; // The slope under the ball
    // Set as the turn starts, with the HUD's other parts; nothing reads it
    UBoolean8 isIgnoredTurnPartShown;
    // Whether the chosen club can putt; nothing reads it
    UBoolean8 isPuttingClubChosen;
    u8 unusedAfterClubChoice;
    s8 shooterIndex; // Of the player whose turn it is
    UHudFlags8 flags; // HUD_FLAG_* bits
    UBoolean8 areLieArrowsShown; // While the pause menu sets the lie
    UBoolean8 areWindSpeedArrowsShown;
    UBoolean8 areWindDirectionArrowsShown;
} RoundHud;

extern RoundHud g_RoundHud;
#define MATRIX_BUFFER_COUNT 2
// Which of the MATRIX_BUFFER_COUNT matrix buffers this frame fills
extern s32 g_MatrixBufferIndex;
// How far the shot panel still has to slide in, down to 0
extern s32 g_ShotPanelOffsetX;

// What DrawBallTrail does with the ball's trail each frame
enum BallTrailStateValue {
    BALL_TRAIL_OFF, // Not drawn
    BALL_TRAIL_RECORDING, // Drawn, adding the ball's position; never set
    BALL_TRAIL_HELD // Drawn as it is
};

typedef s32 BallTrailState;
// The ball trail's state
extern BallTrailState g_BallTrailState;

void PrepareHoleDisplay(void);
void InitRoundHud(void);
void LoadRoundHud(void);
void DrawInsetViewFrame(void);
void DrawBallTrail(Gfx **displayListHead, CameraMatrices *matrices);
void DrawHoleIntroImages(Gfx **displayListHead);
void LoadPlayerFace(s32 playerIndex, CharacterIndex characterIndex, CButtonPalette paletteIndex);
void StopHoleIntro(void);
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
// An I8 texture of a ring, that marks where the ball will land and draws the
// lens flare
#define RING_TEXTURE_SIZE 64
extern u8 *g_RingTexels;
// The 16 by 8 tags "1P" to "4P", "1C" to "4C" and "Com", over one palette
extern ImageAsset *g_PlayerTagImages;
// The line from a player's tag to their ball
extern ImageAsset *g_LeaderLineImage;
// The wooden sign behind the shot's distance
extern ImageAsset *g_DistanceSignImage;
// The frames of the flag that marks the pin
extern ImageAsset *g_PinFlagImages;
// The I8 dash that shot previews are drawn with, this many texels on a side
#define SHOT_PREVIEW_IMAGE_SIZE 32
extern ImageAsset *g_ShotPreviewDashImage;

void StartLandingReaction(LandingReaction reaction);
void StartShotEffect(ShotEffect effect);
void DrawHoleFlag(Boolean isBackDrawn);

void DrawGreenGrid(Gfx **displayListHead, CameraMatrices *matrices);
void DrawHoleIntroText(Gfx **displayListHead);
void DrawRoundHud(CameraView *view);
void DrawShotEffect(Gfx **displayListHead);

#endif
