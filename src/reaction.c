#include "common.h"
#include "billboard.h"
#include "camera.h"
#include "character.h"
#include "character_slot.h"
#include "current_shot.h"
#include "debug_text.h"
#include "functions.h"
#include "game_flags.h"
#include "hole_triangles.h"
#include "libc.h"
#include "math_util.h"
#include "pin.h"
#include "reaction.h"
#include "scene.h"
#include "sound.h"

/*
 * After a hole, the golfer reacts to the score with an animation. These
 * routines frame the golfer for it, cue its sounds, and pick a flat spot
 * for it to play on.
 */

/* EaseReactionView's easedParts bits */
#define EASE_LOOK_AT (1 << 0)
#define EASE_EYE (1 << 1)

/* Reaction sounds take turns on the channels from the first up to the last */
#define FIRST_REACTION_CHANNEL 4
#define LAST_REACTION_CHANNEL 7
#define REACTION_SOUND_VOLUME 0x7F
#define NO_REACTION_SOUND -1

/* Course units in a world unit */
#define COURSE_UNITS_PER_UNIT 1024
#define WORLD_TO_COURSE_SCALE 1024.0f
#define COURSE_TO_WORLD_SCALE (1.0f / 1024)
/* The camera keeps this many course units above the ground */
#define CAMERA_GROUND_CLEARANCE 0x1E00

/* ConsiderReactionSpot samples the ground on rings around the spot */
#define SPOT_RING_COUNT 4
#define SPOT_RING_SPACING 15360.0f
#define SPOT_SAMPLES_PER_RING 16
#define SPOT_SAMPLE_ANGLE 0.3926991f /* A sixteenth of a turn */
/* A spot this near a billboard's edge, in course units, counts as rough */
#define SPOT_BILLBOARD_CLEARANCE 0x12C00
#define BILLBOARD_UNIT_SHIFT 10
#define NEAR_BILLBOARD_ROUGHNESS 0x465000
/*
 * A new spot replaces the last only if it is less than half as rough, and
 * the last is rougher than this
 */
#define SPOT_SMOOTH_ENOUGH_ROUGHNESS 0xB400

/* PlaceAtReactionSpot stands the character this far from the spot */
#define SPOT_PLACE_DISTANCE 13824.0f
#define PITCH_REFINEMENT_COUNT 2
#define QUARTER_BINARY_TURN 0x4000

/* Where UpdateReaction prints the frame number */
#define REACTION_FRAME_COLUMN 3
#define REACTION_FRAME_ROW 4

/* Where the camera's points were last eased to, and whether they have been */
typedef struct EasedView {
    /* 0x00 */ f32 eye[AXIS_COUNT];
    /* 0x0C */ f32 lookAt[AXIS_COUNT];
    /* 0x18 */ s8 isEyeEased;
    /* 0x19 */ s8 isLookAtEased;
} EasedView; /* size = 0x1C */

/* Frames since the reaction began */
s32 sReactionFrame = 0;
s32 sUnusedReactionWord = 0; /* Unused; it is there to match */

/* The third light is set by the framing routines that light the golfer */
Lights3 sReactionLights = gdSPDefLights3(
    0x60, 0x60, 0x60,
    0xFF, 0xDC, 0xDC, 0x00, 0x88, 0x78,
    0xFF, 0xDC, 0xDC, 0x00, 0x88, 0x78,
    0xFF, 0xDC, 0xDC, 0x00, 0x88, 0x78);

s32 sReactionChannel = FIRST_REACTION_CHANNEL;

extern s32 sReactionCharacter;
extern s32 sReactionAnimation;
extern s32 sReactionFrameCount;
extern s32 sHasReactionLight;
extern EasedView sEasedView;
extern CoursePoint sReactionSpot;
extern s32 sReactionSpotRoughness;
extern s8 sHasReactionSpot;

extern s32 D_801B60A0;

s32 GetAnimationFrameCount(u32 slotIndex, s32 id);
void FrameMarioReaction(u32 slotIndex, CameraView *view);
void FrameWarioReaction(u32 slotIndex, CameraView *view);
void FrameBabyMarioReaction(u32 slotIndex, CameraView *view);
void FrameMapleReaction(u32 slotIndex, CameraView *view);
void FrameBowserReaction(u32 slotIndex, CameraView *view);
void FrameAzaleaReaction(u32 slotIndex, CameraView *view);
void FrameDonkeyKongReaction(u32 slotIndex, CameraView *view);
void FrameReaction(u32 slotIndex, CameraView *view);
void FrameYoshiReaction(u32 slotIndex, CameraView *view);
void FrameSherryReaction(u32 slotIndex, CameraView *view);
void FrameKidReaction(u32 slotIndex, CameraView *view);
void SetCharacterLighting(u32 slotIndex, s32 value);

/* Blends from one value to another by the weight, limited to 0 to 1 */
f32 BlendClamped(f32 from, f32 to, f32 weight) {
    if (weight < 0.0f) {
        weight = 0.0f;
    }
    if (weight > 1.0f) {
        weight = 1.0f;
    }
    return from * (1.0f - weight) + to * weight;
}

/*
 * Moves the eye and look-at points the rate of the way from where they were
 * last eased to toward where they are asked to be, for the parts easedParts
 * names. The first call for a part starts it where it is asked to be.
 */
void EaseReactionView(f32 *eye, f32 *lookAt, f32 rate, s32 easedParts) {
    if (easedParts & EASE_EYE) {
        if (!sEasedView.isEyeEased) {
            sEasedView.eye[AXIS_X] = eye[AXIS_X];
            sEasedView.eye[AXIS_Y] = eye[AXIS_Y];
            sEasedView.eye[AXIS_Z] = eye[AXIS_Z];
            sEasedView.isEyeEased = TRUE;
        }
        sEasedView.eye[AXIS_X] = eye[AXIS_X] = BlendClamped(sEasedView.eye[AXIS_X], eye[AXIS_X], rate);
        sEasedView.eye[AXIS_Y] = eye[AXIS_Y] = BlendClamped(sEasedView.eye[AXIS_Y], eye[AXIS_Y], rate);
        sEasedView.eye[AXIS_Z] = eye[AXIS_Z] = BlendClamped(sEasedView.eye[AXIS_Z], eye[AXIS_Z], rate);
    }
    if (easedParts & EASE_LOOK_AT) {
        if (!sEasedView.isLookAtEased) {
            sEasedView.lookAt[AXIS_X] = lookAt[AXIS_X];
            sEasedView.lookAt[AXIS_Y] = lookAt[AXIS_Y];
            sEasedView.lookAt[AXIS_Z] = lookAt[AXIS_Z];
            sEasedView.isLookAtEased = TRUE;
        }
        sEasedView.lookAt[AXIS_X] = lookAt[AXIS_X] = BlendClamped(sEasedView.lookAt[AXIS_X], lookAt[AXIS_X], rate);
        sEasedView.lookAt[AXIS_Y] = lookAt[AXIS_Y] = BlendClamped(sEasedView.lookAt[AXIS_Y], lookAt[AXIS_Y], rate);
        sEasedView.lookAt[AXIS_Z] = lookAt[AXIS_Z] = BlendClamped(sEasedView.lookAt[AXIS_Z], lookAt[AXIS_Z], rate);
    }
}

/*
 * Sets the character's two lights and the reaction's third light, if one is
 * on. It returns whether it set them.
 */
s32 AddReactionLight(Gfx **displayListHead, Lights2 *lights) {
    Gfx *gfx = *displayListHead;

    if (sHasReactionLight) {
        sReactionLights.l[0] = lights->l[0];
        sReactionLights.l[1] = lights->l[1];
        gSPSetLights3(gfx++, sReactionLights);
        *displayListHead = gfx;
        return TRUE;
    }
    *displayListHead = gfx;
    return FALSE;
}

/* Starts the character's reaction in the slot with the animation */
void StartReaction(u32 slotIndex, s32 character, s32 animation) {
    sReactionCharacter = character;
    sHasReactionLight = FALSE;
    sReactionFrame = 0;
    sEasedView.isEyeEased = FALSE;
    sEasedView.isLookAtEased = FALSE;
    sReactionAnimation = animation;
    sReactionFrameCount = GetAnimationFrameCount(slotIndex, animation);
}

void TurnOffReactionLight(void) {
    sHasReactionLight = FALSE;
}

/* Plays the sound if this is the frame after the given one */
void CueReactionSound(s32 frame, s32 soundId) {
    if (soundId != NO_REACTION_SOUND && sReactionFrame - 1 == frame) {
        PlaySound(soundId, sReactionChannel, REACTION_SOUND_VOLUME);
        sReactionChannel++;
        if (sReactionChannel > LAST_REACTION_CHANNEL) {
            sReactionChannel = FIRST_REACTION_CHANNEL;
        }
    }
}

/*
 * Cues the sounds of the reaction, by character and animation. It returns
 * no value; the s32 is there to match.
 */
s32 PlayReactionSounds(void) {
    if (g_Scene == SCENE_UNK30) {
        return;
    }
    switch (sReactionCharacter) {
        case CHARACTER_PLUM:
            switch (sReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(8, 0x1AC);
                    CueReactionSound(62, 0x3B);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(32, 0x1B9);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(30, 0x1A3);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(25, 0x1B7);
                    break;
            }
            break;
        case CHARACTER_CHARLIE:
            switch (sReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(0, 0xCC);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(0, 0xE1);
                    CueReactionSound(10, 0x3F);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(10, 0xD6);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(34, 0xE0);
                    break;
            }
            break;
        case CHARACTER_PEACH:
            switch (sReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(44, 0x15D);
                    CueReactionSound(92, 0x3D);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(12, 0x168);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(12, 0x169);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(8, 0x159);
                    break;
            }
            break;
        case CHARACTER_BABY_MARIO:
            switch (sReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(15, 0xC6);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(9, 0xC9);
                    CueReactionSound(57, 0xCA);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(32, 0xC9);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(40, 0xCB);
                    break;
            }
            break;
        case CHARACTER_LUIGI:
            switch (sReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(2, 0x150);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(0, 0x13F);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(18, 0x14E);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(7, 0x140);
                    break;
            }
            break;
        case CHARACTER_YOSHI:
            switch (sReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(0, 0x1A0);
                    CueReactionSound(56, 0x3B);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(6, 0x194);
                    CueReactionSound(6, 0x3C);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(12, 0x18E);
                    CueReactionSound(42, 0x3B);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(16, 0x19F);
                    break;
            }
            break;
        case CHARACTER_SONNY:
            switch (sReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(14, 0x1C4);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(2, 0x1D0);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(7, 0x1D1);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(0, 0x1C1);
                    break;
            }
            break;
        case CHARACTER_WARIO:
            switch (sReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(0, 0x1FF);
                    CueReactionSound(21, 0x3C);
                    CueReactionSound(88, 0x3B);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(0, 0x1F2);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(17, 0x1F3);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(0, 0x1EE);
                    CueReactionSound(28, 0x39);
                    break;
            }
            break;
        case CHARACTER_HARRY:
            switch (sReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(20, 0x105);
                    CueReactionSound(55, 0x3B);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(27, 0xFF);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(24, 0x103);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(5, 0xF5);
                    break;
            }
            break;
        case CHARACTER_MARIO:
        case CHARACTER_METAL_MARIO:
            switch (sReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(0, 0x180);
                    CueReactionSound(0, 0x3C);
                    CueReactionSound(68, 0x38);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(0, 0x173);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(13, 0x182);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(26, 0x17F);
                    break;
            }
            break;
        case CHARACTER_MAPLE:
            switch (sReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(66, 0x200);
                    CueReactionSound(38, 0x3E);
                    CueReactionSound(38, 0x35);
                    CueReactionSound(3, 0x4F);
                    CueReactionSound(11, 0x4F);
                    CueReactionSound(18, 0x4F);
                    CueReactionSound(25, 0x4F);
                    CueReactionSound(32, 0x4F);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(40, 0x208);
                    CueReactionSound(20, 0x4F);
                    CueReactionSound(38, 0x4F);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(28, 0x209);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(0, 0x206);
                    break;
            }
            break;
        case CHARACTER_DONKEY_KONG:
            switch (sReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(7, 0x42);
                    CueReactionSound(30, 0xE2);
                    CueReactionSound(35, 0x41);
                    CueReactionSound(40, 0x41);
                    CueReactionSound(45, 0x41);
                    CueReactionSound(50, 0x41);
                    CueReactionSound(55, 0x41);
                    CueReactionSound(60, 0x41);
                    CueReactionSound(70, 0xEA);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(35, 0xEA);
                    CueReactionSound(22, 0x41);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(35, 0xEA);
                    CueReactionSound(17, 0x41);
                    CueReactionSound(31, 0x41);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(0, 0xE8);
                    break;
            }
            break;
        case CHARACTER_BOWSER:
            switch (sReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(29, 0x12F);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(42, 0x136);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(0, 0x138);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(0, 0x135);
                    CueReactionSound(10, 0x3A);
                    CueReactionSound(24, 0x3A);
                    CueReactionSound(33, 0x3A);
                    break;
            }
            break;
        case CHARACTER_KID:
            switch (sReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(0, 0x123);
                    CueReactionSound(66, 0x12E);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(0, 0x12E);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(32, 0x11E);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(25, 0x120);
                    break;
            }
            break;
        case CHARACTER_SHERRY:
            switch (sReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(0, 0x1DE);
                    CueReactionSound(6, 0x3C);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(20, 0x1D7);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(0, 0x1D4);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(24, 0x1E8);
                    break;
            }
            break;
        case CHARACTER_AZALEA:
            switch (sReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(25, 0xA7);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(5, 0xB2);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(38, 0xAE);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(0, 0xB1);
                    break;
            }
            break;
        case CHARACTER_JOE:
            switch (sReactionAnimation) {
                case REACTION_ANIMATION_EAGLE:
                    CueReactionSound(15, 0x11B);
                    break;
                case REACTION_ANIMATION_BIRDIE:
                    CueReactionSound(6, 0x11A);
                    break;
                case REACTION_ANIMATION_PAR:
                    CueReactionSound(0, 0x109);
                    break;
                case REACTION_ANIMATION_BOGEY:
                    CueReactionSound(0, 0x10C);
                    break;
            }
            break;
    }
}


/* Keeps the point CAMERA_GROUND_CLEARANCE above the ground, where heights grow downward */
void KeepAboveGround(f32 *point) {
    f32 highestHeight = (GetGroundHeight(point[AXIS_X] * WORLD_TO_COURSE_SCALE, point[AXIS_Z] * WORLD_TO_COURSE_SCALE)
                         - CAMERA_GROUND_CLEARANCE) * COURSE_TO_WORLD_SCALE;

    if (highestHeight < point[AXIS_Y]) {
        point[AXIS_Y] = highestHeight;
    }
}

INCLUDE_ASM("asm/nonmatchings/reaction", FrameMarioReaction);

INCLUDE_ASM("asm/nonmatchings/reaction", FrameWarioReaction);

INCLUDE_ASM("asm/nonmatchings/reaction", FrameBabyMarioReaction);

INCLUDE_ASM("asm/nonmatchings/reaction", FrameMapleReaction);

INCLUDE_ASM("asm/nonmatchings/reaction", FrameBowserReaction);

INCLUDE_ASM("asm/nonmatchings/reaction", FrameAzaleaReaction);

INCLUDE_ASM("asm/nonmatchings/reaction", FrameDonkeyKongReaction);

INCLUDE_ASM("asm/nonmatchings/reaction", FrameReaction);

INCLUDE_ASM("asm/nonmatchings/reaction", FrameYoshiReaction);

INCLUDE_ASM("asm/nonmatchings/reaction", FrameSherryReaction);

INCLUDE_ASM("asm/nonmatchings/reaction", FrameKidReaction);

/*
 * Advances the reaction in the slot by a frame, and frames it with the
 * first camera. It returns no value; the s32 is there to match.
 */
s32 UpdateReaction(u32 slotIndex) {
    char text[0x80];
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    CameraView *view;

    sReactionFrame++;
    DimNextTranslucentTriangles();
    TakePinOut();
    if (IsGameFlagSet(GAME_FLAG_SHOW_REACTION_FRAME)) {
        sprintf(text, "FRAME %d  ", sReactionFrame);
        PrintDebugText(text, REACTION_FRAME_COLUMN, REACTION_FRAME_ROW);
    }
    PlayReactionSounds();
    if (slot != NULL) {
        view = &g_Cameras[0].view;
        switch (slot->modelIndex % CHARACTER_COUNT) {
            case CHARACTER_YOSHI:
                FrameYoshiReaction(slotIndex, view);
                break;
            case CHARACTER_MARIO:
            case CHARACTER_METAL_MARIO:
                FrameMarioReaction(slotIndex, view);
                break;
            case CHARACTER_WARIO:
                FrameWarioReaction(slotIndex, view);
                break;
            case CHARACTER_MAPLE:
                FrameMapleReaction(slotIndex, view);
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
            case CHARACTER_PEACH:
            case CHARACTER_LUIGI:
            case CHARACTER_SONNY:
            case CHARACTER_HARRY:
            default:
                FrameReaction(slotIndex, view);
                break;
        }
    }
}

/* Forgets the reaction spot, so that the next spot considered is taken */
void ClearReactionSpot(void) {
    sHasReactionSpot = FALSE;
}

/*
 * Takes the point as the reaction spot if there is none, or if the ground
 * around it is less than half as rough as around the spot and the spot is
 * rough. Roughness adds up the height differences to the point from rings
 * of samples around it, the nearer rings counting more, and is high near a
 * billboard.
 */
s32 ConsiderReactionSpot(CoursePoint point) {
    s32 roughness;
    s32 ring;
    s32 height;
    s32 sample;
    s32 isNearBillboard;
    s32 i;
    Billboard *billboard;

    height = GetGroundHeight(point[AXIS_X], point[AXIS_Z]);
    roughness = 0;
    for (ring = 1; ring <= SPOT_RING_COUNT; ring++) {
        for (sample = 0; sample < SPOT_SAMPLES_PER_RING; sample++) {
            f32 angle = sample * SPOT_SAMPLE_ANGLE;
            f32 offsetX = cosf(angle) * ring * SPOT_RING_SPACING;
            f32 offsetZ = sinf(angle) * ring * SPOT_RING_SPACING;
            s32 difference = GetGroundHeight(point[AXIS_X] + offsetX, point[AXIS_Z] + offsetZ) - height;

            if (difference < 0) {
                difference = -difference;
            }
            roughness += difference / (ring + 1);
        }
    }
    roughness *= 2;
    if (!sHasReactionSpot) {
        isNearBillboard = FALSE;
        for (i = 0; i < BILLBOARD_COUNT; i++) {
            billboard = &g_Billboards[i];
            if ((s32)IntLength2D((billboard->x << BILLBOARD_UNIT_SHIFT) - point[AXIS_X],
                                 (billboard->z << BILLBOARD_UNIT_SHIFT) - point[AXIS_Z])
                    - (billboard->width << BILLBOARD_UNIT_SHIFT)
                < SPOT_BILLBOARD_CLEARANCE) {
                isNearBillboard = TRUE;
            }
        }
        if (isNearBillboard) {
            roughness = NEAR_BILLBOARD_ROUGHNESS;
        }
    }
    if (!sHasReactionSpot
        || (roughness * 2 < sReactionSpotRoughness && sReactionSpotRoughness > SPOT_SMOOTH_ENOUGH_ROUGHNESS)) {
        sReactionSpotRoughness = roughness;
        sReactionSpot[AXIS_X] = point[AXIS_X];
        sReactionSpot[AXIS_Y] = point[AXIS_Y];
        sReactionSpot[AXIS_Z] = point[AXIS_Z];
        sHasReactionSpot = TRUE;
    }
}

void GetReactionSpot(CoursePoint point) {
    point[AXIS_X] = sReactionSpot[AXIS_X];
    point[AXIS_Y] = sReactionSpot[AXIS_Y];
    point[AXIS_Z] = sReactionSpot[AXIS_Z];
}

/*
 * Stands a character on the ground SPOT_PLACE_DISTANCE from the reaction
 * spot, a quarter turn from the shot's aim, facing back along the aim. Its
 * pitch is refined twice so the slope matches the ground there.
 */
#ifdef NON_MATCHING
void PlaceAtReactionSpot(Vec3 position, Vec3 rotation, Vec3 scale) {
    f32 ahead[AXIS_COUNT];
    CoursePoint spot;
    u8 unusedBuffer[0x80]; /* Unused; it is there to match */
    f32 heading = ((g_CurrentShot.unk0 + QUARTER_BINARY_TURN) & BINARY_ANGLE_MASK) * BINARY_ANGLE_TO_RADIANS;
    f32 pitch;
    f32 distance;
    s32 i;
    f32 cosine;
    f32 absRadius;
    f32 radius = SPOT_PLACE_DISTANCE;

    spot[AXIS_X] = sReactionSpot[AXIS_X];
    spot[AXIS_Y] = sReactionSpot[AXIS_Y];
    spot[AXIS_Z] = sReactionSpot[AXIS_Z];
    ahead[AXIS_X] = cosf(heading) * radius + spot[AXIS_X];
    ahead[AXIS_Z] = sinf(heading) * radius + spot[AXIS_Z];
    ahead[AXIS_Y] = GetGroundHeight(ahead[AXIS_X], ahead[AXIS_Z]);
    pitch = atan2(ahead[AXIS_Y] - spot[AXIS_Y], radius);
    for (i = 0; i < PITCH_REFINEMENT_COUNT; i++) {
        cosine = cosf(pitch);
        absRadius = radius;
        if (absRadius < 0.0f) {
            absRadius = -absRadius;
        }
        distance = cosine * absRadius;
        position[AXIS_X] = cosf(heading) * distance + spot[AXIS_X];
        position[AXIS_Z] = sinf(heading) * distance + spot[AXIS_Z];
        position[AXIS_Y] = GetGroundHeight(position[AXIS_X], position[AXIS_Z]);
        rotation[AXIS_X] = atan2(position[AXIS_Y] - spot[AXIS_Y], distance);
        pitch = (pitch + rotation[AXIS_X]) * 0.5f;
    }
    scale[AXIS_Z] = scale[AXIS_Y] = scale[AXIS_X] = 1.0f;
    rotation[AXIS_X] = 0.0f;
    rotation[AXIS_Z] = 0;
    rotation[AXIS_Y] = ((-g_CurrentShot.unk0) & BINARY_ANGLE_MASK) * BINARY_ANGLE_TO_RADIANS;
}

#else
INCLUDE_ASM("asm/nonmatchings/reaction", PlaceAtReactionSpot);
#endif

#ifdef NON_MATCHING
s32 IsEasedEyeHeightZero(void) {
    s32 isZero = FALSE;

    if (fabsf(sEasedView.eye[AXIS_Y]) == 0.0f) {
        isZero = TRUE;
    }
    return isZero;
}

#else
INCLUDE_ASM("asm/nonmatchings/reaction", IsEasedEyeHeightZero);
#endif

__asm__(".section .rodata");
static const char sUnusedFloatFormat[] = "            %f";
static const char sUnusedEmptyText[] = "";
