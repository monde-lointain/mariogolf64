#ifndef ROUND_HUD_H
#define ROUND_HUD_H

#include <ultra64.h>

#include "camera.h"
#include "math_util.h"
#include "save.h"

/* The effects over a shot in flight that StartShotEffect starts */
enum ShotEffect {
  SHOT_EFFECT_NONE = -1,
  SHOT_EFFECT_LONG_SHOT,
  SHOT_EFFECT_UNK1
};

/* The landing reactions that StartLandingReaction starts */
enum LandingReaction {
  LANDING_REACTION_NONE = -1,
  /* Shows the surface and how far the ball rests from the cup */
  LANDING_REACTION_REST = 1,
  LANDING_REACTION_UNK2 /* In the water or off the course */
};

/* RoundHud.ballHeight*Direction values */
enum HeightComparison {
  HEIGHT_ABOVE,
  HEIGHT_BELOW,
  HEIGHT_LEVEL
};

/* RoundHud.scorecardPage while the scorecard is closed */
#define NO_SCORECARD_PAGE (-1)

/* RoundHud.flags, which UpdateTurns sets for each step of a shot */
/* Read by no routine; set while aiming */
#define HUD_FLAG_AIMING 0x1
/* Lights the club head icon fully, otherwise it is dimmed */
#define HUD_FLAG_LIGHT_CLUB_HEAD 0x2
/* Read by no routine; set while aiming on the green, without an impact
 * point, or away from LETTERBOX_STATE_NONE and LETTERBOX_STATE_UNUSED_AIMING */
#define HUD_FLAG_UNK4 0x4
/* Animates the shot offset marker */
#define HUD_FLAG_ANIMATE_OFFSET_MARKER 0x8
/* Read by no routine; set while the power is set */
#define HUD_FLAG_SETTING_POWER 0x10
/* Read by no routine; set while the impact is set */
#define HUD_FLAG_SETTING_IMPACT 0x20
/* Set in free look, so the course shows through */
#define HUD_FLAG_TRANSLUCENT_GAUGE 0x40
/* Read by no routine; set as the swing starts */
#define HUD_FLAG_SWINGING 0x80

/* The HUD's state for a round, which FreeScenery clears */
typedef struct RoundHud {
  /* 0x00 */ s8 scorecardPage; /* The scorecard page that is shown */
  /* 0x01 */ s8 unk1;
  /* 0x02 */ s8 unk2;
  /* 0x03 */ s8 unk3;
  /* 0x04 */ u8 unk4;
  /* 0x05 */ s8 isLetterboxShown; /* DrawLetterbox draws its bars only while
                                     this is set */
  /* 0x06 */ s8 unk6;
  /* 0x07 */ s8 isWindIndicatorShown;
  /* 0x08 */ s8 isHoleInfoShown; /* The hole's model, yardage and par */
  /* 0x09 */ s8 isLieIconShown;
  /* 0x0A */ s8 isClubHeadIconShown;
  /* 0x0B */ s8 isUnreadItemShown; /* Set with the others; read by none */
  /* 0x0C */ s8 isClubPanelShown;
  /* 0x0D */ s8 isProgressShown; /* The scoreboard, the distance left to
                                    the cup, the ring count and the
                                    stopwatch */
  /* 0x0E */ u8 unkE;
  /* 0x0F */ s8 isDistanceSignShown; /* At g_CurrentShot.targetPoint */
  /* 0x10 */ s8 greenGridState; /* Whether the grid over the ground shows; B
                                   toggles it while aiming */
  /* 0x11 */ u8 unk11;
  /* 0x12 */ s8 isPinFlagShown;
  /* 0x13 */ s8 isShotLengthShown; /* How far the ball has flown */
  /* 0x14 */ u8 unk14[2];
  /* 0x16 */ s8
      isHoleIntroTextShown; /* Whether the course's name and the hole's number
                               show before the flyover */
  /* 0x17 */ s8 isBallTrailShown;
  /*
   * The height of the ball above the cup, in yards, meters and feet, and for
   * each whether the ball is above, below or level with it
   */
  /* 0x18 */ s8 ballHeightYardsDirection;
  /* 0x19 */ s8 ballHeightMetersDirection;
  /* 0x1A */ s8 ballHeightFeetDirection;
  /* 0x1B */ u8 unk1B;
  /* 0x1C */ s32 ballHeightYards;
  /* 0x20 */ f32 ballHeightMeters;
  /* 0x24 */ f32 ballHeightFeet;
  /* 0x28 */ s32 cupDistance;
  /* 0x2C */ s32 stopwatchFrameCount; /* The Speed Golf stopwatch of the player
                                         taking a turn */
  /* 0x30 */ u8 unk30[0x78 - 0x30];
  /* 0x78 */ s16
      skinsTotals[MAX_PLAYER_COUNT]; /* The skins each player has won, as
                                        TallySkins counts them */
  /* 0x80 */ BinaryAngle clubHeadTilt; /* The icons tilt by half */
  /* 0x82 */ BinaryAngle lieTilt; /* The slope under the ball */
  /* 0x84 */ u8 unk84[3];
  /* 0x87 */ s8 shooterIndex; /* Of the player whose turn it is */
  /* 0x88 */ u8 flags; /* HUD_FLAG_* bits */
  /* 0x89 */ u8 areLieArrowsShown; /* While the pause menu sets the lie */
  /* 0x8A */ u8 areWindSpeedArrowsShown;
  /* 0x8B */ u8 areWindDirectionArrowsShown;
} RoundHud; /* size = 0x8C */

extern RoundHud g_RoundHud;
#define MATRIX_BUFFER_COUNT 2
/* Which of the MATRIX_BUFFER_COUNT matrix buffers this frame fills */
extern s32 g_MatrixBufferIndex;
/* How far the shot panel still has to slide in, down to 0 */
extern s32 g_ShotPanelOffsetX;

/* What DrawBallTrail does with the ball's trail each frame */
enum BallTrailStateValue {
  BALL_TRAIL_OFF, /* Not drawn */
  BALL_TRAIL_RECORDING, /* Drawn, adding the ball's position; never set */
  BALL_TRAIL_HELD /* Drawn as it is */
};

typedef s32 BallTrailState;
/* The ball trail's state */
extern BallTrailState g_BallTrailState;

void PrepareHoleDisplay(void);
void InitRoundHud(void);
void LoadRoundHud(void);
void DrawInsetViewFrame(void);
void DrawBallTrail(Gfx** displayListHead, CameraMatrices* matrices);
void DrawHoleIntroImages(Gfx** displayListHead);
void LoadPlayerFace(s32 playerIndex, CharacterId character, s32 paletteIndex);
void EndHoleIntro(void);
void DrawColorTest(Gfx** displayListHead);
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
/* An I8 texture of a ring, that marks where the ball will land and draws the
 * lens flare */
#define RING_TEXTURE_SIZE 64
extern u8* g_RingTexels;
/* The 16 by 8 tags "1P" to "4P", "1C" to "4C" and "Com", over one palette */
extern void* g_PlayerTagImages;
/* The line from a player's tag to their ball */
extern void* g_LeaderLineImage;
/* The wooden sign behind the shot's distance */
extern void* g_DistanceSignImage;
/* The frames of the flag that marks the pin */
extern void* g_PinFlagImages;
/* The I8 dash that shot previews are drawn with, this many texels on a side */
#define SHOT_PREVIEW_IMAGE_SIZE 32
extern void* g_ShotPreviewDashImage;

void StartLandingReaction(s32 reaction);
void StartShotEffect(s32 effect);
void DrawHoleFlag(Boolean isBackDrawn);

#endif
