#ifndef HOLE_CONDITIONS_H
#define HOLE_CONDITIONS_H

#include <math.h>
#include <ultra64.h>

#include "math/math_util.h"
#include "system/c_language.h"

typedef struct HoleConditions {
    s32 windSpeed;
    BinaryAngle windDirection;
    Boolean isRaining;
    u32 sceneColorIndex;
    BinaryAngle lightYaw;
    BinaryAngle lightPitch;
} HoleConditions;

#define SCENE_COLOR_INDEX_ROUND_START 0
#define SCENE_COLOR_INDEX_DRIVING_RANGE 6
#define SCENE_COLOR_INDEX_AFTERNOON 8
#define SCENE_COLOR_INDEX_ROUND_LATE 9
#define SCENE_COLOR_INDEX_ROUND_END 10
#define SCENE_COLOR_INDEX_SUDDEN_DEATH 11

#define WIND_SPEED_UNIT 0x10000
#define RANDOM_WIND_STEP_MASK 0x3F

#define WIND_DIRECTION_TO_RADIANS ((f32)(FULL_TURN_RADIANS / 0x10000))

#define ViewedWindAngle(windDirection, view) \
    (((windDirection) * WIND_DIRECTION_TO_RADIANS) + M_PI - \
        atan2( \
            (view)->eye[AXIS_Z] - (view)->lookAt[AXIS_Z], \
            (view)->eye[AXIS_X] - (view)->lookAt[AXIS_X] \
        ))

#endif
