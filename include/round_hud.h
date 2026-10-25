#ifndef ROUND_HUD_H
#define ROUND_HUD_H

#include <ultra64.h>

void PrepareHoleDisplay(void);
void InitRoundHud(void);
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

#endif
