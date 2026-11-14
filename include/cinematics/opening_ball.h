/*
 * The opening's copy of the ball simulation, and its loading of the other
 * characters.
 */

#ifndef OPENING_BALL_H
#define OPENING_BALL_H

#include <ultra64.h>

#include "shot/ball_simulation.h"
#include "system/c_language.h"

// The opening's own models, as indices into g_OtherCharacterModels. Each is placed on the tee as
// the name logo of its character is shown.
enum OpeningModel {
    OPENING_MODEL_MARIO = 3,
    OPENING_MODEL_YOSHI = 9,
    OPENING_MODEL_BABY_MARIO,
    OPENING_MODEL_DONKEY_KONG,
    OPENING_MODEL_PLUM,
    OPENING_MODEL_PEACH,
    OPENING_MODEL_LUIGI,
    // Toad, by its textures: a red spot on white, a face and a blue vest with yellow trim. Shown
    // without a name logo.
    OPENING_MODEL_TOAD = 16
};

typedef s32 OpeningModel;

// The opening's copies of the ball simulation's routines
s32 LaunchOpeningBallSimulation(
    BallSimulation *simulation,
    LaunchPurpose purpose,
    ShotLaunch *shot
);
StepFlags StepOpeningBallSimulation(BallSimulation *simulation);
Boolean ToggleOpeningCharacter(u32 slotIndex, OpeningModel model);

#endif
