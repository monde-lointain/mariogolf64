#ifndef EFFECTS_H
#define EFFECTS_H

#include <ultra64.h>
#include "ball_simulation.h"
#include "common.h"
#include "save.h"

/* StartWeather's weathers */
#define WEATHER_NONE (-1)
#define WEATHER_RAIN 2

/* The sound channel the rain plays on */
#define RAIN_SOUND_CHANNEL 9

void SpawnImpactEffects(BallSimulation *simulation);
void SpawnShotDebris(CoursePoint point, f32 pitch, f32 yaw, s32 launchSpeed);
void StartPlayerRumble(UnkB8 *player);
void StartRingPassRumble(UnkB8 *player);
void LoadEffectImages(void);
void FreeEffectImages(void);
void ClearEffects(void);
void StartWeather(s32 weather);
void EndWeather(void);

#endif
