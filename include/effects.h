#ifndef EFFECTS_H
#define EFFECTS_H

#include "common.h"

#include <ultra64.h>

#include "ball_simulation.h"
#include "math_util.h"
#include "save.h"

/* StartWeather's weathers */
enum Weather {
  WEATHER_NONE = -1,
  WEATHER_RAIN = 2
};

/* The sound channel the rain plays on */
#define RAIN_SOUND_CHANNEL 9
/* The sound of the rain, played at full volume when a hole starts in the rain
 */
#define RAIN_SOUND 0
#define RAIN_SOUND_START_VOLUME 100

/* The particle effects drawn in the scene, such as the sparkles of a passed
 * ring */
typedef struct Effect {
  /* 0x00 */ Vec3 position; /* In world units */
  /* 0x0C */ Vec3 velocity; /* In world units a frame */
  /* 0x18 */ u8 unk18[0xC];
  /* 0x24 */ f32 unk24; /* An angle about x and z */
  /* 0x28 */ f32 alpha;
  /* 0x2C */ f32 alphaStep;
  /* 0x30 */ f32 scale;
  /* 0x34 */ f32 scaleStep;
  /* 0x38 */ s16 unk38;
  /* 0x3A */ s8 kind;
  /* 0x3B */ s8 unk3B;
  /* 0x3C */ s8 matrixIndex; /* Of the effects' transform matrices */
} Effect; /* size = 0x40 */

#define EFFECT_COUNT 40

/*
 * Set up for each shot's debris. Keeping the club kind in a struct is there to
 * match, as it keeps its loads from moving above the effects' stores.
 */
typedef struct ShotDebris {
  /* 0x00 */ s32 unk0;
  /* 0x04 */ s32 unk4;
  /* 0x08 */ f32 unk8;
  /* 0x0C */ f32 unkC;
  /* 0x10 */ s32 clubKind;
  /* 0x14 */ s32 unk14;
} ShotDebris; /* size = 0x18 */

/* The falling rain's drops, and the splashes they make on landing */
typedef struct Raindrop Raindrop;
typedef struct RainSplash RainSplash;

extern f32 g_RainSoundVolume;

void PrepareShotEffects(BallSimulation* simulation);
void SpawnImpactEffects(BallSimulation* simulation);
void SpawnShotDebris(
    CoursePoint point, Radians pitch, Radians yaw, s32 launchSpeed);
void StartPlayerRumble(UnkB8* player);
void StartRingPassRumble(UnkB8* player);
void LoadEffectImages(void);
void FreeEffectImages(void);
void ClearEffects(void);
void StartWeather(s32 weather);
void EndWeather(void);

#endif
