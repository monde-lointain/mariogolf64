/*
 * The sparks of driven shots and flagstick hits, and the club's trail.
 */

#ifndef SHOT_EFFECTS_H
#define SHOT_EFFECTS_H

#include <ultra64.h>

#include "camera/camera.h"
#include "course/course_space.h"
#include "system/asset.h"

// The states of g_ImpactFlashState, which lights the golfer as the ball is
// struck
enum ImpactFlashState {
    IMPACT_FLASH_NONE,
    IMPACT_FLASH_RISING,
    IMPACT_FLASH_FADING
};

typedef s32 ImpactFlashState;

// What ClearShotEffects clears besides the shot's effects
enum ClearKind {
    CLEAR_KIND_EFFECTS,
    // Also stops the movie and hides the banner
    CLEAR_KIND_ALL,
    // Also hides the NICE SHOT banner
    CLEAR_KIND_NICE_SHOT_BANNER
};

typedef s32 ClearKind;

// The arguments of StartSwingTrail
enum SwingTrailKind {
    // Stops the trail at once
    SWING_TRAIL_KIND_CLEAR = -1,
    // Lets the trail fade out quickly
    SWING_TRAIL_KIND_FADE,
    // A wood's swing in power-shot mode, drawn with the image's colors
    SWING_TRAIL_KIND_POWER_SHOT,
    // A wood's swing in normal mode, drawn in plain shade
    SWING_TRAIL_KIND_NORMAL_SHOT
};

typedef s32 SwingTrailKind;
extern ImpactFlashState g_ImpactFlashState;
// The level of the golfer's second light while the impact flash lasts
extern s32 g_ImpactFlashLevel;

void ResetShotEffects(void);
void ClearShotEffects(ClearKind kind);
void StopAllSounds(void);
void StartImpactSparks(void);
// Images that the shot effects and the contact effects share
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
