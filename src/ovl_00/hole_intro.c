/*
 * The hole intro camera starts in front of the ball and swings round behind it
 * on the first hole; on later holes it starts behind the ball. Either way it
 * closes in and drops to the shot's camera.
 */
#include "common.h"

#include "hole_intro.h"

#include "camera.h"
#include "course.h"
#include "current_shot.h"
#include "debug_text.h"
#include "libc.h"
#include "math_util.h"
#include "movie.h"
#include "save.h"
#include "scoring.h"
#include "sound.h"
#include "text.h"

#define INTRO_CAMERA_START_DISTANCE (-768000.0f)
#define INTRO_CAMERA_START_HEIGHT (-153600.0f)
/* The look-at point sits this far below the ball */
#define INTRO_LOOK_AT_DROP 17664.0f
/* Before the intro moves, the camera is this far behind and above the ball */
#define INTRO_CAMERA_FIRST_DISTANCE (-300.0f)
#define INTRO_CAMERA_FIRST_HEIGHT (-0x1E000)

#define FIRST_HOLE_INTRO_FRAMES 90
#define FIRST_HOLE_INTRO_START_DISTANCE (-0x4B000)
#define FIRST_HOLE_INTRO_START_HEIGHT (-0x1E000)
/* The camera swings round by this many degrees each frame, up to half a turn */
#define FIRST_HOLE_INTRO_SWING_DEGREES 2
#define FIRST_HOLE_INTRO_MOVIE_FRAME 60

#define LATER_HOLE_INTRO_FRAMES 40
#define LATER_HOLE_INTRO_START_DISTANCE (-0x43800)
#define LATER_HOLE_INTRO_START_HEIGHT (-0x1A400)
#define LATER_HOLE_INTRO_MOVIE_FRAME 4
#define LATER_HOLE_INTRO_SOUND_FRAME 8
#define HOLE_INTRO_SOUND_CHANNEL 13

#define INTRO_CAMERA_END_DISTANCE (-0xF000)
#define INTRO_CAMERA_END_HEIGHT (-0x4E00)
#define INTRO_FRAME_COUNT_MAX 1000
#define INTRO_FRAME_TEXT_COLUMN 20
#define INTRO_FRAME_TEXT_ROW 2

/* g_GameState.holeIndex under a symbol of its own, which
 * UpdateHoleIntroCamera needs to match */
static s32 f_IntroFrame = 0;
/* How far in front of the ball the camera is; negative when behind it */
static f32 f_IntroCameraDistance = 0.0f;
/* How far above the ball the camera is; negative values are above */
static f32 f_IntroCameraHeight = 0.0f;

/*
 * Places the camera for the start of the hole intro. The heading is worked
 * out in double precision and narrowed once for sinf and cosf.
 *
 * Doesn't match: the original stores the look-at point's Z through its
 * pointer and its Y through the camera's address, where GCC stores both
 * through the camera's address. Storing Z before Y, or Y straight into
 * g_Cameras, doesn't help.
 */
#ifdef NON_MATCHING
void StartHoleIntroCamera(void) {
  Vec3* lookAt = &g_Cameras[1].view.lookAt;
  f64 heading;
  f32 offsetX;
  f32 scale = COURSE_TO_WORLD_SCALE;

  f_IntroFrame = 0;
  f_IntroCameraDistance = INTRO_CAMERA_START_DISTANCE;
  f_IntroCameraHeight = INTRO_CAMERA_START_HEIGHT;
  (*lookAt)[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X] * scale;
  (*lookAt)[AXIS_Y] =
      (g_CurrentShot.ballPosition[AXIS_Y] - INTRO_LOOK_AT_DROP) * scale;
  (*lookAt)[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z] * scale;
  if (g_GameState.holeIndex == GetFirstHoleIndex()) {
    heading =
        ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) + (M_PI / 2);
  } else {
    heading = ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) +
        (M_PI / 2) - M_PI;
  }

  offsetX = -sinf(heading) * INTRO_CAMERA_FIRST_DISTANCE;
  g_Cameras[0].view.eye[AXIS_Y] =
      (g_CurrentShot.ballPosition[AXIS_Y] + INTRO_CAMERA_FIRST_HEIGHT) * scale;
  g_Cameras[0].view.eye[AXIS_X] = (*lookAt)[AXIS_X] + offsetX;
  g_Cameras[0].view.eye[AXIS_Z] =
      (*lookAt)[AXIS_Z] + (cosf(heading) * INTRO_CAMERA_FIRST_DISTANCE);
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/hole_intro", StartHoleIntroCamera);
#endif

/* Moves the hole intro camera on by a frame. Returns TRUE once the intro is
 * over. */
s32 UpdateHoleIntroCamera(void) {
  Vec3 offset;
  Radians heading;

  if (f_IntroFrame < INTRO_FRAME_COUNT_MAX) {
    f_IntroFrame++;
  }

  if (g_GameState.holeIndex == GetFirstHoleIndex()) {
    if (f_IntroFrame < FIRST_HOLE_INTRO_FRAMES) {
      f_IntroCameraDistance = FIRST_HOLE_INTRO_START_DISTANCE -
          (f_IntroFrame *
              (FIRST_HOLE_INTRO_START_DISTANCE - INTRO_CAMERA_END_DISTANCE) /
              FIRST_HOLE_INTRO_FRAMES);
      f_IntroCameraHeight = FIRST_HOLE_INTRO_START_HEIGHT -
          (f_IntroFrame *
              (FIRST_HOLE_INTRO_START_HEIGHT - INTRO_CAMERA_END_HEIGHT) /
              FIRST_HOLE_INTRO_FRAMES);
    } else {
      f_IntroCameraDistance = INTRO_CAMERA_END_DISTANCE;
      f_IntroCameraHeight = INTRO_CAMERA_END_HEIGHT;
    }

    /* The swing comes first, then the shot's heading is added to it */
    heading =
        -(f32)f_IntroFrame * DegreesToRadians(FIRST_HOLE_INTRO_SWING_DEGREES);
    if (heading < -M_PI) {
      heading = -(f32)M_PI;
    }

    heading = heading +
        (((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) + (M_PI / 2));
  } else {
    if (f_IntroFrame < LATER_HOLE_INTRO_FRAMES) {
      f_IntroCameraDistance = LATER_HOLE_INTRO_START_DISTANCE -
          (f_IntroFrame *
              (LATER_HOLE_INTRO_START_DISTANCE - INTRO_CAMERA_END_DISTANCE) /
              LATER_HOLE_INTRO_FRAMES);
      f_IntroCameraHeight = LATER_HOLE_INTRO_START_HEIGHT -
          (f_IntroFrame *
              (LATER_HOLE_INTRO_START_HEIGHT - INTRO_CAMERA_END_HEIGHT) /
              LATER_HOLE_INTRO_FRAMES);
    } else {
      f_IntroCameraDistance = INTRO_CAMERA_END_DISTANCE;
      f_IntroCameraHeight = INTRO_CAMERA_END_HEIGHT;
    }

    heading = ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) +
        (M_PI / 2) + -(f32)M_PI;
  }

  g_Cameras[1].view.lookAt[AXIS_X] =
      g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
  g_Cameras[1].view.lookAt[AXIS_Y] =
      (g_CurrentShot.ballPosition[AXIS_Y] - INTRO_LOOK_AT_DROP) *
      COURSE_TO_WORLD_SCALE;
  g_Cameras[1].view.lookAt[AXIS_Z] =
      g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
  offset[AXIS_X] =
      -sinf(heading) * f_IntroCameraDistance * COURSE_TO_WORLD_SCALE;
  offset[AXIS_Y] = (g_CurrentShot.ballPosition[AXIS_Y] + f_IntroCameraHeight) *
      COURSE_TO_WORLD_SCALE;
  offset[AXIS_Z] =
      cosf(heading) * f_IntroCameraDistance * COURSE_TO_WORLD_SCALE;
  g_Cameras[0].view.eye[AXIS_Y] = offset[AXIS_Y];
  g_Cameras[0].view.eye[AXIS_X] =
      g_Cameras[1].view.lookAt[AXIS_X] + offset[AXIS_X];
  g_Cameras[0].view.eye[AXIS_Z] =
      g_Cameras[1].view.lookAt[AXIS_Z] + offset[AXIS_Z];
  if (g_GameState.holeIndex == GetFirstHoleIndex()) {
    if (f_IntroFrame == FIRST_HOLE_INTRO_MOVIE_FRAME) {
      StartMovie(MOVIE_LAKITU);
    }

    if (f_IntroFrame > FIRST_HOLE_INTRO_MOVIE_FRAME && IsMoviePastCue()) {
      return TRUE;
    }
  } else {
    if (f_IntroFrame == LATER_HOLE_INTRO_MOVIE_FRAME) {
      StartMovie(MOVIE_READY_GO);
    }

    if (f_IntroFrame == LATER_HOLE_INTRO_SOUND_FRAME) {
      PlaySound(SOUND_HOLE_INTRO, HOLE_INTRO_SOUND_CHANNEL, SOUND_FULL_SCALE);
    }

    if (f_IntroFrame == LATER_HOLE_INTRO_FRAMES) {
      return TRUE;
    }
  }

  sprintf(g_PrintBuffer, "%3d", (int)f_IntroFrame);
  PrintDebugText(g_PrintBuffer, INTRO_FRAME_TEXT_COLUMN, INTRO_FRAME_TEXT_ROW);
  return FALSE;
}
