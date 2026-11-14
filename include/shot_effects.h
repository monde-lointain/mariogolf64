#ifndef SHOT_EFFECTS_H
#define SHOT_EFFECTS_H

#include <ultra64.h>

#include "camera/camera.h"
#include "course_space.h"
#include "system/asset.h"

enum ImpactFlashState {
    IMPACT_FLASH_NONE,
    IMPACT_FLASH_RISING,
    IMPACT_FLASH_FADING
};

typedef s32 ImpactFlashState;

enum ClearKind {
    CLEAR_KIND_EFFECTS,
    CLEAR_KIND_ALL,
    CLEAR_KIND_NICE_SHOT_BANNER
};

typedef s32 ClearKind;

enum SwingTrailKind {
    SWING_TRAIL_KIND_CLEAR = -1,
    SWING_TRAIL_KIND_FADE,
    SWING_TRAIL_KIND_POWER_SHOT,
    SWING_TRAIL_KIND_NORMAL_SHOT
};

typedef s32 SwingTrailKind;

extern ImpactFlashState g_ImpactFlashState;
extern s32 g_ImpactFlashLevel;

void ResetShotEffects(void);
void ClearShotEffects(ClearKind kind);
void StopAllSounds(void);
void StartImpactSparks(void);
extern ImageAsset *g_SharedEffectImages;

void LoadShotEffectImages(void);
void FreeShotEffectImages(void);
void StartFlagSparks(CoursePoint point);
void StartFlagstickSparks(CoursePoint point);
void StartSwingTrail(SwingTrailKind kind);
void DrawSwingTrail(Gfx **displayListHead, CameraMatrices *matrices);

void DrawShotEffectsHud(Gfx **displayListHead);
void DrawSparks(Gfx **displayListHead);

#endif
