/*
 * After a hole, the golfer reacts to the score with an animation. These
 * routines frame the golfer for it, cue its sounds, and pick a flat spot
 * for it to play on.
 */

#include "characters/reaction.h"

#include <math.h>
#include <ultra64.h>

#include "audio/sound.h"
#include "camera/camera.h"
#include "characters/character.h"
#include "characters/character_model.h"
#include "course/billboard.h"
#include "course/course_space.h"
#include "course/hole_triangles.h"
#include "course/pin.h"
#include "course/terrain.h"
#include "debug/debug_text.h"
#include "debug/game_flags.h"
#include "game/scene.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "round/game_state.h"
#include "screen/display_list.h"
#include "shot/current_shot.h"
#include "shot/particle_effects.h"
#include "system/c_language.h"
#include "system/compiler.h"

// Reaction sounds take turns on the channels from the first up to the last
#define FIRST_REACTION_CHANNEL 4
#define LAST_REACTION_CHANNEL 7
#define NO_REACTION_SOUND (-1)

// The camera keeps this many course units above the ground
#define CAMERA_GROUND_CLEARANCE 0x1E00

// ConsiderReactionSpot samples the ground on rings around the spot
#define SPOT_RING_COUNT 4
#define SPOT_RING_SPACING 15360.0f
#define SPOT_SAMPLES_PER_RING 16
// A sixteenth of a turn
#define SPOT_SAMPLE_ANGLE 0.3926991f
// A spot this near a billboard's edge, in course units, counts as rough
#define SPOT_BILLBOARD_CLEARANCE 0x12C00
#define NEAR_BILLBOARD_ROUGHNESS 0x465000
// A new spot replaces the last only if it is less than half as rough, and
// the last is rougher than this
#define SPOT_SMOOTH_ENOUGH_ROUGHNESS 0xB400

// The framings look at this point of the character
#define REACTION_FOCUS_POINT 1
// The camera closes in from the start distance to the end one over the frames
#define REACTION_START_DISTANCE 76800.0f
#define REACTION_END_DISTANCE 30720.0f
#define REACTION_ZOOM_RATE 0.025f
#define REACTION_EASE_RATE 0.1f
// Kid's camera turns this far a frame, a sixty-fourth of a turn
#define KID_TURN_STEP 0.049087387f
// Sherry's eagle effects run for this many frames from this one
#define SHERRY_EFFECT_START_FRAME 15
#define SHERRY_EFFECT_FRAME_COUNT 37
// Baby Mario's camera keeps higher above the ground, and his bogey effects
// start at this frame
#define BABY_MARIO_GROUND_CLEARANCE 0x2400
#define BABY_MARIO_EFFECT_START_FRAME 44
#define WARIO_EFFECT_START_FRAME 20
#define MARIO_EFFECT_START_FRAME 10
// The length of a light direction, the largest an s8 holds
#define LIGHT_DIRECTION_LENGTH 127.0f
// The eye is this much below the focus, where heights grow downward
#define REACTION_EYE_DROP 1.5f

// The framings' own distances, in course units, and rates. The camera of
// most closes in from FAR_REACTION_START_DISTANCE.
#define FAR_REACTION_START_DISTANCE 107520.0f
#define MARIO_END_DISTANCE 38400.0f
#define WARIO_START_DISTANCE 92160.0f
#define WARIO_END_DISTANCE 38400.0f
#define WARIO_EAGLE_END_DISTANCE 46080.0f
#define WARIO_BIRDIE_END_DISTANCE 23040.0f
#define BABY_MARIO_START_DISTANCE 61440.0f
#define BABY_MARIO_ZOOM_RATE 0.0125f
#define SHERRY_END_DISTANCE 46080.0f
#define DONKEY_KONG_EASE_RATE 0.08f
#define KID_EASE_RATE 0.3f

// The framings' own heights, in world units, where heights grow downward:
// a rise is subtracted and a drop added. An eye's rise is over the focus.
#define MARIO_EAGLE_FOCUS_RISE 4.5f
#define MARIO_EYE_RISE 1.5f
#define MARIO_PAR_EYE_RISE 7.5f
// Wario's focus is his shadow's point, raised to his body
#define WARIO_FOCUS_ABOVE_SHADOW 15.0f
#define WARIO_BIRDIE_FOCUS_DROP 1.5f
#define WARIO_EYE_RISE 4.5f
#define WARIO_BIRDIE_EYE_RISE 9.0f
#define YOSHI_EYE_RISE 15.0f
#define BOWSER_EAGLE_EYE_RISE 15.0f
#define AZALEA_FOCUS_DROP 3.0f
#define AZALEA_EYE_RISE 7.5f
#define PEACH_FOCUS_DROP 4.5f

// How far the low views of IsReactionViewLow raise what the camera looks at
// and drop the eye, by default and for some characters
#define LOW_VIEW_LOOK_AT_RISE 4.5f
#define LOW_VIEW_EYE_DROP 4.5f
#define LOW_VIEW_SHORT_EYE_DROP 3.0f
#define YOSHI_LOW_VIEW_SHIFT 7.5f
#define BOWSER_EAGLE_LOW_VIEW_LOOK_AT_RISE 7.5f
#define BOWSER_EAGLE_LOW_VIEW_EYE_DROP 9.0f
#define BOWSER_LOW_VIEW_SHIFT 3.0f

// Most reaction effects start from this character point
#define REACTION_EFFECT_POINT 3
// Yoshi's eagle sparkles on every third frame
#define YOSHI_SPARKLE_INTERVAL 3
// Each frame, Sherry sparkles at one point of each of two runs of points,
// which start at these
#define SHERRY_SPARKLE_RUN_LENGTH 3
#define SHERRY_FIRST_SPARKLE_POINT 3
#define SHERRY_SECOND_SPARKLE_POINT 6
// Baby Mario's tears fall from these points, to either side, on alternate
// frames
#define EVEN_FRAME_TEAR_POINT 3
#define ODD_FRAME_TEAR_POINT 4
// A circle spreads, with its sound, at these frames of Bowser's bogey
#define BOWSER_FIRST_CIRCLE_FRAME 10
#define BOWSER_SECOND_CIRCLE_FRAME 24
#define BOWSER_THIRD_CIRCLE_FRAME 33

// PlaceAtReactionSpot stands the character this far from the spot
#define SPOT_PLACE_DISTANCE 13824.0f
#define PITCH_REFINEMENT_COUNT 2

// Where UpdateReaction prints the frame number
#define REACTION_FRAME_COLUMN 3
#define REACTION_FRAME_ROW 4

// Frames since the reaction began
static s32 f_ReactionFrame = 0;
static s32 f_UnusedReactionWord UNUSED = 0; // Unused; it is there to match

// The third light is set by the framing routines that light the golfer
static Lights3 f_ReactionLights = gdSPDefLights3(
    0x60,
    0x60,
    0x60,
    0xFF,
    0xDC,
    0xDC,
    0x00,
    0x88,
    0x78,
    0xFF,
    0xDC,
    0xDC,
    0x00,
    0x88,
    0x78,
    0xFF,
    0xDC,
    0xDC,
    0x00,
    0x88,
    0x78
);

static SoundChannel f_ReactionChannel = FIRST_REACTION_CHANNEL;

static CharacterIndex f_ReactionCharacter;
static ReactionAnimation f_ReactionAnimation;
static s32 f_ReactionFrameCount;
static Boolean f_HasReactionLight;

static u8 f_UnusedBufferAfterReactionLight[24] UNUSED;

static EasedView f_EasedView;

static u8 f_UnusedBytesAfterEasedView[4] UNUSED;

static CoursePoint f_ReactionSpot;
static s32 f_ReactionSpotRoughness;

static u8 f_UnusedBufferAfterSpotRoughness[32] UNUSED;

static Boolean8 f_HasReactionSpot;

// Whether the reaction views drop the eye and raise what they look at.
// It is a macro, as an inline function compiles differently from the
// original.
#define IsReactionViewLow() \
    (IsGameFlagSet(GAME_FLAG_LOW_REACTION_VIEWS) || \
        ((g_Scene != SCENE_STAFF_ROLL) && (g_GameState.isSuddenDeath == TRUE)))

// Whether a spot this rough should become the reaction spot: it does when
// there is none yet, or when it is less than half as rough as one that is
// not smooth enough. It is a macro, as an inline function compiles
// differently from the original.
#define IsBetterReactionSpot(roughness) \
    (!f_HasReactionSpot || \
        (((roughness) * 2 < f_ReactionSpotRoughness) && \
            (f_ReactionSpotRoughness > SPOT_SMOOTH_ENOUGH_ROUGHNESS)))

// Blends from one value to another by the weight, limited to 0 to 1
f32 BlendClamped(f32 from, f32 to, f32 weight) {
    if (weight < 0.0f) {
        weight = 0.0f;
    }

    if (weight > 1.0f) {
        weight = 1.0f;
    }

    return (from * (1.0f - weight)) + (to * weight);
}

// Moves the eye and look-at points the rate of the way from where they were
// last eased to toward where they are asked to be, for the parts easedParts
// names. The first call for a part starts it where it is asked to be.
void EaseReactionView(f32 *eye, f32 *lookAt, f32 rate, s32 easedParts) {
    if (easedParts & EASE_EYE) {
        if (!f_EasedView.isEyeEased) {
            f_EasedView.eye[AXIS_X] = eye[AXIS_X];
            f_EasedView.eye[AXIS_Y] = eye[AXIS_Y];
            f_EasedView.eye[AXIS_Z] = eye[AXIS_Z];
            f_EasedView.isEyeEased = TRUE;
        }

        eye[AXIS_X] = BlendClamped(f_EasedView.eye[AXIS_X], eye[AXIS_X], rate);
        f_EasedView.eye[AXIS_X] = eye[AXIS_X];
        eye[AXIS_Y] = BlendClamped(f_EasedView.eye[AXIS_Y], eye[AXIS_Y], rate);
        f_EasedView.eye[AXIS_Y] = eye[AXIS_Y];
        eye[AXIS_Z] = BlendClamped(f_EasedView.eye[AXIS_Z], eye[AXIS_Z], rate);
        f_EasedView.eye[AXIS_Z] = eye[AXIS_Z];
    }

    if (easedParts & EASE_LOOK_AT) {
        if (!f_EasedView.isLookAtEased) {
            f_EasedView.lookAt[AXIS_X] = lookAt[AXIS_X];
            f_EasedView.lookAt[AXIS_Y] = lookAt[AXIS_Y];
            f_EasedView.lookAt[AXIS_Z] = lookAt[AXIS_Z];
            f_EasedView.isLookAtEased = TRUE;
        }

        lookAt[AXIS_X] = BlendClamped(f_EasedView.lookAt[AXIS_X], lookAt[AXIS_X], rate);
        f_EasedView.lookAt[AXIS_X] = lookAt[AXIS_X];
        lookAt[AXIS_Y] = BlendClamped(f_EasedView.lookAt[AXIS_Y], lookAt[AXIS_Y], rate);
        f_EasedView.lookAt[AXIS_Y] = lookAt[AXIS_Y];
        lookAt[AXIS_Z] = BlendClamped(f_EasedView.lookAt[AXIS_Z], lookAt[AXIS_Z], rate);
        f_EasedView.lookAt[AXIS_Z] = lookAt[AXIS_Z];
    }
}

// Sets the character's two lights and the reaction's third light, if one is
// on. It returns whether it set them.
Boolean AddReactionLight(Gfx **displayListHead, Lights2 *lights) {
    Gfx *gfx = *displayListHead;

    if (f_HasReactionLight) {
        f_ReactionLights.l[0] = lights->l[0];
        f_ReactionLights.l[1] = lights->l[1];
        gSPSetLights3(gfx++, f_ReactionLights);
        *displayListHead = gfx;
        return TRUE;
    }

    *displayListHead = gfx;
    return FALSE;
}

// Starts the character's reaction in the slot with the animation
void StartReaction(u32 slotIndex, CharacterIndex characterIndex, ReactionAnimation animation) {
    f_ReactionCharacter = characterIndex;
    f_HasReactionLight = FALSE;
    f_ReactionFrame = 0;
    f_EasedView.isEyeEased = FALSE;
    f_EasedView.isLookAtEased = FALSE;
    f_ReactionAnimation = animation;
    f_ReactionFrameCount = GetAnimationFrameCount(slotIndex, animation);
}

void TurnOffReactionLight(void) {
    f_HasReactionLight = FALSE;
}

// Plays the sound if this is the frame after the given one
void CueReactionSound(s32 frame, SoundId soundId) {
    if (soundId != NO_REACTION_SOUND && f_ReactionFrame - 1 == frame) {
        PlaySound(soundId, f_ReactionChannel, SOUND_FULL_SCALE);
        f_ReactionChannel++;
        if (f_ReactionChannel > LAST_REACTION_CHANNEL) {
            f_ReactionChannel = FIRST_REACTION_CHANNEL;
        }
    }
}

// Cues the sounds of the reaction, by character and animation
NO_RESULT PlayReactionSounds(void) {
    if (g_Scene == SCENE_STAFF_ROLL) {
        return;
    }

    switch (f_ReactionCharacter) {
        case CHARACTER_PLUM:
            switch (f_ReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(8, SOUND_PLUM_EAGLE);
                    CueReactionSound(62, SOUND_SAVED);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(32, SOUND_PLUM_BIRDIE);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(30, SOUND_AMBIENT_RANDOM_FIRST);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(25, SOUND_PLUM_BOGEY);
                    break;
            }

            break;
        case CHARACTER_CHARLIE:
            switch (f_ReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(0, SOUND_CHARLIE_EAGLE);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(0, SOUND_CHARLIE_BIRDIE);
                    CueReactionSound(10, SOUND_CHARLIE_BIRDIE_EFFECT);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(10, SOUND_CHARLIE_PAR);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(34, SOUND_CHARLIE_BOGEY);
                    break;
            }

            break;
        case CHARACTER_MAPLE:
            switch (f_ReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(44, SOUND_MAPLE_EAGLE);
                    CueReactionSound(92, SOUND_MAPLE_EAGLE_EFFECT);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(12, SOUND_MAPLE_BIRDIE);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(12, SOUND_MAPLE_PAR);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(8, SOUND_MAPLE_BOGEY);
                    break;
            }

            break;
        case CHARACTER_BABY_MARIO:
            switch (f_ReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(15, SOUND_BABY_MARIO_EAGLE);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(9, SOUND_BABY_MARIO_PAR);
                    CueReactionSound(57, SOUND_BABY_MARIO_BIRDIE);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(32, SOUND_BABY_MARIO_PAR);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(40, SOUND_BABY_MARIO_BOGEY);
                    break;
            }

            break;
        case CHARACTER_LUIGI:
            switch (f_ReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(2, SOUND_LUIGI_EAGLE);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(0, SOUND_LUIGI_BIRDIE);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(18, SOUND_LUIGI_PAR);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(7, SOUND_LUIGI_BOGEY);
                    break;
            }

            break;
        case CHARACTER_PEACH:
            switch (f_ReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(0, SOUND_PEACH_EAGLE);
                    CueReactionSound(56, SOUND_SAVED);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(6, SOUND_PEACH_BIRDIE);
                    CueReactionSound(6, SOUND_PEACH_BIRDIE_EFFECT);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(12, SOUND_PEACH_PAR);
                    CueReactionSound(42, SOUND_SAVED);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(16, SOUND_PEACH_BOGEY);
                    break;
            }

            break;
        case CHARACTER_SONNY:
            switch (f_ReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(14, SOUND_SONNY_EAGLE);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(2, SOUND_SONNY_BIRDIE);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(7, SOUND_SONNY_PAR);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(0, SOUND_SONNY_BOGEY);
                    break;
            }

            break;
        case CHARACTER_WARIO:
            switch (f_ReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(0, SOUND_WARIO_EAGLE);
                    CueReactionSound(21, SOUND_PEACH_BIRDIE_EFFECT);
                    CueReactionSound(88, SOUND_SAVED);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(0, SOUND_WARIO_BIRDIE);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(17, SOUND_WARIO_PAR);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(0, SOUND_WARIO_BOGEY);
                    CueReactionSound(28, SOUND_WARIO_BOGEY_EFFECT);
                    break;
            }

            break;
        case CHARACTER_HARRY:
            switch (f_ReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(20, SOUND_HARRY_EAGLE);
                    CueReactionSound(55, SOUND_SAVED);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(27, SOUND_HARRY_BIRDIE);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(24, SOUND_HARRY_PAR);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(5, SOUND_HARRY_BOGEY);
                    break;
            }

            break;
        case CHARACTER_MARIO:
        case CHARACTER_METAL_MARIO:
            switch (f_ReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(0, SOUND_METAL_MARIO_EAGLE);
                    CueReactionSound(0, SOUND_PEACH_BIRDIE_EFFECT);
                    CueReactionSound(68, SOUND_METAL_MARIO_EAGLE_EFFECT);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(0, SOUND_METAL_MARIO_BIRDIE);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(13, SOUND_METAL_MARIO_PAR);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(26, SOUND_METAL_MARIO_BOGEY);
                    break;
            }

            break;
        case CHARACTER_YOSHI:
            switch (f_ReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(66, SOUND_YOSHI_EAGLE);
                    CueReactionSound(38, SOUND_YOSHI_EAGLE_EFFECT);
                    CueReactionSound(38, SOUND_YOSHI_EAGLE_SECOND_EFFECT);
                    CueReactionSound(3, SOUND_YOSHI_REPEATED);
                    CueReactionSound(11, SOUND_YOSHI_REPEATED);
                    CueReactionSound(18, SOUND_YOSHI_REPEATED);
                    CueReactionSound(25, SOUND_YOSHI_REPEATED);
                    CueReactionSound(32, SOUND_YOSHI_REPEATED);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(40, SOUND_YOSHI_BIRDIE);
                    CueReactionSound(20, SOUND_YOSHI_REPEATED);
                    CueReactionSound(38, SOUND_YOSHI_REPEATED);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(28, SOUND_YOSHI_PAR);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(0, SOUND_YOSHI_BOGEY);
                    break;
            }

            break;
        case CHARACTER_DONKEY_KONG:
            switch (f_ReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(7, SOUND_DONKEY_KONG_EAGLE_EFFECT);
                    CueReactionSound(30, SOUND_DONKEY_KONG_EAGLE);
                    CueReactionSound(35, SOUND_DONKEY_KONG_REPEATED);
                    CueReactionSound(40, SOUND_DONKEY_KONG_REPEATED);
                    CueReactionSound(45, SOUND_DONKEY_KONG_REPEATED);
                    CueReactionSound(50, SOUND_DONKEY_KONG_REPEATED);
                    CueReactionSound(55, SOUND_DONKEY_KONG_REPEATED);
                    CueReactionSound(60, SOUND_DONKEY_KONG_REPEATED);
                    CueReactionSound(70, SOUND_DONKEY_KONG_BIRDIE);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(35, SOUND_DONKEY_KONG_BIRDIE);
                    CueReactionSound(22, SOUND_DONKEY_KONG_REPEATED);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(35, SOUND_DONKEY_KONG_BIRDIE);
                    CueReactionSound(17, SOUND_DONKEY_KONG_REPEATED);
                    CueReactionSound(31, SOUND_DONKEY_KONG_REPEATED);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(0, SOUND_DONKEY_KONG_BOGEY);
                    break;
            }

            break;
        case CHARACTER_BOWSER:
            switch (f_ReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(29, SOUND_BOWSER_EAGLE);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(42, SOUND_BOWSER_BIRDIE);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(0, SOUND_BOWSER_PAR);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(0, SOUND_BOWSER_BOGEY);
                    CueReactionSound(BOWSER_FIRST_CIRCLE_FRAME, SOUND_SPREADING_CIRCLE);
                    CueReactionSound(BOWSER_SECOND_CIRCLE_FRAME, SOUND_SPREADING_CIRCLE);
                    CueReactionSound(BOWSER_THIRD_CIRCLE_FRAME, SOUND_SPREADING_CIRCLE);
                    break;
            }

            break;
        case CHARACTER_KID:
            switch (f_ReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(0, SOUND_KID_EAGLE);
                    CueReactionSound(66, SOUND_KID_BIRDIE);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(0, SOUND_KID_BIRDIE);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(32, SOUND_KID_PAR);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(25, SOUND_KID_BOGEY);
                    break;
            }

            break;
        case CHARACTER_SHERRY:
            switch (f_ReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(0, SOUND_SHERRY_EAGLE);
                    CueReactionSound(6, SOUND_PEACH_BIRDIE_EFFECT);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(20, SOUND_SHERRY_BIRDIE);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(0, SOUND_SHERRY_PAR);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(24, SOUND_SHERRY_BOGEY);
                    break;
            }

            break;
        case CHARACTER_AZALEA:
            switch (f_ReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(25, SOUND_AZALEA_EAGLE);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(5, SOUND_AZALEA_BIRDIE);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(38, SOUND_AZALEA_PAR);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(0, SOUND_AZALEA_BOGEY);
                    break;
            }

            break;
        case CHARACTER_JOE:
            switch (f_ReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(15, SOUND_JOE_EAGLE);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(6, SOUND_JOE_BIRDIE);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(0, SOUND_JOE_PAR);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(0, SOUND_JOE_BOGEY);
                    break;
            }

            break;
    }
}

// Keeps the point CAMERA_GROUND_CLEARANCE above the ground, where heights grow
// downward
void KeepAboveGround(f32 *point) {
    f32 highestHeight = (GetGroundHeight(
                             point[AXIS_X] * WORLD_TO_COURSE_SCALE,
                             point[AXIS_Z] * WORLD_TO_COURSE_SCALE
                         ) - CAMERA_GROUND_CLEARANCE) *
        COURSE_TO_WORLD_SCALE;

    if (highestHeight < point[AXIS_Y]) {
        point[AXIS_Y] = highestHeight;
    }
}

// Frames Mario from farther away and, late in the eagle animation, lights
// him in yellow along the line from his focus point to his third point
void FrameMarioReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance;
    f32 focus[AXIS_COUNT];
    // Pads the frame to match; without it 66 words differ
    u8 unusedBuffer[0x40] UNUSED;
    f32 lightDirection[AXIS_COUNT];
    f32 lightOrigin[AXIS_COUNT];
    f32 length;
    f32 highestHeight;

    if (f_ReactionAnimation == REACTION_ANIMATION_PAR) {
        distance = BlendClamped(
            FAR_REACTION_START_DISTANCE,
            REACTION_END_DISTANCE,
            f_ReactionFrame * REACTION_ZOOM_RATE
        );
    }
    else {
        distance = BlendClamped(
            FAR_REACTION_START_DISTANCE,
            MARIO_END_DISTANCE,
            f_ReactionFrame * REACTION_ZOOM_RATE
        );
    }

    if (f_ReactionAnimation == REACTION_ANIMATION_EAGLE) {
        if (f_ReactionFrame >= MARIO_EFFECT_START_FRAME) {
            SpawnSparkleNearPoint(slotIndex, REACTION_EFFECT_POINT);
            GetCharacterPointPosition(0, REACTION_EFFECT_POINT, lightDirection);
            GetCharacterPointPosition(0, REACTION_FOCUS_POINT, lightOrigin);
            lightDirection[AXIS_X] -= lightOrigin[AXIS_X];
            lightDirection[AXIS_Y] -= lightOrigin[AXIS_Y];
            lightDirection[AXIS_Z] -= lightOrigin[AXIS_Z];
            length = sqrtf(
                (lightDirection[AXIS_X] * lightDirection[AXIS_X]) +
                (lightDirection[AXIS_Y] * lightDirection[AXIS_Y]) +
                (lightDirection[AXIS_Z] * lightDirection[AXIS_Z])
            );
            if (length > 1.0f) {
                length = LIGHT_DIRECTION_LENGTH / length;
                lightDirection[AXIS_X] *= length;
                lightDirection[AXIS_Y] *= length;
                lightDirection[AXIS_Z] *= length;
                f_HasReactionLight = TRUE;
                SetCharacterLighting(slotIndex, CHARACTER_LIGHTING_SCENE_THIRD);
                SetLightColor(&f_ReactionLights.l[2], 0xFF, 0xFF, 0x7F);
                f_ReactionLights.l[2].l.dir[AXIS_X] = lightDirection[AXIS_X];
                f_ReactionLights.l[2].l.dir[AXIS_Y] = lightDirection[AXIS_Y];
                f_ReactionLights.l[2].l.dir[AXIS_Z] = lightDirection[AXIS_Z];
            }
        }
    }

    GetCharacterPointPosition(slotIndex, REACTION_FOCUS_POINT, focus);
    highestHeight = (GetGroundHeight(
                         focus[AXIS_X] * WORLD_TO_COURSE_SCALE,
                         focus[AXIS_Z] * WORLD_TO_COURSE_SCALE
                     ) - CAMERA_GROUND_CLEARANCE) *
        COURSE_TO_WORLD_SCALE;
    if (highestHeight < focus[AXIS_Y]) {
        focus[AXIS_Y] = highestHeight;
    }

    if (f_ReactionAnimation == REACTION_ANIMATION_EAGLE) {
        focus[AXIS_Y] -= MARIO_EAGLE_FOCUS_RISE;
    }

    view->lookAt[AXIS_X] = focus[AXIS_X];
    view->lookAt[AXIS_Y] = focus[AXIS_Y];
    view->lookAt[AXIS_Z] = focus[AXIS_Z];
    view->eye[AXIS_X] = focus[AXIS_X] +
        (cosf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    view->eye[AXIS_Y] = focus[AXIS_Y] + (-MARIO_EYE_RISE);
    if (f_ReactionAnimation == REACTION_ANIMATION_PAR) {
        view->eye[AXIS_Y] = focus[AXIS_Y] + (-MARIO_PAR_EYE_RISE);
    }

    view->eye[AXIS_Z] = focus[AXIS_Z] +
        (sinf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    KeepAboveGround(view->eye);
    EaseReactionView(view->eye, view->lookAt, REACTION_EASE_RATE, EASE_EYE | EASE_LOOK_AT);
    if (IsReactionViewLow()) {
        view->lookAt[AXIS_Y] -= LOW_VIEW_LOOK_AT_RISE;
        view->eye[AXIS_Y] += LOW_VIEW_EYE_DROP;
    }
}

// Frames Wario from a point above where his shadow lies, with distances
// that depend on the animation and an effect late in the eagle animation
void FrameWarioReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance;
    f32 focus[AXIS_COUNT];
    // Pads the frame to match; without it 32 words differ
    u8 unusedBuffer[0x40] UNUSED;
    f32 highestHeight;

    if (f_ReactionAnimation == REACTION_ANIMATION_EAGLE) {
        distance = BlendClamped(
            FAR_REACTION_START_DISTANCE,
            WARIO_EAGLE_END_DISTANCE,
            f_ReactionFrame * REACTION_ZOOM_RATE
        );
    }
    else if (f_ReactionAnimation == REACTION_ANIMATION_BIRDIE) {
        distance = BlendClamped(
            FAR_REACTION_START_DISTANCE,
            WARIO_BIRDIE_END_DISTANCE,
            f_ReactionFrame * REACTION_ZOOM_RATE
        );
    }
    else {
        distance = BlendClamped(
            WARIO_START_DISTANCE,
            WARIO_END_DISTANCE,
            f_ReactionFrame * REACTION_ZOOM_RATE
        );
    }

    if (f_ReactionAnimation == REACTION_ANIMATION_EAGLE) {
        if (f_ReactionFrame >= WARIO_EFFECT_START_FRAME) {
            SpawnSparkleNearPoint(slotIndex, REACTION_EFFECT_POINT);
        }
    }

    GetCharacterPointPosition(slotIndex, CHARACTER_POINT_SHADOW, focus);
    focus[AXIS_Y] -= WARIO_FOCUS_ABOVE_SHADOW;
    if (f_ReactionAnimation == REACTION_ANIMATION_BIRDIE) {
        focus[AXIS_Y] += WARIO_BIRDIE_FOCUS_DROP;
    }

    highestHeight = (GetGroundHeight(
                         focus[AXIS_X] * WORLD_TO_COURSE_SCALE,
                         focus[AXIS_Z] * WORLD_TO_COURSE_SCALE
                     ) - CAMERA_GROUND_CLEARANCE) *
        COURSE_TO_WORLD_SCALE;
    if (highestHeight < focus[AXIS_Y]) {
        focus[AXIS_Y] = highestHeight;
    }

    view->lookAt[AXIS_X] = focus[AXIS_X];
    view->lookAt[AXIS_Y] = focus[AXIS_Y];
    view->lookAt[AXIS_Z] = focus[AXIS_Z];
    view->eye[AXIS_X] = focus[AXIS_X] +
        (cosf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    view->eye[AXIS_Z] = focus[AXIS_Z] +
        (sinf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    switch (f_ReactionAnimation) {
        case REACTION_ANIMATION_EAGLE:
            view->eye[AXIS_Y] = focus[AXIS_Y];
            break;
        case REACTION_ANIMATION_BIRDIE:
            view->eye[AXIS_Y] = focus[AXIS_Y] + (-WARIO_BIRDIE_EYE_RISE);
            break;
        default:
            view->eye[AXIS_Y] = focus[AXIS_Y] + (-WARIO_EYE_RISE);
            break;
    }

    KeepAboveGround(view->eye);
    EaseReactionView(view->eye, view->lookAt, REACTION_EASE_RATE, EASE_EYE | EASE_LOOK_AT);
    if (IsReactionViewLow()) {
        view->lookAt[AXIS_Y] -= LOW_VIEW_LOOK_AT_RISE;
        view->eye[AXIS_Y] += LOW_VIEW_EYE_DROP;
    }
}

// Frames Baby Mario level with him, closing in slowly, with effects on
// alternate sides late in the bogey animation
void FrameBabyMarioReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        BABY_MARIO_START_DISTANCE,
        REACTION_END_DISTANCE,
        f_ReactionFrame * BABY_MARIO_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
    // Pads the frame to match; without it 32 words differ
    u8 unusedBuffer[0x40] UNUSED;
    f32 eastward;
    f32 southward;
    Radians heading;
    f32 highestHeight;

    GetCharacterPointPosition(slotIndex, REACTION_FOCUS_POINT, focus);
    highestHeight = (GetGroundHeight(
                         focus[AXIS_X] * WORLD_TO_COURSE_SCALE,
                         focus[AXIS_Z] * WORLD_TO_COURSE_SCALE
                     ) - BABY_MARIO_GROUND_CLEARANCE) *
        COURSE_TO_WORLD_SCALE;
    if (highestHeight < focus[AXIS_Y]) {
        focus[AXIS_Y] = highestHeight;
    }

    view->lookAt[AXIS_X] = focus[AXIS_X];
    view->lookAt[AXIS_Y] = focus[AXIS_Y];
    view->lookAt[AXIS_Z] = focus[AXIS_Z];
    view->eye[AXIS_X] = focus[AXIS_X] +
        (cosf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    view->eye[AXIS_Y] = focus[AXIS_Y];
    view->eye[AXIS_Z] = focus[AXIS_Z] +
        (sinf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    KeepAboveGround(view->eye);
    EaseReactionView(view->eye, view->lookAt, REACTION_EASE_RATE, EASE_EYE | EASE_LOOK_AT);
    if (f_ReactionAnimation == REACTION_ANIMATION_BOGEY &&
        f_ReactionFrame >= BABY_MARIO_EFFECT_START_FRAME) {
        eastward = view->lookAt[AXIS_X] - view->eye[AXIS_X];
        southward = view->lookAt[AXIS_Z] - view->eye[AXIS_Z];
        if ((s32)eastward != 0 || (s32)southward != 0) {
            heading = atan2f(southward, eastward);
        }
        else {
            heading = 0.0f;
        }

        if (!(f_ReactionFrame & 1)) {
            SpawnTear(slotIndex, EVEN_FRAME_TEAR_POINT, heading + (M_PI / 2));
        }
        else {
            SpawnTear(slotIndex, ODD_FRAME_TEAR_POINT, heading - (M_PI / 2));
        }
    }

    if (IsReactionViewLow()) {
        view->lookAt[AXIS_Y] -= LOW_VIEW_LOOK_AT_RISE;
        view->eye[AXIS_Y] += LOW_VIEW_EYE_DROP;
    }
}

// Frames Yoshi from farther away, with an effect every third frame of the eagle
// animation
void FrameYoshiReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        FAR_REACTION_START_DISTANCE,
        REACTION_END_DISTANCE,
        f_ReactionFrame * REACTION_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
    // Pads the frame to match; without it 28 words differ
    u8 unusedBuffer[0x40] UNUSED;
    f32 highestHeight;

    if (f_ReactionAnimation == REACTION_ANIMATION_EAGLE) {
        if (f_ReactionFrame % YOSHI_SPARKLE_INTERVAL == 0) {
            SpawnSparkleNearPoint(slotIndex, REACTION_EFFECT_POINT);
        }
    }

    GetCharacterPointPosition(slotIndex, REACTION_FOCUS_POINT, focus);
    highestHeight = (GetGroundHeight(
                         focus[AXIS_X] * WORLD_TO_COURSE_SCALE,
                         focus[AXIS_Z] * WORLD_TO_COURSE_SCALE
                     ) - CAMERA_GROUND_CLEARANCE) *
        COURSE_TO_WORLD_SCALE;
    if (highestHeight < focus[AXIS_Y]) {
        focus[AXIS_Y] = highestHeight;
    }

    view->lookAt[AXIS_X] = focus[AXIS_X];
    view->lookAt[AXIS_Y] = focus[AXIS_Y];
    view->lookAt[AXIS_Z] = focus[AXIS_Z];
    view->eye[AXIS_X] = focus[AXIS_X] +
        (cosf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    if (f_ReactionAnimation == REACTION_ANIMATION_BOGEY) {
        view->eye[AXIS_Y] = focus[AXIS_Y];
    }
    else {
        view->eye[AXIS_Y] = focus[AXIS_Y] + (-YOSHI_EYE_RISE);
    }

    view->eye[AXIS_Z] = focus[AXIS_Z] +
        (sinf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    KeepAboveGround(view->eye);
    EaseReactionView(view->eye, view->lookAt, REACTION_EASE_RATE, EASE_EYE | EASE_LOOK_AT);
    if (IsReactionViewLow()) {
        if (f_ReactionAnimation == REACTION_ANIMATION_BOGEY) {
            view->lookAt[AXIS_Y] -= LOW_VIEW_LOOK_AT_RISE;
            view->eye[AXIS_Y] += LOW_VIEW_SHORT_EYE_DROP;
        }
        else {
            view->lookAt[AXIS_Y] -= YOSHI_LOW_VIEW_SHIFT;
            view->eye[AXIS_Y] += YOSHI_LOW_VIEW_SHIFT;
        }
    }
}

// Frames Bowser from farther away, with effects at three frames of the bogey
// animation.
void FrameBowserReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        FAR_REACTION_START_DISTANCE,
        REACTION_END_DISTANCE,
        f_ReactionFrame * REACTION_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
    // Pads the frame to match; without it 32 words differ
    u8 unusedBuffer[0x40] UNUSED;
    f32 highestHeight;

    if (f_ReactionAnimation == REACTION_ANIMATION_BOGEY) {
        if (f_ReactionFrame == BOWSER_FIRST_CIRCLE_FRAME ||
            f_ReactionFrame == BOWSER_SECOND_CIRCLE_FRAME ||
            f_ReactionFrame == BOWSER_THIRD_CIRCLE_FRAME) {
            SpawnSpreadingCircle(0, REACTION_EFFECT_POINT);
        }
    }

    GetCharacterPointPosition(slotIndex, REACTION_FOCUS_POINT, focus);
    highestHeight = (GetGroundHeight(
                         focus[AXIS_X] * WORLD_TO_COURSE_SCALE,
                         focus[AXIS_Z] * WORLD_TO_COURSE_SCALE
                     ) - CAMERA_GROUND_CLEARANCE) *
        COURSE_TO_WORLD_SCALE;
    if (highestHeight < focus[AXIS_Y]) {
        focus[AXIS_Y] = highestHeight;
    }

    view->lookAt[AXIS_X] = focus[AXIS_X];
    view->lookAt[AXIS_Y] = focus[AXIS_Y];
    view->lookAt[AXIS_Z] = focus[AXIS_Z];
    view->eye[AXIS_X] = focus[AXIS_X] +
        (cosf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    view->eye[AXIS_Z] = focus[AXIS_Z] +
        (sinf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    switch (f_ReactionAnimation) {
        case REACTION_ANIMATION_EAGLE:
            view->eye[AXIS_Y] = focus[AXIS_Y] + (-BOWSER_EAGLE_EYE_RISE);
            KeepAboveGround(view->eye);
            EaseReactionView(view->eye, view->lookAt, REACTION_EASE_RATE, EASE_EYE | EASE_LOOK_AT);
            break;
        case REACTION_ANIMATION_BIRDIE:
            view->eye[AXIS_Y] = focus[AXIS_Y] + REACTION_EYE_DROP;
            KeepAboveGround(view->eye);
            EaseReactionView(view->eye, view->lookAt, 1.0f, EASE_EYE | EASE_LOOK_AT);

        // fallthrough
        default:
            view->eye[AXIS_Y] = focus[AXIS_Y] + REACTION_EYE_DROP;
            KeepAboveGround(view->eye);
            EaseReactionView(view->eye, view->lookAt, REACTION_EASE_RATE, EASE_EYE | EASE_LOOK_AT);
            break;
    }

    if (IsReactionViewLow()) {
        if (f_ReactionAnimation == REACTION_ANIMATION_EAGLE) {
            view->lookAt[AXIS_Y] -= BOWSER_EAGLE_LOW_VIEW_LOOK_AT_RISE;
            view->eye[AXIS_Y] += BOWSER_EAGLE_LOW_VIEW_EYE_DROP;
        }
        else {
            view->lookAt[AXIS_Y] -= BOWSER_LOW_VIEW_SHIFT;
            view->eye[AXIS_Y] += BOWSER_LOW_VIEW_SHIFT;
        }
    }
}

// Frames Azalea from farther away and from above
void FrameAzaleaReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        FAR_REACTION_START_DISTANCE,
        REACTION_END_DISTANCE,
        f_ReactionFrame * REACTION_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
    // Pads the frame to match; without it 32 words differ
    u8 unusedBuffer[0x40] UNUSED;
    f32 highestHeight;

    GetCharacterPointPosition(slotIndex, REACTION_FOCUS_POINT, focus);
    highestHeight = (GetGroundHeight(
                         focus[AXIS_X] * WORLD_TO_COURSE_SCALE,
                         focus[AXIS_Z] * WORLD_TO_COURSE_SCALE
                     ) - CAMERA_GROUND_CLEARANCE) *
        COURSE_TO_WORLD_SCALE;
    if (highestHeight < focus[AXIS_Y]) {
        focus[AXIS_Y] = highestHeight;
    }

    focus[AXIS_Y] += AZALEA_FOCUS_DROP;
    view->lookAt[AXIS_X] = focus[AXIS_X];
    view->lookAt[AXIS_Y] = focus[AXIS_Y];
    view->lookAt[AXIS_Z] = focus[AXIS_Z];
    view->eye[AXIS_X] = focus[AXIS_X] +
        (cosf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    view->eye[AXIS_Y] = focus[AXIS_Y] + (-AZALEA_EYE_RISE);
    view->eye[AXIS_Z] = focus[AXIS_Z] +
        (sinf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    KeepAboveGround(view->eye);
    EaseReactionView(view->eye, view->lookAt, REACTION_EASE_RATE, EASE_EYE | EASE_LOOK_AT);
    if (IsReactionViewLow()) {
        view->lookAt[AXIS_Y] -= LOW_VIEW_LOOK_AT_RISE;
        view->eye[AXIS_Y] += LOW_VIEW_EYE_DROP;
    }
}

// Frames Donkey Kong, easing the camera more slowly
void FrameDonkeyKongReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        REACTION_START_DISTANCE,
        REACTION_END_DISTANCE,
        f_ReactionFrame * REACTION_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
    // Pads the frame to match; without it 32 words differ
    u8 unusedBuffer[0x40] UNUSED;
    f32 highestHeight;

    GetCharacterPointPosition(slotIndex, REACTION_FOCUS_POINT, focus);
    highestHeight = (GetGroundHeight(
                         focus[AXIS_X] * WORLD_TO_COURSE_SCALE,
                         focus[AXIS_Z] * WORLD_TO_COURSE_SCALE
                     ) - CAMERA_GROUND_CLEARANCE) *
        COURSE_TO_WORLD_SCALE;
    if (highestHeight < focus[AXIS_Y]) {
        focus[AXIS_Y] = highestHeight;
    }

    view->lookAt[AXIS_X] = focus[AXIS_X];
    view->lookAt[AXIS_Y] = focus[AXIS_Y];
    view->lookAt[AXIS_Z] = focus[AXIS_Z];
    view->eye[AXIS_X] = focus[AXIS_X] +
        (cosf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    view->eye[AXIS_Y] = focus[AXIS_Y] + REACTION_EYE_DROP;
    view->eye[AXIS_Z] = focus[AXIS_Z] +
        (sinf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    KeepAboveGround(view->eye);
    EaseReactionView(view->eye, view->lookAt, DONKEY_KONG_EASE_RATE, EASE_EYE | EASE_LOOK_AT);
    if (IsReactionViewLow()) {
        view->lookAt[AXIS_Y] -= LOW_VIEW_LOOK_AT_RISE;
        view->eye[AXIS_Y] += LOW_VIEW_EYE_DROP;
    }
}

// Frames the reaction of a character with no framing of its own
void FrameReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        REACTION_START_DISTANCE,
        REACTION_END_DISTANCE,
        f_ReactionFrame * REACTION_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
    // Pads the frame to match; without it 32 words differ
    u8 unusedBuffer[0x40] UNUSED;
    f32 highestHeight;

    GetCharacterPointPosition(slotIndex, REACTION_FOCUS_POINT, focus);
    highestHeight = (GetGroundHeight(
                         focus[AXIS_X] * WORLD_TO_COURSE_SCALE,
                         focus[AXIS_Z] * WORLD_TO_COURSE_SCALE
                     ) - CAMERA_GROUND_CLEARANCE) *
        COURSE_TO_WORLD_SCALE;
    if (highestHeight < focus[AXIS_Y]) {
        focus[AXIS_Y] = highestHeight;
    }

    view->lookAt[AXIS_X] = focus[AXIS_X];
    view->lookAt[AXIS_Y] = focus[AXIS_Y];
    view->lookAt[AXIS_Z] = focus[AXIS_Z];
    view->eye[AXIS_X] = focus[AXIS_X] +
        (cosf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    view->eye[AXIS_Y] = focus[AXIS_Y] + REACTION_EYE_DROP;
    view->eye[AXIS_Z] = focus[AXIS_Z] +
        (sinf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    KeepAboveGround(view->eye);
    EaseReactionView(view->eye, view->lookAt, REACTION_EASE_RATE, EASE_EYE | EASE_LOOK_AT);
    if (IsReactionViewLow()) {
        view->lookAt[AXIS_Y] -= LOW_VIEW_LOOK_AT_RISE;
        view->eye[AXIS_Y] += LOW_VIEW_SHORT_EYE_DROP;
    }
}

// Frames Peach, looking lower except after a bogey
void FramePeachReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        REACTION_START_DISTANCE,
        REACTION_END_DISTANCE,
        f_ReactionFrame * REACTION_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
    // Pads the frame to match; without it 32 words differ
    u8 unusedBuffer[0x40] UNUSED;
    f32 highestHeight;

    GetCharacterPointPosition(slotIndex, REACTION_FOCUS_POINT, focus);
    highestHeight = (GetGroundHeight(
                         focus[AXIS_X] * WORLD_TO_COURSE_SCALE,
                         focus[AXIS_Z] * WORLD_TO_COURSE_SCALE
                     ) - CAMERA_GROUND_CLEARANCE) *
        COURSE_TO_WORLD_SCALE;
    if (highestHeight < focus[AXIS_Y]) {
        focus[AXIS_Y] = highestHeight;
    }

    if (f_ReactionAnimation != REACTION_ANIMATION_BOGEY) {
        focus[AXIS_Y] += PEACH_FOCUS_DROP;
    }

    view->lookAt[AXIS_X] = focus[AXIS_X];
    view->lookAt[AXIS_Y] = focus[AXIS_Y];
    view->lookAt[AXIS_Z] = focus[AXIS_Z];
    view->eye[AXIS_X] = focus[AXIS_X] +
        (cosf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    view->eye[AXIS_Y] = focus[AXIS_Y] + REACTION_EYE_DROP;
    view->eye[AXIS_Z] = focus[AXIS_Z] +
        (sinf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    KeepAboveGround(view->eye);
    EaseReactionView(view->eye, view->lookAt, REACTION_EASE_RATE, EASE_EYE | EASE_LOOK_AT);
    if (IsReactionViewLow()) {
        view->lookAt[AXIS_Y] -= LOW_VIEW_LOOK_AT_RISE;
        view->eye[AXIS_Y] += LOW_VIEW_EYE_DROP;
    }
}

// Frames Sherry, with effects at her points through the eagle animation
void FrameSherryReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        REACTION_START_DISTANCE,
        SHERRY_END_DISTANCE,
        f_ReactionFrame * REACTION_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
    // Pads the frame to match; without it 36 words differ
    u8 unusedBuffer[0x40] UNUSED;
    f32 highestHeight;

    if (f_ReactionAnimation == REACTION_ANIMATION_EAGLE &&
        (u32)(f_ReactionFrame - SHERRY_EFFECT_START_FRAME) < SHERRY_EFFECT_FRAME_COUNT) {
        SpawnSparkleNearPoint(
            slotIndex,
            (f_ReactionFrame % SHERRY_SPARKLE_RUN_LENGTH) + SHERRY_FIRST_SPARKLE_POINT
        );
        SpawnSparkleNearPoint(
            slotIndex,
            (f_ReactionFrame % SHERRY_SPARKLE_RUN_LENGTH) + SHERRY_SECOND_SPARKLE_POINT
        );
    }

    GetCharacterPointPosition(slotIndex, REACTION_FOCUS_POINT, focus);
    highestHeight = (GetGroundHeight(
                         focus[AXIS_X] * WORLD_TO_COURSE_SCALE,
                         focus[AXIS_Z] * WORLD_TO_COURSE_SCALE
                     ) - CAMERA_GROUND_CLEARANCE) *
        COURSE_TO_WORLD_SCALE;
    if (highestHeight < focus[AXIS_Y]) {
        focus[AXIS_Y] = highestHeight;
    }

    view->lookAt[AXIS_X] = focus[AXIS_X];
    view->lookAt[AXIS_Y] = focus[AXIS_Y];
    view->lookAt[AXIS_Z] = focus[AXIS_Z];
    view->eye[AXIS_X] = focus[AXIS_X] +
        (cosf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    view->eye[AXIS_Y] = focus[AXIS_Y] + REACTION_EYE_DROP;
    view->eye[AXIS_Z] = focus[AXIS_Z] +
        (sinf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * distance * COURSE_TO_WORLD_SCALE);
    KeepAboveGround(view->eye);
    EaseReactionView(view->eye, view->lookAt, REACTION_EASE_RATE, EASE_EYE | EASE_LOOK_AT);
    if (IsReactionViewLow()) {
        view->lookAt[AXIS_Y] -= LOW_VIEW_LOOK_AT_RISE;
        view->eye[AXIS_Y] += LOW_VIEW_SHORT_EYE_DROP;
    }
}

// Frames Kid, swinging the camera round to the front as the animation ends
void FrameKidReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        REACTION_START_DISTANCE,
        REACTION_END_DISTANCE,
        f_ReactionFrame * REACTION_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
    // Pads the frame to match; without it 36 words differ
    u8 unusedBuffer[0x40] UNUSED;
    f32 turn = (f_ReactionFrame - f_ReactionFrameCount) * KID_TURN_STEP;
    f32 highestHeight;

    if (turn > 0.0f) {
        turn = 0.0f;
    }

    GetCharacterPointPosition(slotIndex, REACTION_FOCUS_POINT, focus);
    highestHeight = (GetGroundHeight(
                         focus[AXIS_X] * WORLD_TO_COURSE_SCALE,
                         focus[AXIS_Z] * WORLD_TO_COURSE_SCALE
                     ) - CAMERA_GROUND_CLEARANCE) *
        COURSE_TO_WORLD_SCALE;
    if (highestHeight < focus[AXIS_Y]) {
        focus[AXIS_Y] = highestHeight;
    }

    view->lookAt[AXIS_X] = focus[AXIS_X];
    view->lookAt[AXIS_Y] = focus[AXIS_Y];
    view->lookAt[AXIS_Z] = focus[AXIS_Z];
    if (IsReactionViewLow()) {
        turn = 0.0f;
    }

    view->eye[AXIS_X] = focus[AXIS_X] +
        (cosf((turn - slot->rotation[AXIS_Y]) - QUARTER_TURN_RADIANS) * distance *
            COURSE_TO_WORLD_SCALE);
    view->eye[AXIS_Y] = focus[AXIS_Y] + REACTION_EYE_DROP;
    view->eye[AXIS_Z] = focus[AXIS_Z] +
        (sinf((turn - slot->rotation[AXIS_Y]) - QUARTER_TURN_RADIANS) * distance *
            COURSE_TO_WORLD_SCALE);
    KeepAboveGround(view->eye);
    EaseReactionView(view->eye, view->lookAt, KID_EASE_RATE, EASE_EYE | EASE_LOOK_AT);
    if (IsReactionViewLow()) {
        view->lookAt[AXIS_Y] -= LOW_VIEW_LOOK_AT_RISE;
        view->eye[AXIS_Y] += LOW_VIEW_EYE_DROP;
    }
}

// Advances the reaction in the slot by a frame, and frames it with the
// first camera
NO_RESULT UpdateReaction(u32 slotIndex) {
    char text[DEBUG_TEXT_BUFFER_SIZE];
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    CameraView *view;

    f_ReactionFrame++;
    DimNextTranslucentTriangles();
    TakePinOut();
    if (IsGameFlagSet(GAME_FLAG_SHOW_REACTION_FRAME)) {
        sprintf(text, "FRAME %d  ", (int)f_ReactionFrame);
        PrintDebugText(text, REACTION_FRAME_COLUMN, REACTION_FRAME_ROW);
    }

    PlayReactionSounds();
    if (slot != NULL) {
        view = &g_Cameras[MAIN_CAMERA].view;
        switch (slot->modelIndex % CHARACTER_COUNT) {
            case CHARACTER_PEACH:
                FramePeachReaction(slotIndex, view);
                break;
            case CHARACTER_MARIO:
            case CHARACTER_METAL_MARIO:
                FrameMarioReaction(slotIndex, view);
                break;
            case CHARACTER_WARIO:
                FrameWarioReaction(slotIndex, view);
                break;
            case CHARACTER_YOSHI:
                FrameYoshiReaction(slotIndex, view);
                break;
            case CHARACTER_BOWSER:
                FrameBowserReaction(slotIndex, view);
                break;
            case CHARACTER_DONKEY_KONG:
                FrameDonkeyKongReaction(slotIndex, view);
                break;
            case CHARACTER_KID:
                FrameKidReaction(slotIndex, view);
                break;
            case CHARACTER_SHERRY:
                FrameSherryReaction(slotIndex, view);
                break;
            case CHARACTER_AZALEA:
                FrameAzaleaReaction(slotIndex, view);
                break;
            case CHARACTER_BABY_MARIO:
                FrameBabyMarioReaction(slotIndex, view);
                break;
            case CHARACTER_PLUM:
            case CHARACTER_CHARLIE:
            case CHARACTER_MAPLE:
            case CHARACTER_LUIGI:
            case CHARACTER_SONNY:
            case CHARACTER_HARRY:
            default:
                FrameReaction(slotIndex, view);
                break;
        }
    }
}

// Forgets the reaction spot, so that the next spot considered is taken
void ClearReactionSpot(void) {
    f_HasReactionSpot = FALSE;
}

// Takes the point as the reaction spot if there is none, or if the ground
// around it is less than half as rough as around the spot and the spot is
// rough. Roughness adds up the height differences to the point from rings
// of samples around it, the nearer rings counting more, and is high near a
// billboard.
NO_RESULT ConsiderReactionSpot(CoursePoint point) {
    s32 roughness;
    s32 ringNumber;
    s32 groundHeight;
    s32 sampleIndex;
    Boolean isNearBillboard;
    s32 billboardIndex;
    Billboard *billboard;

    groundHeight = GetGroundHeight(point[AXIS_X], point[AXIS_Z]);
    roughness = 0;
    for (ringNumber = 1; ringNumber <= SPOT_RING_COUNT; ringNumber++) {
        for (sampleIndex = 0; sampleIndex < SPOT_SAMPLES_PER_RING; sampleIndex++) {
            Radians angle = sampleIndex * SPOT_SAMPLE_ANGLE;
            f32 offsetX = cosf(angle) * ringNumber * SPOT_RING_SPACING;
            f32 offsetZ = sinf(angle) * ringNumber * SPOT_RING_SPACING;
            s32 heightDifference =
                GetGroundHeight(point[AXIS_X] + offsetX, point[AXIS_Z] + offsetZ) - groundHeight;

            if (heightDifference < 0) {
                heightDifference = -heightDifference;
            }

            roughness += heightDifference / (ringNumber + 1);
        }
    }

    roughness *= 2;
    if (!f_HasReactionSpot) {
        isNearBillboard = FALSE;
        for (billboardIndex = 0; billboardIndex < BILLBOARD_COUNT; billboardIndex++) {
            billboard = &g_Billboards[billboardIndex];
            if ((s32)IntLength2D(
                    (billboard->x << WORLD_TO_COURSE_SHIFT) - point[AXIS_X],
                    (billboard->z << WORLD_TO_COURSE_SHIFT) - point[AXIS_Z]
                ) - (billboard->width << WORLD_TO_COURSE_SHIFT) <
                SPOT_BILLBOARD_CLEARANCE) {
                isNearBillboard = TRUE;
            }
        }

        if (isNearBillboard) {
            roughness = NEAR_BILLBOARD_ROUGHNESS;
        }
    }

    if (IsBetterReactionSpot(roughness)) {
        f_ReactionSpotRoughness = roughness;
        f_ReactionSpot[AXIS_X] = point[AXIS_X];
        f_ReactionSpot[AXIS_Y] = point[AXIS_Y];
        f_ReactionSpot[AXIS_Z] = point[AXIS_Z];
        f_HasReactionSpot = TRUE;
    }
}

void GetReactionSpot(CoursePoint point) {
    point[AXIS_X] = f_ReactionSpot[AXIS_X];
    point[AXIS_Y] = f_ReactionSpot[AXIS_Y];
    point[AXIS_Z] = f_ReactionSpot[AXIS_Z];
}

// Stands a character on the ground SPOT_PLACE_DISTANCE from the reaction
// spot, a quarter turn from the shot's aim, facing back along the aim. Its
// pitch is refined twice so the slope matches the ground there.
void PlaceAtReactionSpot(Vec3 position, Vec3 rotation, Vec3 scale) {
    f32 ahead[AXIS_COUNT];
    CoursePoint spot;
    // Pads the frame to match; without it 48 words differ
    u8 unusedBuffer[0x80] UNUSED;
    Radians heading;
    Radians pitch;
    f32 distance;
    s32 refinementStep;
    f32 cosine;
    f32 absRadius;
    f32 radius = SPOT_PLACE_DISTANCE;
    SignedBinaryAngle shotHeading = g_CurrentShot.heading;
    WideBinaryAngle sidewaysHeading;

    spot[AXIS_X] = f_ReactionSpot[AXIS_X];
    spot[AXIS_Y] = f_ReactionSpot[AXIS_Y];
    spot[AXIS_Z] = f_ReactionSpot[AXIS_Z];
    sidewaysHeading = shotHeading + BINARY_ANGLE_QUARTER_TURN;
    heading = (BinaryAngle)sidewaysHeading * BINARY_ANGLE_TO_RADIANS;
    ahead[AXIS_X] = (cosf(heading) * radius) + spot[AXIS_X];
    ahead[AXIS_Z] = (sinf(heading) * radius) + spot[AXIS_Z];
    ahead[AXIS_Y] = GetGroundHeight(ahead[AXIS_X], ahead[AXIS_Z]);
    pitch = atan2(ahead[AXIS_Y] - spot[AXIS_Y], radius);
    for (refinementStep = 0; refinementStep < PITCH_REFINEMENT_COUNT; refinementStep++) {
        cosine = cosf(pitch);
        absRadius = radius;
        if (absRadius < 0.0f) {
            absRadius = -absRadius;
        }

        distance = cosine * absRadius;
        position[AXIS_X] = (cosf(heading) * distance) + spot[AXIS_X];
        position[AXIS_Z] = (sinf(heading) * distance) + spot[AXIS_Z];
        position[AXIS_Y] = GetGroundHeight(position[AXIS_X], position[AXIS_Z]);
        rotation[AXIS_X] = atan2(position[AXIS_Y] - spot[AXIS_Y], distance);
        pitch += rotation[AXIS_X];
        pitch *= 0.5f;
    }

    scale[AXIS_Z] = 1.0f;
    scale[AXIS_Y] = scale[AXIS_Z];
    scale[AXIS_X] = scale[AXIS_Y];
    rotation[AXIS_X] = 0.0f;
    rotation[AXIS_Z] = 0;
    rotation[AXIS_Y] = ((-g_CurrentShot.heading) & BINARY_ANGLE_MASK) * BINARY_ANGLE_TO_RADIANS;
}

// Whether the eased eye sits at height zero
Boolean IsEasedEyeHeightZero(void) {
    Boolean isZero;

    if (fabsf(f_EasedView.eye[AXIS_Y]) == 0.0f) {
        isZero = TRUE;
    }
    else {
        isZero = FALSE;
    }

    return isZero;
}

static const char f_UnusedFloatFormat[] = "            %f";
static const char f_UnusedEmptyText[] = "";
