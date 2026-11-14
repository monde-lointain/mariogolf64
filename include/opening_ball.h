#ifndef OPENING_BALL_H
#define OPENING_BALL_H

#include <ultra64.h>

#include "ball_simulation.h"
#include "language.h"

enum OpeningModel {
    OPENING_MODEL_MARIO = 3,
    OPENING_MODEL_YOSHI = 9,
    OPENING_MODEL_BABY_MARIO,
    OPENING_MODEL_DONKEY_KONG,
    OPENING_MODEL_PLUM,
    OPENING_MODEL_PEACH,
    OPENING_MODEL_LUIGI,
    OPENING_MODEL_TOAD = 16
};

typedef s32 OpeningModel;

s32 LaunchOpeningBallSimulation(BallSimulation *simulation, s32 purpose, ShotLaunch *shot);
s32 StepOpeningBallSimulation(BallSimulation *simulation);
Boolean ToggleOpeningCharacter(u32 slotIndex, OpeningModel model);

#endif
