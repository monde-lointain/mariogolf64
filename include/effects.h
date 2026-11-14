#ifndef EFFECTS_H
#define EFFECTS_H

#include <ultra64.h>

#include "ball_simulation.h"
#include "camera.h"
#include "club.h"
#include "compiler.h"
#include "course_space.h"
#include "geometry.h"
#include "math_util.h"
#include "player.h"

enum Weather {
    WEATHER_NONE = -1,
    WEATHER_RAIN = 2
};

typedef s32 Weather;

#define RAIN_SOUND_CHANNEL 9
#define RAIN_SOUND 0
#define RAIN_SOUND_START_VOLUME 100

typedef struct Effect {
    Vec3 position;
    Vec3 velocity;
    u8 unusedAfterVelocity[0xC];
    f32 tilt;
    f32 alpha;
    f32 alphaStep;
    f32 scale;
    f32 scaleStep;
    s16 color;
    s8 kind;
    s8 frame;
    s8 matrixIndex;
} Effect;

#define EFFECT_COUNT 40

typedef struct ShotDebris {
    s32 ignoredSpawnCount;
    s32 unusedAfterSpawnCount;
    f32 ignoredNegativeSetting;
    f32 ignoredPositiveSetting;
    ClubCategory clubCategory;
    s32 unusedAfterClubCategory;
} ShotDebris;

typedef struct Raindrop Raindrop;
typedef struct RainSplash RainSplash;

extern f32 g_RainSoundVolume;

void PrepareShotEffects(BallSimulation *simulation);
void SpawnImpactEffects(BallSimulation *simulation);
void SpawnShotDebris(CoursePoint point, Radians pitch, Radians yaw, s32 launchSpeed);
void StartPlayerRumble(Player *player);
void StartRingPassRumble(Player *player);
void LoadEffectImages(void);
void FreeEffectImages(void);
void ClearEffects(void);
void StartWeather(Weather weather);
void StopWeather(void);
void DrawBirdieBadgeCount(Gfx **displayListHead);
void SpawnRisingClumps(CoursePoint unusedPoint UNUSED);
void UpdateEffects(void);

void DrawEffects(CameraMatrices *matrices, Gfx **displayListHead);
void DrawRain(Gfx **displayListHead, CameraMatrices *matrices);
void SpawnDrop(CoursePoint point);
void ShowBirdieBadgeCount(s32 badgeCount);
void SpawnRipple(CoursePoint point);
void SpawnSparkle(CoursePoint point);
void SpawnSparkleNearPoint(u32 slotIndex, s32 pointId);
void SpawnSpreadingCircle(u32 slotIndex, s32 pointId);
void SpawnTear(u32 slotIndex, s32 pointId, Radians heading);

#endif
