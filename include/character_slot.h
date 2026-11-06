#ifndef CHARACTER_SLOT_H
#define CHARACTER_SLOT_H

#include <ultra64.h>

#include "camera.h"
#include "club.h"
#include "compiler.h"
#include "course_space.h"
#include "geometry.h"
#include "language.h"
#include "math_util.h"

#define CHARACTER_MODEL_KIND_COUNT 4
#define CHARACTER_MODELS_PER_KIND 36
/* The character that a model index shows, whichever kind of model it is */
#define ModelCharacter(modelIndex) ((modelIndex) % CHARACTER_MODELS_PER_KIND)
#define OTHER_CHARACTER_MODEL_COUNT 18
#define CHARACTER_MODEL_SEGMENT_ADDRESS_COUNT 15

/* What some of a CharacterModel's segment addresses hold */
/* The display list drawing the whole model */
#define CHARACTER_MODEL_DISPLAY_LIST 0
/* A command calling one of the display list choices */
#define CHARACTER_MODEL_CHOSEN_DISPLAY_LIST 1
/* Ended by END_OF_DISPLAY_LIST_CHOICES */
#define CHARACTER_MODEL_DISPLAY_LIST_CHOICES 2
/* CharacterPoints, ended by one with no joint path */
#define CHARACTER_MODEL_POINTS 4
/* CharacterAnimations, ended by one with no events */
#define CHARACTER_MODEL_ANIMATIONS 3
/* JointAnimations, ended by END_OF_JOINT_ANIMATIONS */
#define CHARACTER_MODEL_JOINTS 5
/* TextureAnimations, ended by a negative index */
#define CHARACTER_MODEL_TEXTURE_ANIMATIONS 6
/* AnimatedTriangle pointers, ended by NULL */
#define CHARACTER_MODEL_ANIMATED_TRIANGLES 7
/* AnimatedTextures */
#define CHARACTER_MODEL_TEXTURES 8
/*
 * Six f32 values, one in each segment address from here, copied to a slot's
 * portraitCamera: in 30 of the 36 character models, two points within 1.5
 * units of the same height, the first near the vertical axis and the second
 * several units off it. They look like a close-up camera's target and eye.
 */
#define CHARACTER_MODEL_PORTRAIT_CAMERA 9
#define PORTRAIT_CAMERA_VALUE_COUNT 6

#define END_OF_DISPLAY_LIST_CHOICES 0xFFFFFFFF
/* The display list choice that draws nothing */
#define NO_DISPLAY_LIST_CHOICE (-1)

/* Where SetMultiTlb maps a slot's model buffer, in TLB_PAGE_MODE_4K pages */
#define CHARACTER_MODEL_ADDRESS 0x02000000
#define CHARACTER_MODEL_PAGE_COUNT 32
#define TLB_PAGE_MODE_4K 0
#define SMALLEST_PAGE_SIZE 0x1000

/* Scales a unit light direction to the s8 range the lights take */
#define LIGHT_DIRECTION_SCALE 127.0f

/* Where a character model's files are in ROM and where its parts are in segment
 * 2 */
typedef struct CharacterModel {
  /* 0x00 */ u32 romStart;
  /* 0x04 */ u32 romEnd;
  /* 0x08 */ u32 segmentAddresses[CHARACTER_MODEL_SEGMENT_ADDRESS_COUNT];
  /* 0x44 */ s16 shadowScale; /* Copied to CharacterSlot.shadowScale */
  /* The model loads this file first, then patches it with romStart's when the
   * two differ */
  /* 0x48 */ u32 baseRomStart;
  /* 0x4C */ u32 baseRomEnd;
  /* The part of the model a slot copies for each frame, its joint matrices
   * first */
  /* 0x50 */ u32 frameDataStart;
  /* 0x54 */ u32 frameDataEnd;
} CharacterModel; /* size = 0x58 */

#define CHARACTER_SLOT_COUNT 4
/* A slot's model index when no character is loaded into it */
#define NO_CHARACTER_MODEL (-1)

/* A point on a character model, found by moving through a path of joints */
typedef struct CharacterPoint {
  /* 0x00 */ s16*
      jointPath; /* Joint matrix indices, ended by END_OF_JOINT_PATH */
  /* 0x04 */ f32 offset[3]; /* From the last joint */
  /* 0x10 */ s16 id;
} CharacterPoint; /* size = 0x14 */

#define END_OF_JOINT_PATH (-1)

/* The CharacterPoint.id of the point the shadow lies under */
#define CHARACTER_POINT_SHADOW 2

/* guRotateRPYF's roll for a slot with no roll */
#define ROLL_ORIGIN_DEGREES 180

/* The keys around the current frame, kept from frame to frame */
typedef struct KeyframeSpan {
  /* 0x0 */ s16 startFrame;
  /* 0x2 */ s16 endFrame;
  /* 0x4 */ s16 index; /* Of the key at startFrame */
} KeyframeSpan; /* size = 0x6 */

/* A joint's keyed translation, rotation and scale */
typedef struct JointAnimation {
  /* 0x00 */ s16 translationKeyCount;
  /* 0x02 */ s16 rotationKeyCount;
  /* 0x04 */ s16 scaleKeyCount;
  /* 0x08 */ s16* translationFrames;
  /* 0x0C */ s16* rotationFrames;
  /* 0x10 */ s16* scaleFrames;
  /* 0x14 */ f32 (*translations)[3];
  /* 0x18 */ s16 (*rotations)[4]; /* Quaternions (x, y, z, w) */
  /* 0x1C */ f32 (*scales)[3];
  /* 0x20 */ KeyframeSpan translationSpan;
  /* 0x26 */ KeyframeSpan rotationSpan;
  /* 0x2C */ KeyframeSpan scaleSpan;
} JointAnimation; /* size = 0x34 */

/* A KeyframeSpan frame that makes AnimateCharacterJoints look for the keys
 * again */
#define UNSET_KEYFRAME_SPAN (-1)

/* The translation key count of the joint ending a model's list */
#define END_OF_JOINT_ANIMATIONS (-1)

/* Rotation keys are quaternions in units of QUATERNION_SCALE */
#define QUATERNION_SCALE (1.0f / 16384)
#define QUATERNION_PRODUCT_SCALE (1.0f / (16384 * 16384))
/* The least 1 - cos of the angle between rotations that are slerped */
#define SLERP_THRESHOLD 1e-06

/* Something that happens on a frame of a CharacterAnimation */
typedef struct AnimationEvent {
  /* 0x0 */ s16 frame; /* END_OF_ANIMATION_EVENTS ends the list */
  /* 0x2 */ u8 kind;
  /* 0x3 */ u8 value;
} AnimationEvent; /* size = 0x4 */

#define END_OF_ANIMATION_EVENTS (-1)

/* AnimationEvent kinds */
enum AnimationEventKind {
  ANIMATION_EVENT_LOOP_START,
  /* Jumps back to the loop start when the slot is looping */
  ANIMATION_EVENT_LOOP_END,
  /* Where a swing strikes the ball; UpdateTurns launches it then */
  ANIMATION_EVENT_IMPACT = 6,
  /* Starts the TextureAnimation that value names */
  ANIMATION_EVENT_TEXTURE_ANIMATION = 0xFE
};

/* What an AnimationEvent does, as gathered for its frame */
typedef struct AnimationEventAction {
  /* 0x0 */ u8 kind;
  /* 0x1 */ u8 value;
} AnimationEventAction; /* size = 0x2 */

/* The most AnimationEventActions gathered for one frame */
#define EVENT_ACTION_COUNT_MAX 16

/* Values of an ANIMATION_EVENT_TEXTURE_ANIMATION event that do something else
 * instead */
/* Starts playing RANDOM_TEXTURE_ANIMATION now and then */
enum TextureAnimationEvent {
  TEXTURE_ANIMATION_EVENT_START_RANDOM,
  TEXTURE_ANIMATION_EVENT_STOP_RANDOM = 2
};

/* A span of a model's joint keyframes, played as one animation */
typedef struct CharacterAnimation {
  /* 0x0 */ AnimationEvent* events;
  /* 0x4 */ s16 startFrame;
  /* 0x6 */ s16 frameCount;
  /* 0x8 */ u8 id;
} CharacterAnimation; /* size = 0xC */

/* How many frames a TextureAnimation runs, and which of a slot's states runs it
 */
typedef struct TextureAnimation {
  /* 0x0 */ s16 index; /* Into a slot's textureAnimations */
  /* 0x2 */ s16 frameCount;
} TextureAnimation; /* size = 0x4 */

/* The TextureAnimation that plays now and then by itself */
#define RANDOM_TEXTURE_ANIMATION 0

/* An RGBA16 texture an AnimatedTriangle can show */
typedef struct AnimatedTexture {
  /* 0x0 */ u32 address;
  /* 0x4 */ u8 width;
  /* 0x5 */ u8 height;
} AnimatedTexture; /* size = 0x8 */

#define TRIANGLE_VERTEX_COUNT 3

/* A change of an AnimatedTriangle's texture from a frame of its
 * TextureAnimation */
typedef struct TriangleTextureKey {
  /* 0x0 */ s16 frame;
  /* 0x2 */ s16 textureIndex;
  /* 0x4 */ s16 s[TRIANGLE_VERTEX_COUNT];
  /* 0xA */ s16 t[TRIANGLE_VERTEX_COUNT];
} TriangleTextureKey; /* size = 0x10 */

/* The keys one TextureAnimation gives an AnimatedTriangle */
typedef struct TriangleTextureKeys {
  /* 0x0 */ s16 count;
  /* 0x2 */ s16 textureAnimationIndex;
  /* 0x4 */ TriangleTextureKey* keys;
} TriangleTextureKeys; /* size = 0x8 */

/* A triangle whose texture TextureAnimations change, by patching a slot's frame
 * data */
typedef struct AnimatedTriangle {
  /* Segment addresses into the frame data, whose low 24 bits are offsets into a
   * copy */
  /* 0x00 */ u32 vertices; /* Its three Vtx */
  /* 0x04 */ u32
      loadTextureBlock; /* 0x10 bytes before its gDPLoadTextureBlock */
  /* 0x08 */ TriangleTextureKeys* keySets;
  /* 0x0C */ s16 keySetCount;
  /* 0x0E */ s16
      textureIndex; /* Shown when no key applies; negative for the first */
  /* 0x10 */ s16 s[TRIANGLE_VERTEX_COUNT];
  /* 0x16 */ s16 t[TRIANGLE_VERTEX_COUNT];
} AnimatedTriangle; /* size = 0x1C */

#define TEXTURE_ANIMATION_COUNT 32

/*
 * The state of a TextureAnimation in a slot. A frame data copy is patched
 * only while pendingCopyCount is nonzero, which a change sets to cover each
 * copy once.
 */
typedef struct TextureAnimationState {
  /* 0x0 */ s32 frame;
  /* 0x4 */ s8 pendingCopyCount; /* TEXTURE_ANIMATION_PLAYING while it plays */
} TextureAnimationState; /* size = 0x8 */

/* A slot keeps three copies of its frame data, so the one being written is
 * never one being drawn */
#define FRAME_DATA_COPY_COUNT 3
#define TEXTURE_ANIMATION_PLAYING (FRAME_DATA_COPY_COUNT + 1)

/* A character loaded for drawing */
typedef struct CharacterSlot {
  /* 0x000 */ const CharacterModel* model;
  /* 0x004 */ void* modelBuffer; /* Where the model's files are loaded */
  /* 0x008 */ s32 modelIndex;
  /* 0x00C */ s32 animationIndex;
  /* 0x010 */ f32 animationFrame;
  /* 0x014 */ s32 animationStartFrame;
  /* 0x018 */ s32 animationEndFrame;
  /* 0x01C */ s32 animationCount;
  /* 0x020 */ void* frameData[FRAME_DATA_COPY_COUNT];
  /* 0x02C */ Mtx* jointMatrices;
  /* 0x030 */ s32 frameDataIndex; /* Of the copy the next frame writes */
  /* 0x034 */ u32 lastDrawnFrame;
  /* 0x038 */ f32 position[3]; /* In 1024ths of a world unit */
  /* 0x044 */ f32 rotation[3]; /* In radians */
  /* 0x050 */ f32 scale[3];
  /* 0x05C */ f32 animationSpeed; /* In frames per step */
  /* 0x060 */ s8 isTranslucent;
  /* 0x061 */ u8 alpha;
  /* 0x064 */ Gfx* displayList;
  /* Written but never read */
  /* 0x068 */ f32 portraitCamera[PORTRAIT_CAMERA_VALUE_COUNT];
  /* 0x080 */ s16 shadowScale; /* In units of 1/10000 */
  /* 0x082 */ s8 lighting; /* A CHARACTER_LIGHTING value */
  /* 0x083 */ s8 isLooping;
  /* 0x084 */ s8 nextAnimationId; /* NO_NEXT_ANIMATION, or the one to play after
                                     this one ends */
  /* 0x085 */ s8 playsRandomTextureAnimation;
  /* 0x086 */ s8 randomTextureAnimationDelay; /* In steps */
  /* 0x088 */ TextureAnimationState textureAnimations[TEXTURE_ANIMATION_COUNT];
  /* 0x188 */ s8 usesFrontCulling; /* Set for a lefty golfer */
  /* 0x189 */ s8 reusesLastMatrices; /* For the next draw, which clears it */
} CharacterSlot; /* size = 0x18C */

#define NO_NEXT_ANIMATION (-1)

/* Each club kind has three animations in a row: a stance, a swing and an
 * approach swing */
#define WOOD_ANIMATIONS 0
#define IRON_ANIMATIONS 3
#define WEDGE_ANIMATIONS 6
#define PUTTER_ANIMATIONS 9
#define SWING_ANIMATION 1

/* CharacterSlot.lighting values: the lights the slot's character is drawn with
 */
/* Lights that don't follow the scene's light direction */
enum CharacterLighting {
  CHARACTER_LIGHTING_FIXED,
  /* Lit along the scene's light direction, in the weather's colors */
  CHARACTER_LIGHTING_SCENE,
  /* As above, with AddReactionLight's third light when it adds one */
  CHARACTER_LIGHTING_SCENE_THIRD,
  CHARACTER_LIGHTING_BLACK
};

extern CharacterSlot g_CharacterSlots[CHARACTER_SLOT_COUNT];

/* Display lists drawn before the characters, rebuilt by SetCharacterAlpha and
 * SetCharacterHilite */
extern Gfx g_CharacterRenderModeDisplayList[3];
extern Gfx g_CharacterCombineDisplayList[2];
extern Gfx g_CharacterHiliteDisplayList[3];
#define ENV_COLOR_COUNT 10
/* The RGB that SetCharacterAlpha gives each of the env color display lists */
extern u8 g_CharacterEnvColors[ENV_COLOR_COUNT][3];
/* One env color and an end, for each of g_CharacterEnvColors, at the alpha of
 * SetCharacterAlpha */
extern Gfx g_CharacterEnvColorDisplayLists[ENV_COLOR_COUNT][2];

/* The animation state AnimateCharacterJoints works on, copied from a slot */
extern f32 g_CurrentAnimationFrame;
extern s32 g_CurrentAnimationEndFrame;
extern Mtx* g_CurrentJointMatrices;

/* The model index of the slot last set up or asked about */
extern s32 g_CharacterModelIndex;

extern const CharacterModel g_CharacterModels[CHARACTER_MODEL_KIND_COUNT]
                                             [CHARACTER_MODELS_PER_KIND];
extern const CharacterModel g_OtherCharacterModels[OTHER_CHARACTER_MODEL_COUNT];

void SetCharacterAlpha(s32 alpha);
void AnimateCharacterJoints(u32 slotIndex);
NO_RESULT FreeCharacterSlot(u32 slotIndex);
Boolean IsCharacterSlotBusy(u32 slotIndex);
Boolean IsAnyCharacterSlotBusy(void);
CharacterSlot* GetUsedCharacterSlot(u32 slotIndex);
void FreeAllCharacterSlots(void);
s32 SetMultiTlb(s32 firstTlbIndex, s32 pageMode, u32 pageCount,
    u32 virtualAddress, u32 physicalAddress);
s32 SelectCharacterDisplayList(u32 slotIndex, s32 choiceIndex);
ClubKind GetCharacterClubKind(u32 slotIndex, s32 clubIndex);
s32 GetCharacterPointModelPosition(u32 slotIndex, s32 pointId, f32* position);
void GetCharacterSlotMatrix(CharacterSlot* slot, Mat4 matrix);
s32 GetCharacterPointPosition(u32 slotIndex, s32 pointId, f32* position);
void GetCharacterShadowPoint(u32 slotIndex, f32* position);

/* Defined in character.c */
s32 GetBallDistance(
    u32 slotIndex, s32 club, Boolean isClubKind, ShotMode shotMode);
void LoadCharacter(u32 slotIndex, s32 character, s32 kind);
void SetCharacterAnimation(u32 slotIndex, s32 id);
void SetAnimationLooping(u32 slotIndex, s32 value);
void SetNextAnimation(u32 slotIndex, s32 value);
s32 GetCurrentAnimationEndFrame(u32 slotIndex);
s32 GetAnimationEndFrame(u32 slotIndex, s32 id);
s32 GetAnimationFrame(u32 slotIndex);
void SkipToAnimationEnd(u32 slotIndex);
void SetAnimationSpeed(u32 slotIndex, f32 speed);
void AnimateCharacter(u32 slotIndex);
void AnimateCharacterInSameCopy(u32 slotIndex);
void SetCharacterLights(void);
void SetCharacterClub(u32 slotIndex, s32 clubIndex);
s32 GetAnimationEventFrame(u32 slotIndex, s32 animationIndex, s32 kind);
s32 GetAnimationIndex(u32 slotIndex, s32 id);
void AdvanceAnimation(u32 slotIndex, s32 frameCount);
void SetCharacterLighting(u32 slotIndex, s32 value);
void AnimateAllCharacters(void);
void PlaceAndViewCharacter(u32 slotIndex);
void PlaceCharacter(u32 slotIndex, CoursePoint point, Radians heading);
void DrawCharacter(u32 slotIndex);
void DrawCharacterAndShadow(
    CameraMatrices* matrices, s32 cameraIndex, s32 slotIndex);
void SetCharacterHilite(CameraMatrices* matrices UNUSED);

#endif
