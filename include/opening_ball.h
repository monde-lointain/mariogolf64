#ifndef OPENING_BALL_H
#define OPENING_BALL_H

#include <ultra64.h>

#include "ball_simulation.h"

s32 LaunchOpeningBallSimulation(BallSimulation *simulation, s32 purpose, ShotLaunch *shot);
s32 StepOpeningBallSimulation(BallSimulation *simulation);
s32 ToggleOpeningCharacter(u32 slotIndex, s32 character);

#endif
