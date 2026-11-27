/*
 * The hole intro camera starts in front of the ball and swings round behind it
 * on the first hole; on later holes it starts behind the ball. Either way it
 * closes in and drops to the shot's camera.
 */

#include "round/hole_intro.h"

#include <ultra64.h>

#include "audio/sound.h"
#include "camera/camera.h"
#include "course/course_space.h"
#include "debug/debug_text.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "round/game_state.h"
#include "round/movie.h"
#include "round/scoring.h"
#include "shot/current_shot.h"
#include "text/text.h"

#define INTRO_CAMERA_START_DISTANCE (-768000.0f)
#define INTRO_CAMERA_START_HEIGHT (-153600.0f)
// The look-at point sits this far below the ball
#define INTRO_LOOK_AT_DROP 17664.0f
// Before the intro moves, the camera is this far behind and above the ball
#define INTRO_CAMERA_FIRST_DISTANCE (-300.0f)
#define INTRO_CAMERA_FIRST_HEIGHT (-0x1E000)

#define FIRST_HOLE_INTRO_FRAMES 90
#define FIRST_HOLE_INTRO_START_DISTANCE (-0x4B000)
#define FIRST_HOLE_INTRO_START_HEIGHT (-0x1E000)
// The camera swings round by this many degrees each frame, up to half a turn
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

// Frames since the intro started, counting up to INTRO_FRAME_COUNT_MAX
static s32 f_IntroFrame = 0;
// How far in front of the ball the camera is; negative when behind it
static f32 f_IntroCameraDistance = 0.0f;
// How far above the ball the camera is; negative values are above
static f32 f_IntroCameraHeight = 0.0f;

// Places the camera for the start of the hole intro: the look-at point goes
// under the ball, and the eye behind the ball and above it, looking along the
// shot's heading on the first hole and against it on the others.
void StartHoleIntroCamera(void) {
    f_IntroFrame = 0;
    f_IntroCameraDistance = INTRO_CAMERA_START_DISTANCE;
    f_IntroCameraHeight = INTRO_CAMERA_START_HEIGHT;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] =
        g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] =
        (g_CurrentShot.ballPosition[AXIS_Y] - INTRO_LOOK_AT_DROP) * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] =
        g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    if (g_GameState.holeIndex == GetFirstHoleIndex()) {
        Radians heading = ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) + (M_PI / 2);

        g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] = g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] +
            (-sinf(heading) * INTRO_CAMERA_FIRST_DISTANCE);
        g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] =
            (g_CurrentShot.ballPosition[AXIS_Y] + INTRO_CAMERA_FIRST_HEIGHT) *
            COURSE_TO_WORLD_SCALE;
        g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] = g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] +
            (cosf(heading) * INTRO_CAMERA_FIRST_DISTANCE);
    }
    else {
        Radians heading =
            ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) + (M_PI / 2) - M_PI;

        g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] = g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] +
            (-sinf(heading) * INTRO_CAMERA_FIRST_DISTANCE);
        g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] =
            (g_CurrentShot.ballPosition[AXIS_Y] + INTRO_CAMERA_FIRST_HEIGHT) *
            COURSE_TO_WORLD_SCALE;
        g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] = g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] +
            (cosf(heading) * INTRO_CAMERA_FIRST_DISTANCE);
    }
}

// Moves the hole intro camera on by a frame. Returns TRUE once the intro is
// over.
Boolean UpdateHoleIntroCamera(void) {
    Vec3 offset;
    Radians heading;

    if (f_IntroFrame < INTRO_FRAME_COUNT_MAX) {
        f_IntroFrame++;
    }

    if (g_GameState.holeIndex == GetFirstHoleIndex()) {
        if (f_IntroFrame < FIRST_HOLE_INTRO_FRAMES) {
            f_IntroCameraDistance = FIRST_HOLE_INTRO_START_DISTANCE -
                (f_IntroFrame * (FIRST_HOLE_INTRO_START_DISTANCE - INTRO_CAMERA_END_DISTANCE) /
                    FIRST_HOLE_INTRO_FRAMES);
            f_IntroCameraHeight = FIRST_HOLE_INTRO_START_HEIGHT -
                (f_IntroFrame * (FIRST_HOLE_INTRO_START_HEIGHT - INTRO_CAMERA_END_HEIGHT) /
                    FIRST_HOLE_INTRO_FRAMES);
        }
        else {
            f_IntroCameraDistance = INTRO_CAMERA_END_DISTANCE;
            f_IntroCameraHeight = INTRO_CAMERA_END_HEIGHT;
        }

        // The swing comes first, then the shot's heading is added to it
        heading = -(f32)f_IntroFrame * DegreesToRadians(FIRST_HOLE_INTRO_SWING_DEGREES);
        if (heading < -M_PI) {
            heading = -(f32)M_PI;
        }

        heading = heading + (((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) + (M_PI / 2));
    }
    else {
        if (f_IntroFrame < LATER_HOLE_INTRO_FRAMES) {
            f_IntroCameraDistance = LATER_HOLE_INTRO_START_DISTANCE -
                (f_IntroFrame * (LATER_HOLE_INTRO_START_DISTANCE - INTRO_CAMERA_END_DISTANCE) /
                    LATER_HOLE_INTRO_FRAMES);
            f_IntroCameraHeight = LATER_HOLE_INTRO_START_HEIGHT -
                (f_IntroFrame * (LATER_HOLE_INTRO_START_HEIGHT - INTRO_CAMERA_END_HEIGHT) /
                    LATER_HOLE_INTRO_FRAMES);
        }
        else {
            f_IntroCameraDistance = INTRO_CAMERA_END_DISTANCE;
            f_IntroCameraHeight = INTRO_CAMERA_END_HEIGHT;
        }

        heading = ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) + (M_PI / 2) + -(f32)M_PI;
    }

    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] =
        g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] =
        (g_CurrentShot.ballPosition[AXIS_Y] - INTRO_LOOK_AT_DROP) * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] =
        g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    offset[AXIS_X] = -sinf(heading) * f_IntroCameraDistance * COURSE_TO_WORLD_SCALE;
    offset[AXIS_Y] =
        (g_CurrentShot.ballPosition[AXIS_Y] + f_IntroCameraHeight) * COURSE_TO_WORLD_SCALE;
    offset[AXIS_Z] = cosf(heading) * f_IntroCameraDistance * COURSE_TO_WORLD_SCALE;
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] = offset[AXIS_Y];
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] =
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] + offset[AXIS_X];
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] =
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] + offset[AXIS_Z];
    if (g_GameState.holeIndex == GetFirstHoleIndex()) {
        if (f_IntroFrame == FIRST_HOLE_INTRO_MOVIE_FRAME) {
            StartMovie(MOVIE_LAKITU);
        }

        if (f_IntroFrame > FIRST_HOLE_INTRO_MOVIE_FRAME && IsMoviePastCue()) {
            return TRUE;
        }
    }
    else {
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
