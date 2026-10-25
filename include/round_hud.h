#ifndef ROUND_HUD_H
#define ROUND_HUD_H

#include <ultra64.h>

/* The landing reactions that StartLandingReaction starts */
#define LANDING_REACTION_NONE (-1)
#define LANDING_REACTION_UNK1 1
#define LANDING_REACTION_UNK2 2 /* In the water or off the course */

void PrepareHoleDisplay(void);
void InitRoundHud(void);
void LoadRoundHud(void);
void LoadPlayerFace(s32 playerIndex, s32 character, s32 paletteIndex);
void EndHoleIntro(void);
void StartHoleIntro(void);
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
void StartLandingReaction(s32 reaction);

#endif
