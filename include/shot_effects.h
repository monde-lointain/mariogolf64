#ifndef SHOT_EFFECTS_H
#define SHOT_EFFECTS_H

#include "common.h"

#include <ultra64.h>

#include "camera.h"

/* The states of g_ImpactFlashState, which lights the golfer as the ball is
 * struck */
enum ImpactFlashState {
  IMPACT_FLASH_NONE,
  IMPACT_FLASH_RISING,
  IMPACT_FLASH_FADING
};

extern s32 g_ImpactFlashState;
/* The level of the golfer's second light while the impact flash lasts */
extern s32 g_ImpactFlashLevel;

void ResetShotEffects(void);
void ClearShotEffects(s32 kind);
void StopAllSounds(void);
void StartImpactSparks(void);
/* Images that the shot effects and the contact effects share */
extern u8* g_SharedEffectImages;

void LoadShotEffectImages(void);
void FreeShotEffectImages(void);
void StartFlagSparks(CoursePoint point);
void StartFlagstickSparks(CoursePoint point);
void StartSwingTrail(s32 kind);
void DrawSwingTrail(Gfx** displayListHead, CameraMatrices* matrices);

#endif
