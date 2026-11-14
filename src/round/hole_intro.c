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
//
// Each arm of the test has a heading of its own and places the eye itself. A
// heading that lives in one arm lives in one basic block, so local-alloc
// gives it its register, $f20, which has to last across sinf, and ties the
// heading's double precision steps to it: the original builds each arm's
// heading in $f20 from the first conversion on. A heading shared by the arms
// is left to global-alloc, which gives it $f22, and its steps take $f0 and
// $f2.
//
// The look-at point is written and read as g_Cameras' member, with no pointer
// to it. cse's first pass gives the point's address a register and reaches Y
// and Z as offsets from it; its second pass writes the offsets as addresses
// again, except Z's, whose own register the else arm still read after the
// first pass. So the original keeps the point's address in $s0 for X and Z
// and stores Y by its address. With a pointer variable, Z goes by its address
// as well.
//
// Measured against the match, as words differing and how many of them in
// shape:
//
//   A heading declared in the function and set in each arm 44 (0)
//   The same, with the eye placed once after the arms      108 (16), frame +8
//   An inline function for the eye, given the heading      22 (10)
//   No heading variable, the sum written in each call      130 (54), frame -8
//   The heading as an f64 built in two statements          21 (3)
//   The else arm's heading as the product less (M_PI / 2)  3 (3)
//   The same as + (M_PI / 2) + -(f32)M_PI                  5 (5)
//   The same as + ((M_PI / 2) - M_PI)                      6 (6)
//   The eye's Y set first                                  49 (35), frame +8
//   The eye's Y set last                                   18 (16)
//   The eye's X as the look-at's less sinf's product       10 (8)
//   The eye's X with the distance negated, not sinf        30 (8), frame +8
//   The eye's products before the look-at coordinates      8 (0)
//   The eye worked out from the ball's position again      96 (30), frame -16
//   The look-at point's Z set before its Y                 24 (4)
//   The look-at point's Y set first                        42 (26)
//   A look-at pointer, for the stores or the reads         6 (2)
//   The same, with one f64 heading built in two statements
//   and the eye placed after the arms (the old draft)      33 (1)
//   The test as !=, with its arms swapped                  30 (30)
//
// Also matching: the heading as an f32 or an f64, or declared and then set; a
// heading of its own in one arm only, with the other's declared in the
// function; the eye's X offset in a local set first, with the eye's Y stored
// before its X; the scale in a variable; and the look-at point set before the
// intro's frame, distance and height.
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
        // Each arm has its own heading and places the eye itself, to match
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
        // As in the first hole's arm, to match
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
