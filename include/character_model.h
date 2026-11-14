#ifndef CHARACTER_MODEL_H
#define CHARACTER_MODEL_H

#include <ultra64.h>

#include "camera.h"
#include "character.h"
#include "club.h"
#include "course_space.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "screen/display_list.h"
#include "system/c_language.h"
#include "system/compiler.h"

#define CHARACTER_MODEL_KIND_COUNT 4
#define CHARACTER_MODELS_PER_KIND 36
#define ModelCharacter(modelIndex) ((modelIndex) % CHARACTER_MODELS_PER_KIND)
#define OPENING_CHARACTER_MODEL_COUNT 18
#define CHARACTER_MODEL_SEGMENT_ADDRESS_COUNT 15

#define CHARACTER_MODEL_DISPLAY_LIST 0
#define CHARACTER_MODEL_CHOSEN_DISPLAY_LIST 1
#define CHARACTER_MODEL_DISPLAY_LIST_CHOICES 2
#define CHARACTER_MODEL_POINTS 4
#define CHARACTER_MODEL_ANIMATIONS 3
#define CHARACTER_MODEL_JOINTS 5
#define CHARACTER_MODEL_TEXTURE_ANIMATIONS 6
#define CHARACTER_MODEL_ANIMATED_TRIANGLES 7
#define CHARACTER_MODEL_TEXTURES 8
#define CHARACTER_MODEL_PORTRAIT_CAMERA 9
#define PORTRAIT_CAMERA_VALUE_COUNT 6

#define END_OF_DISPLAY_LIST_CHOICES 0xFFFFFFFF
#define NO_DISPLAY_LIST_CHOICE (-1)

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

#define LIGHT_DIRECTION_SCALE 127.0f

typedef struct CharacterModel {
    u32 romStart;
    u32 romEnd;
    u32 segmentAddresses[CHARACTER_MODEL_SEGMENT_ADDRESS_COUNT];
    s16 shadowScale;
    u32 baseRomStart;
    u32 baseRomEnd;
    u32 frameDataStart;
    u32 frameDataEnd;
} CharacterModel;

#define CHARACTER_SLOT_COUNT 4
#define NO_CHARACTER_MODEL (-1)

typedef struct CharacterPoint {
    s16 *jointPath;
    Vec3 offset;
    s16 id;
} CharacterPoint;

#define END_OF_JOINT_PATH (-1)

#define CHARACTER_POINT_SHADOW 2

enum PointLookup {
    POINT_LOOKUP_NO_SLOT = -1,
    POINT_LOOKUP_FOUND,
    POINT_LOOKUP_NO_POINT
};

typedef s32 PointLookup;

#define ROLL_ORIGIN_DEGREES 180

typedef struct KeyframeSpan {
    s16 startFrame;
    s16 endFrame;
    s16 startKeyIndex;
} KeyframeSpan;

#define QUATERNION_W 3
#define QUATERNION_COMPONENT_COUNT 4

typedef f32 Quaternion[QUATERNION_COMPONENT_COUNT];

typedef struct JointAnimation {
    s16 translationKeyCount;
    s16 rotationKeyCount;
    s16 scaleKeyCount;
    s16 *translationFrames;
    s16 *rotationFrames;
    s16 *scaleFrames;
    Vec3 *translations;
    s16 (*rotations)[QUATERNION_COMPONENT_COUNT];
    Vec3 *scales;
    KeyframeSpan translationSpan;
    KeyframeSpan rotationSpan;
    KeyframeSpan scaleSpan;
} JointAnimation;

#define UNSET_KEYFRAME_SPAN (-1)

#define END_OF_JOINT_ANIMATIONS (-1)

#define QUATERNION_SCALE (1.0f / 16384)
#define QUATERNION_PRODUCT_SCALE (1.0f / (16384 * 16384))
#define SLERP_THRESHOLD 1e-06

static inline void QuaternionToMatrix(Quaternion quaternion, Mat4 matrix) {
    f32 lengthSquared = (quaternion[AXIS_X] * quaternion[AXIS_X]) +
        (quaternion[AXIS_Y] * quaternion[AXIS_Y]) + (quaternion[AXIS_Z] * quaternion[AXIS_Z]) +
        (quaternion[QUATERNION_W] * quaternion[QUATERNION_W]);
    f32 scale = (lengthSquared > 0.0f) ? (2.0f / lengthSquared) : 0.0f;
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

#define END_OF_ANIMATION_EVENTS (-1)

enum AnimationEventKind {
    ANIMATION_EVENT_LOOP_START,
    ANIMATION_EVENT_LOOP_END,
    ANIMATION_EVENT_IMPACT = 6,
    ANIMATION_EVENT_TEXTURE_ANIMATION = 0xFE
};

typedef s32 AnimationEventKind;
typedef u8 AnimationEventKind8;

typedef struct AnimationEvent {
    s16 frame;
    AnimationEventKind8 kind;
    u8 value;
} AnimationEvent;

typedef struct AnimationEventAction {
    AnimationEventKind8 kind;
    u8 value;
} AnimationEventAction;

#define EVENT_ACTION_COUNT_MAX 16

enum TextureAnimationEvent {
    TEXTURE_ANIMATION_EVENT_START_RANDOM,
    TEXTURE_ANIMATION_EVENT_STOP_RANDOM = 2
};

typedef struct CharacterAnimation {
    AnimationEvent *events;
    s16 startFrame;
    s16 frameCount;
    UAnimationId8 id;
} CharacterAnimation;

typedef struct TextureAnimation {
    s16 index;
    s16 frameCount;
} TextureAnimation;

#define RANDOM_TEXTURE_ANIMATION 0

typedef struct AnimatedTexture {
    u32 address;
    u8 width;
    u8 height;
} AnimatedTexture;

typedef struct TriangleTextureKey {
    s16 frame;
    s16 textureIndex;
    s16 s[TRIANGLE_VERTEX_COUNT];
    s16 t[TRIANGLE_VERTEX_COUNT];
} TriangleTextureKey;

typedef struct TriangleTextureKeys {
    s16 count;
    s16 textureAnimationIndex;
    TriangleTextureKey *keys;
} TriangleTextureKeys;

typedef struct AnimatedTriangle {
    u32 vertices;
    u32 loadTextureBlock;
    TriangleTextureKeys *keySets;
    s16 keySetCount;
    s16 textureIndex;
    s16 s[TRIANGLE_VERTEX_COUNT];
    s16 t[TRIANGLE_VERTEX_COUNT];
} AnimatedTriangle;

#define TEXTURE_ANIMATION_COUNT 32

typedef struct TextureAnimationState {
    s32 frame;
    s8 pendingCopyCount;
} TextureAnimationState;

#define FRAME_DATA_COPY_COUNT 3
#define TEXTURE_ANIMATION_PLAYING (FRAME_DATA_COPY_COUNT + 1)

enum CharacterLighting {
    CHARACTER_LIGHTING_FIXED,
    CHARACTER_LIGHTING_SCENE,
    CHARACTER_LIGHTING_SCENE_THIRD,
    CHARACTER_LIGHTING_BLACK
};

typedef s32 CharacterLighting;
typedef s8 CharacterLighting8;

typedef struct CharacterSlot {
    const CharacterModel *model;
    void *modelBuffer;
    s32 modelIndex;
    s32 animationIndex;
    f32 animationFrame;
    s32 animationStartFrame;
    s32 animationEndFrame;
    s32 animationCount;
    void *frameData[FRAME_DATA_COPY_COUNT];
    Mtx *jointMatrices;
    s32 frameDataIndex;
    u32 lastDrawnFrame;
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;
    f32 animationSpeed;
    Boolean8 isTranslucent;
    u8 alpha;
    Gfx *displayList;
    f32 portraitCamera[PORTRAIT_CAMERA_VALUE_COUNT];
    s16 shadowScale;
    CharacterLighting8 lighting;
    Boolean8 isLooping;
    AnimationId8 nextAnimationId;
    Boolean8 playsRandomTextureAnimation;
    s8 randomTextureAnimationDelay;
    TextureAnimationState textureAnimations[TEXTURE_ANIMATION_COUNT];
    Boolean8 usesFrontCulling;
    Boolean8 reusesLastMatrices;
} CharacterSlot;

#define NO_NEXT_ANIMATION (-1)

#define WOOD_ANIMATIONS 0
#define IRON_ANIMATIONS 3
#define WEDGE_ANIMATIONS 6
#define PUTTER_ANIMATIONS 9
#define SWING_ANIMATION 1

extern CharacterSlot g_CharacterSlots[CHARACTER_SLOT_COUNT];

extern Gfx g_CharacterRenderModeDisplayList[3];
extern Gfx g_CharacterCombineDisplayList[2];
extern Gfx g_CharacterHighlightDisplayList[3];
#define ENV_COLOR_COUNT 10
extern u8 g_CharacterEnvColors[ENV_COLOR_COUNT][RGB_CHANNEL_COUNT];
extern Gfx g_CharacterEnvColorDisplayLists[ENV_COLOR_COUNT][2];

extern f32 g_CurrentAnimationFrame;
extern s32 g_CurrentAnimationEndFrame;
extern Mtx *g_CurrentJointMatrices;

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
    u32 physicalAddress
);
Outcome SelectCharacterDisplayList(u32 slotIndex, s32 choiceIndex);
ClubCategory GetCharacterClubCategory(u32 slotIndex, ClubIndex clubIndex);
PointLookup GetCharacterPointModelPosition(u32 slotIndex, s32 pointId, f32 *position);
void GetCharacterSlotMatrix(CharacterSlot *slot, Mat4 matrix);
PointLookup GetCharacterPointPosition(u32 slotIndex, s32 pointId, f32 *position);
void GetCharacterShadowPoint(u32 slotIndex, f32 *position);

s32 GetBallDistance(u32 slotIndex, s32 club, Boolean isClubCategory, ShotMode shotMode);
void LoadCharacter(u32 slotIndex, s32 characterIndex, s32 modelKind);
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
