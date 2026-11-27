/*
 * The tuning of the ball's flight, bounces and rolls, and of its launch,
 * which StepBallSimulation and LaunchBallSimulation share with the opening's
 * copies of them
 */

#ifndef BALL_PHYSICS_H
#define BALL_PHYSICS_H

#include <ultra64.h>

#include "audio/sound.h"
#include "course/course_space.h"
#include "shot/ball_simulation.h"

// The channels of a rolling ball's sound and of a ring passed through
#define ROLL_SOUND_CHANNEL 8
#define RING_SOUND_CHANNEL 14
// Hit sounds quieter than this are not played
#define HIT_SOUND_VOLUME_MIN 10
// The speeds of the ball's hits whose sounds play at SOUND_FULL_SCALE: most
// hits, a wall's and a lip-out's
#define FULL_VOLUME_HIT_SPEED KilometersPerHour(20)
#define FULL_VOLUME_WALL_HIT_SPEED KilometersPerHour(10)
#define FULL_VOLUME_LIP_OUT_SPEED KilometersPerHour(5)

// Distances from the cup's center, in course units; each is a whole number of
// centimeters, and the speeds are centimeters a frame
// 8 cm: nearer than this a slow ball drops in, and in the cup its center meets
// the wall this far out
#define CUP_RADIUS 1228.8f
// 7 cm a frame: a faster ball, or one rising faster than 5 km/h, does not drop
// in
#define CUP_IN_SPEED_MAX 1075.2f
#define CUP_IN_RISE_SPEED_MAX KilometersPerHour(5)
// 10.5 cm a frame: a ball that drops in slower plays SOUND_CUP_IN
#define CUP_IN_SOUND_SPEED_MAX 1612.7999f
// 7 cm: where the wall puts a ball that reaches it
#define CUP_WALL_REBOUND_RADIUS 1075.2f
// 7 cm and 13 cm: a rolling ball between them is pulled toward the cup, and
// a flying one landing there bounces off ground tilted toward it
#define CUP_PULL_INNER_RADIUS 1075.2f
#define CUP_PULL_OUTER_RADIUS 1996.7999f
// 4 cm: the pull and the lip-out act only on a ball within this height of the
// cup
#define CUP_PULL_HEIGHT_MAX 614.4f
// 4 cm: the pull rises and falls again over this much depth into its zone
#define CUP_PULL_DEPTH_RANGE 614.4f
// 2 cm and 3 mm a frame: how much the pull can rise above its least, and its
// least
#define CUP_PULL_RISE 307.2f
#define CUP_PULL_MIN 46.079998f
// The pull keeps this much of the ball's speed each frame
#define CUP_PULL_DRAG 0.93f
// The ball's direction gets this vertical part before it is measured against
// the way to the cup, so a slower ball counts as less aligned and feels more
// of the pull
#define CUP_PULL_DIRECTION_TILT 2096.0f
// A rolling ball leaving the pull's zone heading out within about 53 degrees
// of straight away lips out: it keeps the share of its speed along the ground
// that its speed is of LIP_OUT_FULL_SPEED (14 cm a frame), and turns the rest
// upward, with LIP_OUT_EXTRA_RISE of its speed more. Directions' dot product
// never reaches THROUGH_CUP_ALIGNMENT, so "Through Cup" is never printed.
#define LIP_OUT_ALIGNMENT_MIN 0.6f
// A ball that lips out spins at this share of its speed along the ground
#define LIP_OUT_SPIN_SCALE 0.5f
#define LIP_OUT_FULL_SPEED 2150.4f
#define LIP_OUT_EXTRA_RISE 0.1f
#define THROUGH_CUP_ALIGNMENT 17203.2f
// 60 cm: only a ball within this of the cup along both x and z is tested
// for dropping in or being pulled
#define CUP_SEARCH_SPAN 0x2400
// Just under 5 cm: a ball higher than this above the cup does not drop in
#define CUP_ENTRY_HEIGHT_MAX 0x2FF
// Where the debug text shows that a ball drops in or is pulled
#define CUP_DEBUG_TEXT_COLUMN 0x14
#define CUP_DEBUG_TEXT_ROW 0xA
// 20 cm: the depth of the cup's floor below the cup's position
#define CUP_DEPTH 0xC00
// 5 cm: a ball in the cup that rises faster than MIN_CUP_CEILING_SPEED above
// this depth is held to it and sent toward the cup's middle
#define CUP_CEILING_DEPTH 768.0f
#define CUP_CEILING_SPEED_MIN KilometersPerHour(5)
// The wall takes this much of the ball's speed toward it: all of it, and
// half again back
#define CUP_WALL_REFLECTION 1.5f
// The floor sends a ball back up at this much of its falling speed
#define CUP_FLOOR_BOUNCE 0.5f
// Slower hits on the cup's wall and floor play no sound
#define CUP_WALL_SOUND_SPEED_MIN KilometersPerHour(5)
#define CUP_FLOOR_SOUND_SPEED_MIN KilometersPerHour(2)

// Walls. A ball that hits one is put this share of the way along its step
// to the wall, short of it.
#define WALL_HIT_STEP_SHARE 0.9f
// 1.5 cm a frame: a ball already past a wall and slower into it is sent back
// this much faster, and keeps less of its speed the slower it was
#define WALL_ESCAPE_SPEED 230.40001f
// 1 cm a frame: a faster ball throws sparks off a wall
#define WALL_SPARK_SPEED_MIN 153.6f
// A ball that hits a hole triangle is put this share of the way to it
#define TRIANGLE_HIT_STEP_SHARE 0.995f
// A flying ball that hits a billboard has its spins divided by this and
// keeps this share of its speed along the ground; a trunk or other solid one
// turns it aside
#define BILLBOARD_HIT_SPIN_DIVISOR 2
#define BILLBOARD_HIT_SPEED_KEPT 0.5f
// A flying ball that hits the pin keeps this much of its spin, and a rolling
// one is thrown up at this share of its speed along the ground. Either moves
// on from the hit to this share of the step's length.
#define PIN_HIT_SPIN_KEPT 0.3f
#define PIN_HIT_RISE_SCALE 0.5f
#define PIN_HIT_STEP_SHARE 1.3f
// About 3.4 cm: in mini golf, a rolling ball also looks for the ground along
// a path whose end is raised this much, so it finds ground it rolls up onto
#define MINI_GOLF_ROLL_PROBE_RISE 0x20C
// Where the debug text shows what the ball's path hit
#define CONTACT_DEBUG_TEXT_COLUMN 2
#define CONTACT_DEBUG_TEXT_ROW 6

// A softer surface takes more of a bouncing ball's speed, more for a steeper
// landing, in thousandths at this scale
#define SOFTNESS_SPEED_LOSS_SCALE 0.1f
// Landing in the water, a ball skips off only if it is flying between these
// speeds into the water, and this share of its speed along the water is
// more. Otherwise it sinks, keeping the square of WATER_ENTRY_DRAG of its
// speed along x.
#define SKIP_IMPACT_SPEED_MIN KilometersPerHour(3)
#define SKIP_IMPACT_SPEED_MAX KilometersPerHour(100)
#define SKIP_SPEED_RATIO 0.4f
#define WATER_ENTRY_DRAG 0.2f
// A bounce within this share of a step's length, in its first substep, adds
// a second substep
#define EARLY_HIT_FRACTION 0.4f
// On the green, a hole triangle's normal is taken halfway to straight up
#define GREEN_TRIANGLE_LEVELING 0.5f
// On a bounce, the ball's velocity moves toward its spin's, by this share
// times the ground's flatness and, unless the ball starts rolling, the
// bounce's grip. The spin loses a tenth, and moves toward the ball's
// velocity before the substep by its own share.
#define TRIANGLE_SPIN_GRIP 0.25f
#define GROUND_SPIN_GRIP 0.5f
#define BOUNCE_SPIN_DECAY 0.9f
#define TRIANGLE_SPIN_FOLLOW 0.25f
#define GROUND_SPIN_FOLLOW 0.2f
// The spin's pull on a ball bouncing off the ground is no more than this
// share of the landing speed
#define SPIN_DRIFT_SHARE_MAX 0.4f

// Ground landings. Within the cup's pull, a ball falling steeply toward the
// cup lands on ground tilted to face it: the ground's upward part is less
// this much before it is normalized.
#define CUP_PULL_TILT 0.7f
#define CUP_PULL_SLIDE_RATIO_MAX 0.5f
// Off mini golf, a fast landing tilts the ground to one side or the other at
// random, by at least MIN_LANDING_TILT
#define LANDING_TILT_NOISE_SCALE 0.1f
#define LANDING_TILT_MIN 0.02f
// A slower ball takes a fixed slide along the ground
#define SLIDE_SPEED_MIN 0.0001f
#define STILL_BALL_SLIDE 0.1f

// How deep a ball that lands in a bunker is buried
enum BunkerBurial {
    BUNKER_BURIAL_NONE,
    BUNKER_BURIAL_HALF, // SURFACE_BUNKER_FAIR_LIE
    BUNKER_BURIAL_MOSTLY // SURFACE_BUNKER_POOR_LIE
};

// An enum BunkerBurial value
typedef s32 BunkerBurial;

// A ball landing in a bunker faster than these is half or mostly buried;
// the faster is about 70 km/h
#define FAIR_LIE_BUNKER_SPEED KilometersPerHour(50)
#define POOR_LIE_BUNKER_SPEED 9956.353f
// A slower ball landing on the step after a bounce starts rolling
#define ROLL_START_SPEED_MAX KilometersPerHour(50)
// A bouncing ball keeps at least this share of its speed along the ground;
// off mini golf, a quarter of it in the first steps
#define BOUNCE_SPEED_KEPT_MIN 0.1f
#define EARLY_BOUNCE_STEP_COUNT 2
// A flying ball can't hit a billboard until this many steps after a bounce
#define BILLBOARD_HIT_BOUNCE_GAP 2
// A ball that lands slower than this out of bounds or in the water stops
// there
#define LOST_BALL_STOP_SPEED KilometersPerHour(10)
#define EARLY_BOUNCE_SPEED_SCALE 0.25

// Rolling into the ground. A rolling ball pressing into it this fast, or in
// mini golf about 4.6 km/h, leaves along it; off mini golf, keeping a tenth
// of its speed.
#define MINI_GOLF_ROLL_JUMP_SPEED 655.0f
#define ROLL_JUMP_SPEED KilometersPerHour(50)
#define ROLL_JUMP_SPEED_KEPT 0.1f
// Off mini golf, the ground under a rolling ball tilts at random to one side,
// by up to a quarter of the share that the ball's speed is of
// ROLL_TILT_FULL_SPEED
#define ROLL_TILT_NOISE_SCALE 0.5f
#define ROLL_TILT_FULL_SPEED KilometersPerHour(20)
// The chances that a ball on the fairway, off mini golf, sits in a divot or
// a tuft, and in the rough deeper in the grass
#define FAIRWAY_POOR_LIE_CHANCE 0.25f
#define FAIRWAY_FAIR_LIE_CHANCE 0.25f
#define ROUGH_POOR_LIE_CHANCE 0.35f
// 30 cm: a ball in the cup, or holed out, may settle this far below the
// ground, deeper than the cup's floor
#define IN_CUP_FLOOR_DEPTH 0x1200
// 20 cm: only a rolling ball within this of the cup along both x and z
// settles into it
#define CUP_FLOOR_SEARCH_SPAN 0xC00
// 4 cm: a rolling ball this near the cup's middle settles to its floor
#define CUP_DROP_RADIUS 614.4f
// 4 cm: beyond the cup's edge the ground dips along a circle of this radius
// out to CUP_RIM_OUTER_RADIUS, sinking a rolling ball by half the circle's
// depth
#define CUP_RIM_CURVE_RADIUS 614.4f
#define CUP_RIM_OUTER_RADIUS (CUP_RADIUS + CUP_RIM_CURVE_RADIUS)
#define CUP_RIM_SINK_SCALE 0.5f
// 300 m: the ball goes no higher above the course's zero height
#define BALL_ALTITUDE_MAX 0x465000

// 3 m: a ball that has been in the water stops once it sinks this far below
// the ground or water's surface
#define WATER_STOP_DEPTH 0xB400
// The water keeps this much of the ball's velocity each frame
#define WATER_DRAG 0.7f

// 30 cm: off mini golf, a rolling ball that finds the ground this far above
// it goes back to where it was and takes off with this much of its velocity
#define BURIED_BALL_DEPTH 0x1200
#define BURIED_BALL_SPEED_SCALE 0.1f
// 2 cm: a rolling ball this far above the ground takes off
#define TAKE_OFF_HEIGHT 307.2f

// The rolling sound's volume is its full scale at 100 km/h, times one more
// than the frames rolled, up to ROLL_VOLUME_RAMP_FRAMES, and no more than
// MAX_ROLL_VOLUME
#define ROLL_VOLUME_PER_SPEED (SOUND_FULL_SCALE / KilometersPerHour(100))
#define ROLL_VOLUME_RAMP_FRAMES 5
#define ROLL_VOLUME_MAX 0x37

// The surface's ratings are in thousandths
#define THOUSANDTHS_PER_ONE 1000
#define ONE_PER_THOUSANDTH (1.0f / THOUSANDTHS_PER_ONE)

// Rolling. A surface gives the share of a rolling ball's speed it keeps each
// frame, kept less in the rain.
#define RAIN_FRICTION_SCALE 0.993f
// A slope of 45 degrees or more is steep, but only for this many steps
#define STEEP_SLOPE_STEP_LIMIT 800
// For its first frames rolling, a ball that has been in the water feels the
// slope three times as much, and counts as on a steep one
#define WATER_EXIT_ROLL_FRAMES 30
#define WATER_EXIT_SLOPE_SCALE 3
// A steep slope pulls twice as hard, and keeps at least 0.9 of the speed
#define STEEP_SLOPE_SCALE 2
#define STEEP_FRICTION_MIN 0.9f
// 2 cm a frame: a slower ball on a gentle slope feels it from an eighth as
// much, at rest, rising to all of it at this speed
#define SLOW_ROLL_SPEED 307.2f
#define SLOW_ROLL_SLOPE_SCALE_MIN 0.125f
#define SLOW_ROLL_SLOPE_SCALE_PER_SPEED ((1.0f - SLOW_ROLL_SLOPE_SCALE_MIN) / SLOW_ROLL_SPEED)
// On a gentle slope, a simulation that runs long keeps less of the speed
#define LONG_ROLL_STEP_COUNT 150
#define LONGER_ROLL_STEP_COUNT 400
#define LONGEST_ROLL_STEP_COUNT 600
#define LONG_ROLL_FRICTION_SCALE 0.99f
#define LONGER_ROLL_FRICTION_SCALE 0.9f
#define LONGEST_ROLL_FRICTION_SCALE 0.5f
// 1 mm a frame: a faster ball's slope changes by a fifth of its part against
// the ball's motion, times the slope's share of the speed, and a slower ball
// feels half the slope
#define SLOPE_STEER_SPEED_MIN 15.36f
#define SLOPE_STEER_SCALE 0.2f
#define STILL_BALL_SLOPE_SCALE 0.5f
// The slope's pull on the ball's velocity, before friction
#define SLOPE_PULL_SCALE 0.4f
// After its first frames rolling, a ball stops when slower than
// MAX_STOP_SPEED with the slope's pull, times friction, under
// MAX_STOP_SLOPE_PULL, off a steep slope. A ball that has been in the water
// stops after WATER_STOP_ROLL_FRAMES.
#define STOP_ROLL_FRAMES_MIN 16
#define WATER_STOP_ROLL_FRAMES 61
#define STOP_SPEED_MAX 30
#define STOP_SLOPE_PULL_MAX 500.0f
// Off the green and off a steep slope, a ball slower than this this many
// steps after its last bounce stops. It is 2 km/h, as 2% of 100 km/h: a
// double that is not KilometersPerHour(2) but a hundredth of twice the float
// KilometersPerHour(100), so both simulations compare the speed as a double.
#define OFF_GREEN_STOP_SPEED_MAX (KilometersPerHour(100) * 0.02)
#define OFF_GREEN_STOP_STEPS 90

// Landing. The surface's friction is scaled up with the landing speed, from
// a base, and varied by up to a tenth with noise. On the green and fairway
// the share of the ball's speed along the ground that it loses comes from
// the friction, within limits; elsewhere it is fixed.
#define LANDING_NOISE_SCALE 0.1f
#define LANDING_FRICTION_BASE 0.3f
#define GREEN_LANDING_FRICTION_BASE 0.6f
#define GREEN_LANDING_FRICTION_PER_SPEED (2.4f / KilometersPerHour(100))
#define GREEN_LANDING_FRICTION_MAX 0.8f
#define GREEN_LANDING_FRICTION_MIN 0.05f
#define FAIRWAY_LANDING_FRICTION_PER_SPEED (1.6f / KilometersPerHour(100))
#define OTHER_LANDING_FRICTION_PER_SPEED (1.0f / KilometersPerHour(100))
#define GRASS_LANDING_SPIN_MAX 0.01f
#define GRASS_LANDING_SPIN_MIN (-0.2f)
#define BUNKER_LANDING_SPIN 0.7f
#define ROUGH_LANDING_SPIN 0.4f
#define OTHER_LANDING_SPIN 0.2f
// The most of its speed into the ground that a landing ball bounces back
#define RESTITUTION_MAX 1.5f

// Flight. The air's drag grows from a least value with the root of the
// ball's speed through it, in half kilometers an hour.
#define AIR_DRAG_SPEED_SCALE (1.0f / KilometersPerHour(0.5f))
#define AIR_DRAG_PER_ROOT_SPEED 0.0011f
#define AIR_DRAG_MIN 0.004f
// Side spin curves the ball most when its scaled size matches the ball's
// speed along the ground, less the further they differ, and never by more
// than a hundredth of that speed a frame
#define SIDE_SPIN_SPEED_SCALE 8.0f
#define SIDE_SPIN_MISMATCH_FALLOFF 0.8f
#define CURVE_PER_SIDE_SPIN 0.038146973f
#define CURVE_SPEED_RATIO_MAX 100.0f
// Only a ball faster than this along the ground curves
#define CURVE_SPEED_MIN KilometersPerHour(10)
// Back spin lifts the ball by no more than an eightieth of its air speed
#define LIFT_PER_SPIN 0.045776367f
// The lift pushes the ball this far from its path through the air, turning
// about the spin's axis
#define LIFT_ANGLE_DEGREES 88.0f
#define LIFT_SPEED_RATIO_MAX 80.0f
// Only a ball faster than this through the air lifts
#define LIFT_AIR_SPEED_MIN KilometersPerHour(1)
// The share of each spin kept each frame in flight
#define SIDE_SPIN_DECAY 0.995f
#define SPIN_DECAY 0.993f
// The wind carries a ball no higher than where the shot started at half
// strength, gaining strength with height: full 40 yards up, and more above
#define LOW_WIND_SCALE 0.5f
#define WIND_SCALE_PER_HEIGHT (1.0f / (80 * UNITS_PER_YARD))
// The cup's sounds grow louder with the ball's speed, to their loudest at
// 1 km/h, which is slower than any speed that plays them
#define CUP_SOUND_VOLUME_PER_SPEED ((SOUND_FULL_SCALE - 1) / KilometersPerHour(1))
// The flagstick's sound grows ten times as fast, written to fewer places
#define FLAGSTICK_SOUND_VOLUME_PER_SPEED 8.858665f
// The wall's and flagstick's sounds play no louder than this
#define KNOCK_VOLUME_MAX (SOUND_FULL_SCALE - 1)

// Gravity pulls a rolling ball down a slope by up to this
#define SLOPE_ACCELERATION (GRAVITY * 3 / 4)
#define SLOPE_ACCELERATION_MAX (GRAVITY / 2)

// The launch speed is the club's power, scaled in 256ths by the impact's
// speedChange, in 128ths, and by the shot's power, then by the lie's
// percent. A test shot takes 99% of it, written in half percents.
#define LAUNCH_SCALE_ONE 0x100
#define SPEED_CHANGE_UNITS 128
#define ONE_PER_PERCENT (1.0f / 100)
#define CLUB_TEST_SPEED_HALF_PERCENTS 198
#define HALF_PERCENTS_PER_ONE 200
// A launch is at least this fast
#define LAUNCH_SPEED_MIN 10
// Off the putter, the impact point's offsetY raises the pitch by the club's
// offsetPitch degrees per this much, and scales the speed by the cosine of
// the pitch less offsetPitch, less LAUNCH_SCALE_COSINE_BASE, twice as much
// for a wood
#define OFFSET_Y_PER_PITCH_STEP 32
#define LAUNCH_SCALE_COSINE_BASE 0.86f
#define LAUNCH_SCALE_PER_OFFSET 0.008300781f
#define WOOD_LAUNCH_SCALE_FACTOR 2.0f
// Outside mini golf a putt takes the aim's slope into its pitch; other shots
// take a quarter of it, slowing with it. Off the putter, half the cross
// slope turns the yaw, and the root of a quarter of it adds side spin.
#define AIM_SLOPE_PITCH_DIVISOR 4
#define CROSS_SLOPE_YAW_DIVISOR 2
#define CROSS_SLOPE_SPIN_SCALE 0.25f
// The spins grow with the speed. The side spin is the club's, doubled, plus
// the mishit's and the impact point's; the back spin rises with offsetY
// from less half the club's spin.
#define SIDE_SPIN_PER_SPEED 0.00078125f
#define SPIN_PER_SPEED 0.0078125f
#define CLUB_SIDE_SPIN_SCALE 2
#define MISHIT_SIDE_SPIN_DIVISOR 64
#define OFFSET_SIDE_SPIN_PER_MISHIT 0.0078125f
#define CLUB_SPIN_BASE_SCALE 0.5f
#define CLUB_TEST_SPIN_OFFSET 25.0f
#define CLUB_TEST_SPIN_PER_OFFSET 0.006f
#define STROKE_SPIN_SCALE 0.8f
#define STROKE_SPIN_OFFSET 10.0f
#define STROKE_SPIN_PER_OFFSET 0.004f
// The rain slows the launch and takes some of its spins
#define RAIN_LAUNCH_SPEED_SCALE 0.95f
#define RAIN_SPIN_SCALE 0.6f
#define RAIN_SIDE_SPIN_SCALE 0.95f
#define SPIN_SIZE_SCALE 0.4f
// The wind's speed becomes its velocity at this scale. Out of the rain, the
// wind loses this share of its part along the shot's heading.
#define WIND_SPEED_TO_VELOCITY 0.0078125f
#define WIND_ALONG_SHOT_CUT 0.3f

// 0.1 mm: a launch starts at least this far above the ground
#define LAUNCH_GROUND_CLEARANCE 1.536f

// The rows of the launch values that GAME_FLAG_SHOW_LAUNCH_VALUES prints
enum LaunchValueRow {
    LAUNCH_VALUE_ROW_YAW = 6,
    LAUNCH_VALUE_ROW_PITCH,
    LAUNCH_VALUE_ROW_SPEED,
    LAUNCH_VALUE_ROW_SIDE_SPIN,
    LAUNCH_VALUE_ROW_SPIN,
    LAUNCH_VALUE_ROW_SPIN_RPM,
    LAUNCH_VALUE_ROW_SPIN_HEADING,
    LAUNCH_VALUE_ROW_SPIN_SPEED,
    LAUNCH_VALUE_ROW_SIDE_SPIN_SPEED
};

#define LAUNCH_VALUE_COLUMN 3

#endif
