#include "reaction.h"

#include <math.h>
#include <ultra64.h>

#include "billboard.h"
#include "camera.h"
#include "character.h"
#include "character_slot.h"
#include "compiler.h"
#include "course_space.h"
#include "current_shot.h"
#include "debug_text.h"
#include "effects.h"
#include "game_flags.h"
#include "game_state.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "hole_triangles.h"
#include "language.h"
#include "math_util.h"
#include "pin.h"
#include "scene.h"
#include "sound.h"
#include "terrain.h"

#define FIRST_REACTION_CHANNEL 4
#define LAST_REACTION_CHANNEL 7
#define NO_REACTION_SOUND (-1)

#define CAMERA_GROUND_CLEARANCE 0x1E00

#define SPOT_RING_COUNT 4
#define SPOT_RING_SPACING 15360.0f
#define SPOT_SAMPLES_PER_RING 16
#define SPOT_SAMPLE_ANGLE 0.3926991f
#define SPOT_BILLBOARD_CLEARANCE 0x12C00
#define NEAR_BILLBOARD_ROUGHNESS 0x465000
#define SPOT_SMOOTH_ENOUGH_ROUGHNESS 0xB400

#define REACTION_FOCUS_POINT 1
#define REACTION_START_DISTANCE 76800.0f
#define REACTION_END_DISTANCE 30720.0f
#define REACTION_ZOOM_RATE 0.025f
#define REACTION_EASE_RATE 0.1f
#define KID_TURN_STEP 0.049087387f
#define SHERRY_EFFECT_START_FRAME 15
#define SHERRY_EFFECT_FRAME_COUNT 37
#define BABY_MARIO_GROUND_CLEARANCE 0x2400
#define BABY_MARIO_EFFECT_START_FRAME 44
#define WARIO_EFFECT_START_FRAME 20
#define MARIO_EFFECT_START_FRAME 10
#define LIGHT_DIRECTION_LENGTH 127.0f
#define REACTION_EYE_DROP 1.5f

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

#define MARIO_EAGLE_FOCUS_RISE 4.5f
#define MARIO_EYE_RISE 1.5f
#define MARIO_PAR_EYE_RISE 7.5f
#define WARIO_FOCUS_ABOVE_SHADOW 15.0f
#define WARIO_BIRDIE_FOCUS_DROP 1.5f
#define WARIO_EYE_RISE 4.5f
#define WARIO_BIRDIE_EYE_RISE 9.0f
#define YOSHI_EYE_RISE 15.0f
#define BOWSER_EAGLE_EYE_RISE 15.0f
#define AZALEA_FOCUS_DROP 3.0f
#define AZALEA_EYE_RISE 7.5f
#define PEACH_FOCUS_DROP 4.5f

#define LOW_VIEW_LOOK_AT_RISE 4.5f
#define LOW_VIEW_EYE_DROP 4.5f
#define LOW_VIEW_SHORT_EYE_DROP 3.0f
#define YOSHI_LOW_VIEW_SHIFT 7.5f
#define BOWSER_EAGLE_LOW_VIEW_LOOK_AT_RISE 7.5f
#define BOWSER_EAGLE_LOW_VIEW_EYE_DROP 9.0f
#define BOWSER_LOW_VIEW_SHIFT 3.0f

#define REACTION_EFFECT_POINT 3
#define YOSHI_SPARKLE_INTERVAL 3
#define SHERRY_SPARKLE_RUN_LENGTH 3
#define SHERRY_FIRST_SPARKLE_POINT 3
#define SHERRY_SECOND_SPARKLE_POINT 6
#define EVEN_FRAME_TEAR_POINT 3
#define ODD_FRAME_TEAR_POINT 4
#define BOWSER_FIRST_CIRCLE_FRAME 10
#define BOWSER_SECOND_CIRCLE_FRAME 24
#define BOWSER_THIRD_CIRCLE_FRAME 33

#define SPOT_PLACE_DISTANCE 13824.0f
#define PITCH_REFINEMENT_COUNT 2

#define REACTION_FRAME_COLUMN 3
#define REACTION_FRAME_ROW 4

static s32 f_ReactionFrame = 0;
static s32 f_UnusedReactionWord UNUSED = 0;

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

#define IsReactionViewLow() \
    (IsGameFlagSet(GAME_FLAG_LOW_REACTION_VIEWS) || \
        ((g_Scene != SCENE_STAFF_ROLL) && (g_GameState.isSuddenDeath == TRUE)))

#define IsBetterReactionSpot(roughness) \
    (!f_HasReactionSpot || \
        (((roughness) * 2 < f_ReactionSpotRoughness) && \
            (f_ReactionSpotRoughness > SPOT_SMOOTH_ENOUGH_ROUGHNESS)))

f32 BlendClamped(f32 from, f32 to, f32 weight) {
    if (weight < 0.0f) {
        weight = 0.0f;
    }

    if (weight > 1.0f) {
        weight = 1.0f;
    }

    return (from * (1.0f - weight)) + (to * weight);
}

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

s32 AddReactionLight(Gfx **displayListHead, Lights2 *lights) {
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

void CueReactionSound(s32 frame, SoundId soundId) {
    if (soundId != NO_REACTION_SOUND && f_ReactionFrame - 1 == frame) {
        PlaySound(soundId, f_ReactionChannel, SOUND_FULL_SCALE);
        f_ReactionChannel++;
        if (f_ReactionChannel > LAST_REACTION_CHANNEL) {
            f_ReactionChannel = FIRST_REACTION_CHANNEL;
        }
    }
}

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

void FrameMarioReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance;
    f32 focus[AXIS_COUNT];
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
                f_ReactionLights.l[2].l.dir[0] = lightDirection[AXIS_X];
                f_ReactionLights.l[2].l.dir[1] = lightDirection[AXIS_Y];
                f_ReactionLights.l[2].l.dir[2] = lightDirection[AXIS_Z];
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

void FrameWarioReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance;
    f32 focus[AXIS_COUNT];
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

void FrameBabyMarioReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        BABY_MARIO_START_DISTANCE,
        REACTION_END_DISTANCE,
        f_ReactionFrame * BABY_MARIO_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
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

void FrameYoshiReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        FAR_REACTION_START_DISTANCE,
        REACTION_END_DISTANCE,
        f_ReactionFrame * REACTION_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
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

void FrameBowserReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        FAR_REACTION_START_DISTANCE,
        REACTION_END_DISTANCE,
        f_ReactionFrame * REACTION_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
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

void FrameAzaleaReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        FAR_REACTION_START_DISTANCE,
        REACTION_END_DISTANCE,
        f_ReactionFrame * REACTION_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
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

void FrameDonkeyKongReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        REACTION_START_DISTANCE,
        REACTION_END_DISTANCE,
        f_ReactionFrame * REACTION_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
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

void FrameReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        REACTION_START_DISTANCE,
        REACTION_END_DISTANCE,
        f_ReactionFrame * REACTION_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
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

void FramePeachReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        REACTION_START_DISTANCE,
        REACTION_END_DISTANCE,
        f_ReactionFrame * REACTION_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
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

void FrameSherryReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        REACTION_START_DISTANCE,
        SHERRY_END_DISTANCE,
        f_ReactionFrame * REACTION_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
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

void FrameKidReaction(u32 slotIndex, CameraView *view) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    f32 distance = BlendClamped(
        REACTION_START_DISTANCE,
        REACTION_END_DISTANCE,
        f_ReactionFrame * REACTION_ZOOM_RATE
    );
    f32 focus[AXIS_COUNT];
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

void ClearReactionSpot(void) {
    f_HasReactionSpot = FALSE;
}

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

void PlaceAtReactionSpot(Vec3 position, Vec3 rotation, Vec3 scale) {
    f32 ahead[AXIS_COUNT];
    CoursePoint spot;
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
