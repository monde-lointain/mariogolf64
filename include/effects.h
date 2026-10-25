#ifndef EFFECTS_H
#define EFFECTS_H

#include <ultra64.h>
#include "ball_simulation.h"
#include "common.h"
#include "save.h"

void SpawnImpactEffects(BallSimulation *simulation);
void SpawnShotDebris(CoursePoint point, f32 pitch, f32 yaw, s32 launchSpeed);
void StartPlayerRumble(UnkB8 *player);
void StartRingPassRumble(UnkB8 *player);

#endif
