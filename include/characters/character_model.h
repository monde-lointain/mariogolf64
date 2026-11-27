/*
 * The slots that hold the loaded characters, their models' joints and display
 * lists, and the positions of points on them.
 */

#ifndef CHARACTER_MODEL_H
#define CHARACTER_MODEL_H

#include <ultra64.h>

#include "camera/camera.h"
#include "characters/character.h"
#include "course/course_space.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "screen/display_list.h"
#include "shot/club.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/memory_address.h"

#define CHARACTER_MODEL_KIND_COUNT 4
#define CHARACTER_MODELS_PER_KIND 36
// The character that a model index shows, whichever kind of model it is
#define ModelCharacter(modelIndex) ((modelIndex) % CHARACTER_MODELS_PER_KIND)
// The model at an index across all the kinds, found by its row and column, as
// the index passes the first row of g_CharacterModels
#define CharacterModelAt(modelIndex) \
    (&g_CharacterModels[(modelIndex) / CHARACTER_MODELS_PER_KIND][ModelCharacter(modelIndex)])
#define OPENING_CHARACTER_MODEL_COUNT 18
#define CHARACTER_MODEL_SEGMENT_ADDRESS_COUNT 15

// What some of a CharacterModel's segment addresses hold
// The display list drawing the whole model
#define CHARACTER_MODEL_DISPLAY_LIST 0
// A command calling one of the display list choices
#define CHARACTER_MODEL_CHOSEN_DISPLAY_LIST 1
// Ended by END_OF_DISPLAY_LIST_CHOICES
#define CHARACTER_MODEL_DISPLAY_LIST_CHOICES 2
// CharacterPoints, ended by one with no joint path
#define CHARACTER_MODEL_POINTS 4
// CharacterAnimations, ended by one with no events
#define CHARACTER_MODEL_ANIMATIONS 3
// JointAnimations, ended by END_OF_JOINT_ANIMATIONS
#define CHARACTER_MODEL_JOINTS 5
// TextureAnimations, ended by a negative index
#define CHARACTER_MODEL_TEXTURE_ANIMATIONS 6
// AnimatedTriangle pointers, ended by NULL
#define CHARACTER_MODEL_ANIMATED_TRIANGLES 7
// AnimatedTextures
#define CHARACTER_MODEL_TEXTURES 8
// Six f32 values, one in each segment address from here, copied to a slot's
// portraitCamera: in 30 of the 36 character models, two points within 1.5
// units of the same height, the first near the vertical axis and the second
// several units off it. They look like a close-up camera's target and eye.
#define CHARACTER_MODEL_PORTRAIT_CAMERA 9
#define PORTRAIT_CAMERA_VALUE_COUNT 6

#define END_OF_DISPLAY_LIST_CHOICES 0xFFFFFFFF
// The display list choice that draws nothing
#define NO_DISPLAY_LIST_CHOICE (-1)

// Where SetMultiTlb maps a slot's model buffer, in TLB_PAGE_MODE_4K pages
#define CHARACTER_MODEL_ADDRESS 0x02000000
#define CHARACTER_MODEL_PAGE_COUNT 32

enum TlbPageMode {
    TLB_PAGE_MODE_4K,
    TLB_PAGE_MODE_8K,
    TLB_PAGE_MODE_16K,
    TLB_PAGE_MODE_32K,
    TLB_PAGE_MODE_64K,
    TLB_PAGE_MODE_128K,
    TLB_PAGE_MODE_256K,
    TLB_PAGE_MODE_512K
};

typedef s32 TlbPageMode;
#define SMALLEST_PAGE_SIZE 0x1000

// Scales a unit light direction to the s8 range the lights take
#define LIGHT_DIRECTION_SCALE 127.0f

// Where a character model's files are in ROM and where its parts are in segment
// 2
typedef struct CharacterModel {
    u32 romStart;
    u32 romEnd;
    u32 segmentAddresses[CHARACTER_MODEL_SEGMENT_ADDRESS_COUNT];
    s16 shadowScale; // Copied to CharacterSlot.shadowScale
    // The model loads this file first, then patches it with romStart's when the
    // two differ
    u32 baseRomStart;
    u32 baseRomEnd;
    // The part of the model a slot copies for each frame, its joint matrices
    // first
    u32 frameDataStart;
    u32 frameDataEnd;
} CharacterModel;

#define CHARACTER_SLOT_COUNT 4
// A slot's model index when no character is loaded into it
#define NO_CHARACTER_MODEL (-1)

// A point on a character model, found by moving through a path of joints
typedef struct CharacterPoint {
    s16 *jointPath; // Joint matrix indices, ended by END_OF_JOINT_PATH
    Vec3 offset; // From the last joint
    s16 id;
} CharacterPoint;

#define END_OF_JOINT_PATH (-1)

// The CharacterPoint.id of the point the shadow lies under
#define CHARACTER_POINT_SHADOW 2

enum PointLookup {
    POINT_LOOKUP_NO_SLOT = -1,
    POINT_LOOKUP_FOUND,
    POINT_LOOKUP_NO_POINT
};

typedef s32 PointLookup;

// guRotateRPYF's roll for a slot with no roll
#define ROLL_ORIGIN_DEGREES 180

// The keys around the current frame, kept from frame to frame
typedef struct KeyframeSpan {
    s16 startFrame;
    s16 endFrame;
    s16 startKeyIndex; // Of the key at startFrame
} KeyframeSpan;

#define QUATERNION_W 3
#define QUATERNION_COMPONENT_COUNT 4
typedef f32 Quaternion[QUATERNION_COMPONENT_COUNT];

// A joint's keyed translation, rotation and scale
typedef struct JointAnimation {
    s16 translationKeyCount;
    s16 rotationKeyCount;
    s16 scaleKeyCount;
    s16 *translationFrames;
    s16 *rotationFrames;
    s16 *scaleFrames;
    Vec3 *translations;
    s16 (*rotations)[QUATERNION_COMPONENT_COUNT]; // Quaternions (x, y, z, w)
    Vec3 *scales;
    KeyframeSpan translationSpan;
    KeyframeSpan rotationSpan;
    KeyframeSpan scaleSpan;
} JointAnimation;

// A KeyframeSpan frame that makes AnimateCharacterJoints look for the keys
// again
#define UNSET_KEYFRAME_SPAN (-1)

// The translation key count of the joint ending a model's list
#define END_OF_JOINT_ANIMATIONS (-1)

// Rotation keys are quaternions in units of QUATERNION_SCALE
#define QUATERNION_SCALE (1.0f / 16384)
#define QUATERNION_PRODUCT_SCALE (1.0f / (16384 * 16384))
// The least 1 - cos of the angle between rotations that are slerped
#define SLERP_THRESHOLD 1e-06

// Sets the matrix to the rotation of the quaternion (x, y, z, w), whatever its
// length; a zero quaternion gives the identity.
static inline void QuaternionToMatrix(Quaternion quaternion, Mat4 matrix) {
    f32 lengthSquared = (quaternion[AXIS_X] * quaternion[AXIS_X]) +
        (quaternion[AXIS_Y] * quaternion[AXIS_Y]) + (quaternion[AXIS_Z] * quaternion[AXIS_Z]) +
        (quaternion[QUATERNION_W] * quaternion[QUATERNION_W]);
    f32 scale = (lengthSquared > 0.0f) ? (2.0f / lengthSquared) : 0.0f;
    // The components scaled, then their products with the unscaled ones
    f32 xs = quaternion[AXIS_X] * scale;
    f32 ys = quaternion[AXIS_Y] * scale;
    f32 zs = quaternion[AXIS_Z] * scale;
    f32 wx = quaternion[QUATERNION_W] * xs;
    f32 wy = quaternion[QUATERNION_W] * ys;
    f32 wz = quaternion[QUATERNION_W] * zs;
    f32 xx = quaternion[AXIS_X] * xs;
    f32 yy = quaternion[AXIS_Y] * ys;
    f32 zz = quaternion[AXIS_Z] * zs;
    f32 xy = quaternion[AXIS_X] * ys;
    f32 yz = quaternion[AXIS_Y] * zs;
    f32 xz = quaternion[AXIS_X] * zs;

    matrix[0][0] = 1.0f - (yy + zz);
    matrix[1][0] = xy - wz;
    matrix[2][0] = xz + wy;
    matrix[0][1] = xy + wz;
    matrix[1][1] = 1.0f - (xx + zz);
    matrix[2][1] = yz - wx;
    matrix[0][2] = xz - wy;
    matrix[1][2] = yz + wx;
    matrix[2][2] = 1.0f - (xx + yy);
    matrix[0][3] = matrix[1][3] = matrix[2][3] = 0.0f;
    matrix[3][0] = matrix[3][1] = matrix[3][2] = 0.0f;
    matrix[3][3] = 1.0f;
}

#define END_OF_ANIMATION_EVENTS (-1) // END_OF_ANIMATION_EVENTS ends the list

// AnimationEvent kinds
enum AnimationEventKind {
    ANIMATION_EVENT_LOOP_START,
    // Jumps back to the loop start when the slot is looping
    ANIMATION_EVENT_LOOP_END,
    // Where a swing strikes the ball; UpdateTurns launches it then
    ANIMATION_EVENT_IMPACT = 6,
    // Starts the TextureAnimation that value names
    ANIMATION_EVENT_TEXTURE_ANIMATION = 0xFE
};

typedef s32 AnimationEventKind;
typedef u8 AnimationEventKind8;

// Something that happens on a frame of a CharacterAnimation
typedef struct AnimationEvent {
    s16 frame; // END_OF_ANIMATION_EVENTS ends the list
    AnimationEventKind8 kind;
    u8 value;
} AnimationEvent;

// What an AnimationEvent does, as gathered for its frame
typedef struct AnimationEventAction {
    AnimationEventKind8 kind;
    u8 value;
} AnimationEventAction;

// The most AnimationEventActions gathered for one frame
#define EVENT_ACTION_COUNT_MAX 16

// Values of an ANIMATION_EVENT_TEXTURE_ANIMATION event that do something else
// instead
enum TextureAnimationEvent {
    // Starts playing RANDOM_TEXTURE_ANIMATION now and then
    TEXTURE_ANIMATION_EVENT_START_RANDOM,
    TEXTURE_ANIMATION_EVENT_STOP_RANDOM = 2
};

// A span of a model's joint keyframes, played as one animation
typedef struct CharacterAnimation {
    AnimationEvent *events;
    s16 startFrame;
    s16 frameCount;
    UAnimationId8 id;
} CharacterAnimation;

// How many frames a TextureAnimation runs, and which of a slot's states runs it
typedef struct TextureAnimation {
    s16 index; // Into a slot's textureAnimations
    s16 frameCount;
} TextureAnimation;

// The TextureAnimation that plays now and then by itself
#define RANDOM_TEXTURE_ANIMATION 0

// An RGBA16 texture an AnimatedTriangle can show
typedef struct AnimatedTexture {
    u32 address;
    u8 width;
    u8 height;
} AnimatedTexture;

// A change of an AnimatedTriangle's texture from a frame of its
// TextureAnimation
typedef struct TriangleTextureKey {
    s16 frame;
    s16 textureIndex;
    s16 s[TRIANGLE_VERTEX_COUNT];
    s16 t[TRIANGLE_VERTEX_COUNT];
} TriangleTextureKey;

// The keys one TextureAnimation gives an AnimatedTriangle
typedef struct TriangleTextureKeys {
    s16 count;
    s16 textureAnimationIndex;
    TriangleTextureKey *keys;
} TriangleTextureKeys;

// A triangle whose texture TextureAnimations change, by patching a slot's frame
// data
typedef struct AnimatedTriangle {
    // Segment addresses into the frame data, whose low 24 bits are offsets into a
    // copy
    u32 vertices; // Its three Vtx
    u32 loadTextureBlock; // 0x10 bytes before its gDPLoadTextureBlock
    TriangleTextureKeys *keySets;
    s16 keySetCount;
    s16 textureIndex; // Shown when no key applies; negative for the first
    s16 s[TRIANGLE_VERTEX_COUNT];
    s16 t[TRIANGLE_VERTEX_COUNT];
} AnimatedTriangle;

#define TEXTURE_ANIMATION_COUNT 32

// The state of a TextureAnimation in a slot. A frame data copy is patched
// only while pendingCopyCount is nonzero, which a change sets to cover each
// copy once.
typedef struct TextureAnimationState {
    s32 frame;
    s8 pendingCopyCount; // TEXTURE_ANIMATION_PLAYING while it plays
} TextureAnimationState;

// A slot keeps three copies of its frame data, so the one being written is
// never one being drawn
#define FRAME_DATA_COPY_COUNT 3
#define TEXTURE_ANIMATION_PLAYING (FRAME_DATA_COPY_COUNT + 1)

// CharacterSlot.lighting values: the lights the slot's character is drawn with
enum CharacterLighting {
    CHARACTER_LIGHTING_FIXED, // Lights that don't follow the scene's light direction
    CHARACTER_LIGHTING_SCENE, // Lit along the scene's light direction, in the weather's colors
    // As above, with AddReactionLight's third light when it adds one
    CHARACTER_LIGHTING_SCENE_THIRD,
    CHARACTER_LIGHTING_BLACK
};

typedef s32 CharacterLighting;
typedef s8 CharacterLighting8;

// A character loaded for drawing
typedef struct CharacterSlot {
    const CharacterModel *model;
    void *modelBuffer; // Where the model's files are loaded
    s32 modelIndex;
    s32 animationIndex;
    f32 animationFrame;
    s32 animationStartFrame;
    s32 animationEndFrame;
    s32 animationCount;
    void *frameData[FRAME_DATA_COPY_COUNT];
    Mtx *jointMatrices;
    s32 frameDataIndex; // Of the copy the next frame writes
    u32 lastDrawnFrame;
    Vec3 position; // In 1024ths of a world unit
    Vec3 rotation; // In radians
    Vec3 scale;
    f32 animationSpeed; // In frames per step
    Boolean8 isTranslucent;
    u8 alpha;
    Gfx *displayList;
    // Written but never read
    f32 portraitCamera[PORTRAIT_CAMERA_VALUE_COUNT];
    s16 shadowScale; // In units of 1/10000
    CharacterLighting8 lighting; // A CHARACTER_LIGHTING value
    Boolean8 isLooping;
    AnimationId8 nextAnimationId; // NO_NEXT_ANIMATION, or the one to play after this one ends
    Boolean8 playsRandomTextureAnimation;
    s8 randomTextureAnimationDelay; // In steps
    TextureAnimationState textureAnimations[TEXTURE_ANIMATION_COUNT];
    Boolean8 usesFrontCulling; // Set for a lefty golfer
    Boolean8 reusesLastMatrices; // For the next draw, which clears it
} CharacterSlot;

#define NO_NEXT_ANIMATION (-1)

// Each club kind has three animations in a row: a stance, a swing and an
// approach swing
#define WOOD_ANIMATIONS 0
#define IRON_ANIMATIONS 3
#define WEDGE_ANIMATIONS 6
#define PUTTER_ANIMATIONS 9
#define SWING_ANIMATION 1

extern CharacterSlot g_CharacterSlots[CHARACTER_SLOT_COUNT];

// Display lists drawn before the characters, rebuilt by SetCharacterAlpha and
// SetCharacterHilite
extern Gfx g_CharacterRenderModeDisplayList[3];
extern Gfx g_CharacterCombineDisplayList[2];
extern Gfx g_CharacterHighlightDisplayList[3];
#define ENV_COLOR_COUNT 10
// The RGB that SetCharacterAlpha gives each of the env color display lists
extern u8 g_CharacterEnvColors[ENV_COLOR_COUNT][RGB_CHANNEL_COUNT];
// One env color and an end, for each of g_CharacterEnvColors, at the alpha of
// SetCharacterAlpha
extern Gfx g_CharacterEnvColorDisplayLists[ENV_COLOR_COUNT][2];

// The animation state AnimateCharacterJoints works on, copied from a slot
extern f32 g_CurrentAnimationFrame;
extern s32 g_CurrentAnimationEndFrame;
extern Mtx *g_CurrentJointMatrices;

// The model index of the slot last set up or asked about
extern s32 g_CharacterModelIndex;

extern const CharacterModel g_CharacterModels[CHARACTER_MODEL_KIND_COUNT]
                                             [CHARACTER_MODELS_PER_KIND];
extern const CharacterModel g_OpeningCharacterModels[OPENING_CHARACTER_MODEL_COUNT];

void SetCharacterAlpha(s32 alpha);
void AnimateCharacterJoints(u32 slotIndex);
NO_RESULT FreeCharacterSlot(u32 slotIndex);
Boolean IsCharacterSlotBusy(u32 slotIndex);
Boolean IsAnyCharacterSlotBusy(void);
CharacterSlot *GetUsedCharacterSlot(u32 slotIndex);
void FreeAllCharacterSlots(void);
s32 SetMultiTlb(
    s32 firstTlbIndex,
    TlbPageMode pageMode,
    u32 pageCount,
    u32 virtualAddress,
    UnsignedAddress physicalAddress
);
Outcome SelectCharacterDisplayList(u32 slotIndex, s32 choiceIndex);
ClubCategory GetCharacterClubCategory(u32 slotIndex, ClubIndex clubIndex);
PointLookup GetCharacterPointModelPosition(u32 slotIndex, s32 pointId, f32 *position);
void GetCharacterSlotMatrix(CharacterSlot *slot, Mat4 matrix);
PointLookup GetCharacterPointPosition(u32 slotIndex, s32 pointId, f32 *position);
void GetCharacterShadowPoint(u32 slotIndex, f32 *position);

// Defined in character.c
s32 GetBallDistance(u32 slotIndex, s32 club, Boolean isClubCategory, ShotMode shotMode);
void LoadCharacter(u32 slotIndex, CharacterIndex characterIndex, s32 modelKind);
void SetCharacterAnimation(u32 slotIndex, AnimationId animationId);
void SetAnimationLooping(u32 slotIndex, Boolean isLooping);
void SetNextAnimation(u32 slotIndex, AnimationId nextAnimationId);
s32 GetCurrentAnimationEndFrame(u32 slotIndex);
s32 GetAnimationEndFrame(u32 slotIndex, AnimationId animationId);
s32 GetAnimationFrame(u32 slotIndex);
void SkipToAnimationEnd(u32 slotIndex);
void SetAnimationSpeed(u32 slotIndex, f32 speed);
void AnimateCharacter(u32 slotIndex);
void AnimateCharacterInSameCopy(u32 slotIndex);
void SetCharacterLights(void);
void SetCharacterClub(u32 slotIndex, ClubIndex clubIndex);
s32 GetAnimationEventFrame(u32 slotIndex, s32 animationIndex, AnimationEventKind kind);
s32 GetAnimationIndex(u32 slotIndex, AnimationId animationId);
void AdvanceAnimation(u32 slotIndex, s32 frameCount);
void SetCharacterLighting(u32 slotIndex, CharacterLighting lighting);
void AnimateAllCharacters(void);
void PlaceAndViewCharacter(u32 slotIndex);
void PlaceCharacter(u32 slotIndex, CoursePoint point, Radians heading);
void DrawCharacter(u32 slotIndex);
void DrawCharacterAndShadow(CameraMatrices *matrices, s32 cameraIndex, s32 slotIndex);
void SetCharacterHighlight(CameraMatrices *matrices UNUSED);

#endif
