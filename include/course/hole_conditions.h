/*
 * The weather and light that a hole is played in: the wind, the rain and the
 * scene colors.
 */

#ifndef HOLE_CONDITIONS_H
#define HOLE_CONDITIONS_H

#include <math.h>
#include <ultra64.h>

#include "math/math_util.h"
#include "system/c_language.h"

// The weather and light that a hole is played in
typedef struct HoleConditions {
    s32 windSpeed; // As the ball's flight feels it
    BinaryAngle windDirection;
    // Shortens shots and changes the scene's colors
    Boolean isRaining;
    u32 sceneColorIndex; // With isRaining, picks DeriveSceneColors' colors
    BinaryAngle lightYaw;
    BinaryAngle lightPitch;
} HoleConditions;

// HoleConditions.sceneColorIndex values; over a round it rises from start to
// end
#define SCENE_COLOR_INDEX_ROUND_START 0
#define SCENE_COLOR_INDEX_DRIVING_RANGE 6
// Halfway from the driving range's midday light to the round end's
// sunset. A round's later holes reach it, and the staff roll's fairway
// shots set it.
#define SCENE_COLOR_INDEX_AFTERNOON 8
#define SCENE_COLOR_INDEX_ROUND_LATE 9
#define SCENE_COLOR_INDEX_ROUND_END 10
// Every hole of a sudden death, whatever its place in the round
#define SCENE_COLOR_INDEX_SUDDEN_DEATH 11

// HoleConditions.windSpeed is fixed point, with this as one unit
#define WIND_SPEED_UNIT 0x10000
#define RANDOM_WIND_STEP_MASK 0x3F

// HoleConditions.windDirection's units per radian
#define WIND_DIRECTION_TO_RADIANS ((f32)(FULL_TURN_RADIANS / 0x10000))

// The wind's direction as seen from a camera: the arrow turns with the
// camera's direction of view
#define ViewedWindAngle(windDirection, view) \
    (((windDirection) * WIND_DIRECTION_TO_RADIANS) + M_PI - \
        atan2( \
            (view)->eye[AXIS_Z] - (view)->lookAt[AXIS_Z], \
            (view)->eye[AXIS_X] - (view)->lookAt[AXIS_X] \
        ))

#endif
