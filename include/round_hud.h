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

#endif
