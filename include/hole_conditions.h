#ifndef HOLE_CONDITIONS_H
#define HOLE_CONDITIONS_H

#include "common.h"

#include <ultra64.h>

#include "libc.h"
#include "math_util.h"

/* The weather and light that a hole is played in */
typedef struct HoleConditions {
  /* 0x00 */ s32 windSpeed; /* As the ball's flight feels it */
  /* 0x04 */ BinaryAngle windDirection;
  /* Shortens shots and changes the scene's colors */
  /* 0x08 */ Boolean isRaining;
  /* 0x0C */ u32
      sceneColorIndex; /* With isRaining, picks DeriveSceneColors' colors */
  /* 0x10 */ BinaryAngle lightYaw;
  /* 0x12 */ BinaryAngle lightPitch;
} HoleConditions; /* size = 0x14 */

/* HoleConditions.sceneColorIndex values; over a round it rises from start to
 * end */
#define SCENE_COLOR_INDEX_ROUND_START 0
#define SCENE_COLOR_INDEX_DRIVING_RANGE 6
/* Halfway from the driving range's midday light to the round end's
 * sunset. A round's later holes reach it, and the staff roll's fairway
 * shots set it. */
#define SCENE_COLOR_INDEX_AFTERNOON 8
#define SCENE_COLOR_INDEX_ROUND_LATE 9
#define SCENE_COLOR_INDEX_ROUND_END 10
/* Every hole of a sudden death, whatever its place in the round */
#define SCENE_COLOR_INDEX_SUDDEN_DEATH 11

/* HoleConditions.windSpeed is fixed point, with this as one unit */
#define WIND_SPEED_UNIT 0x10000

/* HoleConditions.windDirection's units per radian */
#define WIND_DIRECTION_TO_RADIANS ((f32)(2 * M_PI / 0x10000))

/*
 * The wind's direction as seen from a camera: the arrow turns with the
 * camera's direction of view
 */
#define ViewedWindAngle(windDirection, view) \
  (((windDirection) * WIND_DIRECTION_TO_RADIANS) + M_PI - \
      atan2((view)->eye[2] - (view)->lookAt[2], \
          (view)->eye[0] - (view)->lookAt[0]))

#endif
