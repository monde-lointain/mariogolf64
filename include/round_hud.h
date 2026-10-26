#ifndef ROUND_HUD_H
#define ROUND_HUD_H

#include <ultra64.h>
#include "camera.h"
#include "save.h"

/* The effects over a shot in flight that StartShotEffect starts */
#define SHOT_EFFECT_NONE (-1)
#define SHOT_EFFECT_LONG_SHOT 0
#define SHOT_EFFECT_UNK1 1

/* The landing reactions that StartLandingReaction starts */
#define LANDING_REACTION_NONE (-1)
#define LANDING_REACTION_REST 1 /* Shows the surface and how far the ball rests from the cup */
#define LANDING_REACTION_UNK2 2 /* In the water or off the course */

/* RoundHud.ballHeight*Direction values */
#define HEIGHT_ABOVE 0
#define HEIGHT_BELOW 1
#define HEIGHT_LEVEL 2

/* RoundHud.scorecardPage while the scorecard is closed */
#define NO_SCORECARD_PAGE (-1)

/* RoundHud.flags */
#define HUD_FLAG_ANIMATE_OFFSET_MARKER 0x8 /* Animates the shot offset marker */

/* The HUD's state for a round, which FreeScenery clears */
typedef struct RoundHud {
    /* 0x00 */ s8 scorecardPage; /* The scorecard page that is shown */
    /* 0x01 */ s8 unk1;
    /* 0x02 */ s8 unk2;
    /* 0x03 */ s8 unk3;
    /* 0x04 */ u8 unk4;
    /* 0x05 */ s8 isLetterboxShown; /* DrawLetterbox draws its bars only while this is set */
    /* 0x06 */ s8 unk6;
    /* 0x07 */ s8 unk7;
    /* 0x08 */ s8 unk8;
    /* 0x09 */ s8 unk9;
    /* 0x0A */ s8 unkA;
    /* 0x0B */ s8 unkB;
    /* 0x0C */ s8 unkC;
    /* 0x0D */ s8 unkD;
    /* 0x0E */ u8 unkE;
    /* 0x0F */ s8 unkF;
    /* 0x10 */ s8 unk10;
    /* 0x11 */ u8 unk11;
    /* 0x12 */ s8 unk12;
    /* 0x13 */ s8 unk13;
    /* 0x14 */ u8 unk14[2];
    /* 0x16 */ s8 isHoleIntroTextShown; /* Whether the course's name and the hole's number show before the flyover */
    /* 0x17 */ s8 unk17;
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
    /* 0x2C */ u8 unk2C[0x78 - 0x2C];
    /* 0x78 */ u16 skinsTotals[MAX_PLAYER_COUNT]; /* The skins each player has won, as TallySkins counts them */
    /* 0x80 */ u16 unk80;
    /* 0x82 */ u16 unk82;
    /* 0x84 */ u8 unk84[3];
    /* 0x87 */ s8 unk87;
    /* 0x88 */ u8 flags; /* HUD_FLAG_* bits */
    /* 0x89 */ u8 unk89;
    /* 0x8A */ u8 unk8A;
    /* 0x8B */ u8 unk8B;
} RoundHud; /* size = 0x8C */

extern RoundHud g_RoundHud;

void PrepareHoleDisplay(void);
void InitRoundHud(void);
void LoadRoundHud(void);
void DrawInsetViewFrame(void);
void DrawBallTrail(Gfx **displayListHead, CameraMatrices *matrices);
void DrawHoleIntroImages(Gfx **displayListHead);
void LoadPlayerFace(s32 playerIndex, s32 character, s32 paletteIndex);
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
/* An I8 texture of a ring, that marks where the ball will land and draws the lens flare */
#define RING_TEXTURE_SIZE 64
extern u8 *g_RingTexels;

void StartLandingReaction(s32 reaction);
void StartShotEffect(s32 effect);
void DrawHoleFlag(s32 isBackDrawn);

#endif
