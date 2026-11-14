/*
 * The small effects of play, the rain, the Rumble Paks' rumbles and the birdie
 * badge count.
 */

#ifndef PARTICLE_EFFECTS_H
#define PARTICLE_EFFECTS_H

#include <ultra64.h>

#include "camera/camera.h"
#include "course/course_space.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "round/player.h"
#include "shot/ball_simulation.h"
#include "shot/club.h"
#include "system/compiler.h"

// StartWeather's weathers
enum Weather {
    WEATHER_NONE = -1,
    WEATHER_RAIN = 2
};

typedef s32 Weather;

// The sound channel the rain plays on
#define RAIN_SOUND_CHANNEL 9
// The sound of the rain, played at full volume when a hole starts in the rain
#define RAIN_SOUND 0
#define RAIN_SOUND_START_VOLUME 100

enum EffectKind {
    EFFECT_KIND_NONE = -1,
    // The debris a shot throws up, in the ground's color, from SpawnHighDebris and SpawnLowDebris:
    // puffs from bunkers and bare ground, drawn lightened, of which SpawnImpactEffects' landing
    // puffs are high ones too; pieces of the rough, drawn with an alpha threshold; and low pieces
    // of grass, from fairways, semi-rough and the tee.
    EFFECT_KIND_HIGH_PUFF,
    EFFECT_KIND_LOW_PUFF,
    EFFECT_KIND_LOW_GRASS,
    EFFECT_KIND_HIGH_ROUGH,
    EFFECT_KIND_LOW_ROUGH,
    EFFECT_KIND_DROP, // Drawn with f_DropImage
    EFFECT_KIND_DIVOT, // Drawn with f_DivotImage
    EFFECT_KIND_DIVOT_MARK, // Drawn with f_DivotImage's last frame
    EFFECT_KIND_SPARKLE,
    EFFECT_KIND_RIPPLE = 0xC,
    EFFECT_KIND_SMOKE_PUFF = 0xE, // A dark speckled puff, drawn with f_SpeckleImage
    EFFECT_KIND_SPREADING_CIRCLE = 0x13, // Drawn with f_ArcImage
    EFFECT_KIND_RISING_CLUMP = 0x10, // Drawn with f_ClumpImage
    EFFECT_KIND_TEAR = 0x14
};

typedef s32 EffectKind;
typedef s8 EffectKind8;

// The particle effects drawn in the scene, such as the sparkles of a passed
// ring
typedef struct Effect {
    Vec3 position; // In world units
    Vec3 velocity; // In world units a frame
    u8 unusedAfterVelocity[0xC];
    f32 tilt; // An angle about x and z
    f32 alpha;
    f32 alphaStep;
    f32 scale;
    f32 scaleStep;
    s16 color; // A billboard effect's color, in RGBA16
    EffectKind8 kind;
    s8 frame; // Of the effect's animation
    s8 matrixIndex; // Of the effects' transform matrices
} Effect;

#define EFFECT_COUNT 40

// Set up for each shot's debris. Keeping the club kind in a struct is there to
// match, as it keeps its loads from moving above the effects' stores.
typedef struct ShotDebris {
    // SpawnShotDebris sets these three, with its own counts, as the debris
    // spawns; nothing reads them
    s32 ignoredSpawnCount; // Cleared
    s32 unusedAfterSpawnCount;
    f32 ignoredNegativeSetting; // Set to -2
    f32 ignoredPositiveSetting; // Set to 0.5
    ClubCategory clubCategory;
    s32 unusedAfterClubCategory;
} ShotDebris;

// The falling rain's drops, and the splashes they make on landing
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
