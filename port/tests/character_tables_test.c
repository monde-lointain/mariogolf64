/*
 * Tests of the unpacking of a character model's tables into the host's
 * layout, on small models built here in the N64's layout: a nominal one, the
 * least one, and malformed ones, which the double of PcFatalError catches.
 */

#include <SDL3/SDL.h>
#include <setjmp.h>
#include <ultra64.h>

#include "characters/character_model.h"
#include "characters/character_tables.h"
#include "port/os_host.h"
#include "test_support.h"

#define IMAGE_SIZE 0x1000
#define Mapped(offset) (CHARACTER_MODEL_ADDRESS + (offset))

// Where the model's tables lie in it
#define CHOICES 0x000
#define ANIMATIONS 0x040
#define EVENTS 0x080
#define POINTS 0x0C0
#define JOINT_PATH 0x0F0
#define JOINTS 0x100
#define TRANSLATION_FRAMES 0x200
#define ROTATION_FRAMES 0x210
#define TRANSLATIONS 0x220
#define ROTATIONS 0x240
#define TEXTURE_ANIMATIONS 0x300
#define TRIANGLE_POINTERS 0x340
#define TRIANGLE 0x380
#define KEY_SETS 0x3C0
#define KEYS 0x3E0
#define TEXTURES 0x400
#define PORTRAIT 0x440
// The size of an AnimationEvent on the N64
#define N64_EVENT_SIZE_FOR_TEST 4

static _Alignas(SMALLEST_PAGE_SIZE) u8 f_Images[3][IMAGE_SIZE];
static CharacterModel f_Model;
static CharacterModel f_OtherModel;

static jmp_buf f_FatalErrorJump;
static int f_FatalErrorCount;

// The double of the port's fatal error, which returns to the test
void PcFatalError(const char *format, ...) {
    (void)format;
    f_FatalErrorCount++;
    longjmp(f_FatalErrorJump, 1);
}

// Unpacks the model in the image into slot 2, and tells whether that failed
// with a fatal error
static bool FailsToUnpack(const CharacterModel *model, const u8 *image) {
    f_FatalErrorCount = 0;
    if (setjmp(f_FatalErrorJump) == 0) {
        UnpackCharacterTables(2, model, image, IMAGE_SIZE);
    }

    ForgetCharacterTables(2);
    return f_FatalErrorCount != 0;
}

static void Put16(u8 *image, u32 offset, s32 value) {
    image[offset] = (u8)((u32)value >> 8);
    image[offset + 1] = (u8)value;
}

static void Put32(u8 *image, u32 offset, u32 value) {
    Put16(image, offset, (s32)(value >> 16));
    Put16(image, offset + 2, (s32)(value & 0xFFFF));
}

static void PutFloat(u8 *image, u32 offset, f32 value) {
    u32 bits;

    SDL_memcpy(&bits, &value, sizeof(bits));
    Put32(image, offset, bits);
}

static void PutKeyframeSpans(u8 *image, u32 offset) {
    s32 i;

    for (i = 0; i < 9; i++) {
        Put16(image, offset + (i * 2), UNSET_KEYFRAME_SPAN);
    }
}

// A model with an animation of one event, a point, a joint keyed in
// translation and rotation, and a triangle with two keyed textures
static void BuildImage(u8 *image) {
    s32 i;

    SDL_memset(image, 0, IMAGE_SIZE);
    Put32(image, CHOICES, Mapped(0x800));
    Put32(image, CHOICES + 4, Mapped(0x900));
    Put32(image, CHOICES + 8, END_OF_DISPLAY_LIST_CHOICES);

    Put32(image, ANIMATIONS, Mapped(EVENTS));
    Put16(image, ANIMATIONS + 4, 10);
    Put16(image, ANIMATIONS + 6, 20);
    image[ANIMATIONS + 8] = 7;
    Put16(image, EVENTS, 12);
    image[EVENTS + 2] = ANIMATION_EVENT_IMPACT;
    image[EVENTS + 3] = 1;
    Put16(image, EVENTS + 4, END_OF_ANIMATION_EVENTS);

    Put32(image, POINTS, Mapped(JOINT_PATH));
    PutFloat(image, POINTS + 4, 1.5f);
    PutFloat(image, POINTS + 8, -2.0f);
    PutFloat(image, POINTS + 12, 3.0f);
    Put16(image, POINTS + 16, CHARACTER_POINT_SHADOW);
    Put16(image, JOINT_PATH, 0);
    Put16(image, JOINT_PATH + 2, 3);
    Put16(image, JOINT_PATH + 4, END_OF_JOINT_PATH);

    Put16(image, JOINTS, 2);
    Put16(image, JOINTS + 2, 1);
    Put16(image, JOINTS + 4, 0);
    Put32(image, JOINTS + 8, Mapped(TRANSLATION_FRAMES));
    Put32(image, JOINTS + 12, Mapped(ROTATION_FRAMES));
    Put32(image, JOINTS + 20, Mapped(TRANSLATIONS));
    Put32(image, JOINTS + 24, Mapped(ROTATIONS));
    PutKeyframeSpans(image, JOINTS + 32);
    Put16(image, JOINTS + 0x34, END_OF_JOINT_ANIMATIONS);
    Put16(image, TRANSLATION_FRAMES, 0);
    Put16(image, TRANSLATION_FRAMES + 2, 30);
    Put16(image, ROTATION_FRAMES, 5);
    for (i = 0; i < 6; i++) {
        PutFloat(image, TRANSLATIONS + (i * 4), (f32)(i + 1));
    }
    Put16(image, ROTATIONS + 6, 16384);

    Put16(image, TEXTURE_ANIMATIONS, 0);
    Put16(image, TEXTURE_ANIMATIONS + 2, 40);
    Put16(image, TEXTURE_ANIMATIONS + 4, -1);

    Put32(image, TRIANGLE_POINTERS, Mapped(TRIANGLE));
    Put32(image, TRIANGLE, 0x03000010);
    Put32(image, TRIANGLE + 4, 0x03000040);
    Put32(image, TRIANGLE + 8, Mapped(KEY_SETS));
    Put16(image, TRIANGLE + 12, 1);
    Put16(image, TRIANGLE + 14, -1);
    for (i = 0; i < 6; i++) {
        Put16(image, TRIANGLE + 16 + (i * 2), i + 1);
    }
    Put16(image, KEY_SETS, 2);
    Put16(image, KEY_SETS + 2, 0);
    Put32(image, KEY_SETS + 4, Mapped(KEYS));
    Put16(image, KEYS, 5);
    Put16(image, KEYS + 2, 1);
    for (i = 0; i < 6; i++) {
        Put16(image, KEYS + 4 + (i * 2), i + 7);
    }
    // A key before the table's first texture, which the N64 read from the
    // entry before it
    Put16(image, KEYS + 0x10, 9);
    Put16(image, KEYS + 0x12, -1);
    Put32(image, TEXTURES - 8, Mapped(0xC00));

    Put32(image, TEXTURES, Mapped(0xA00));
    image[TEXTURES + 4] = 32;
    image[TEXTURES + 5] = 32;
    Put32(image, TEXTURES + 8, Mapped(0xB00));
    image[TEXTURES + 12] = 16;
    image[TEXTURES + 13] = 8;

    for (i = 0; i < PORTRAIT_CAMERA_VALUE_COUNT; i++) {
        PutFloat(image, PORTRAIT + (i * 4), 0.5f * (f32)i);
    }
}

static void BuildModel(CharacterModel *model) {
    s32 i;

    SDL_memset(model, 0, sizeof(*model));
    model->segmentAddresses[CHARACTER_MODEL_DISPLAY_LIST_CHOICES] = Mapped(CHOICES);
    model->segmentAddresses[CHARACTER_MODEL_ANIMATIONS] = Mapped(ANIMATIONS);
    model->segmentAddresses[CHARACTER_MODEL_POINTS] = Mapped(POINTS);
    model->segmentAddresses[CHARACTER_MODEL_JOINTS] = Mapped(JOINTS);
    model->segmentAddresses[CHARACTER_MODEL_TEXTURE_ANIMATIONS] = Mapped(TEXTURE_ANIMATIONS);
    model->segmentAddresses[CHARACTER_MODEL_ANIMATED_TRIANGLES] = Mapped(TRIANGLE_POINTERS);
    model->segmentAddresses[CHARACTER_MODEL_TEXTURES] = Mapped(TEXTURES);
    for (i = 0; i < PORTRAIT_CAMERA_VALUE_COUNT; i++) {
        model->segmentAddresses[CHARACTER_MODEL_PORTRAIT_CAMERA + i] = Mapped(PORTRAIT + (i * 4));
    }

    // One value outside the model, which reads as zero
    model->segmentAddresses[CHARACTER_MODEL_PORTRAIT_CAMERA + 5] = 0;
}

static void *FindTable(s32 tableIndex) {
    return FindCharacterTable(&f_Model, tableIndex);
}

static void TestUnpacksAnimations(void) {
    CharacterAnimation *animations = FindTable(CHARACTER_MODEL_ANIMATIONS);
    u32 *choices = FindTable(CHARACTER_MODEL_DISPLAY_LIST_CHOICES);

    EXPECT(animations[0].startFrame == 10 && animations[0].frameCount == 20);
    EXPECT(animations[0].id == 7);
    EXPECT(animations[0].events[0].frame == 12);
    EXPECT(animations[0].events[0].kind == ANIMATION_EVENT_IMPACT);
    EXPECT(animations[0].events[0].value == 1);
    EXPECT(animations[0].events[1].frame == END_OF_ANIMATION_EVENTS);
    EXPECT(animations[1].events == NULL);
    EXPECT(choices[0] == Mapped(0x800) && choices[1] == Mapped(0x900));
    EXPECT(choices[2] == END_OF_DISPLAY_LIST_CHOICES);
}

static void TestUnpacksJoints(void) {
    JointAnimation *joints = FindTable(CHARACTER_MODEL_JOINTS);

    EXPECT(joints[0].translationKeyCount == 2 && joints[0].rotationKeyCount == 1);
    EXPECT(joints[0].translationFrames[1] == 30);
    EXPECT(joints[0].rotationFrames[0] == 5);
    EXPECT(joints[0].translations[1][AXIS_X] == 4.0f);
    EXPECT(joints[0].translations[1][AXIS_Z] == 6.0f);
    EXPECT(joints[0].rotations[0][QUATERNION_W] == 16384);
    EXPECT(joints[0].rotations[0][AXIS_X] == 0);
    // A joint without scale keys still has a first key to read
    EXPECT(joints[0].scaleFrames != NULL && joints[0].scales != NULL);
    EXPECT(joints[0].rotationSpan.startFrame == UNSET_KEYFRAME_SPAN);
    EXPECT(joints[1].translationKeyCount == END_OF_JOINT_ANIMATIONS);
}

static void TestUnpacksTriangles(void) {
    AnimatedTriangle **triangles = FindTable(CHARACTER_MODEL_ANIMATED_TRIANGLES);
    AnimatedTexture *textures = FindTable(CHARACTER_MODEL_TEXTURES);
    TextureAnimation *animations = FindTable(CHARACTER_MODEL_TEXTURE_ANIMATIONS);
    AnimatedTriangle *triangle = triangles[0];
    TriangleTextureKey *key;

    EXPECT(triangles[1] == NULL);
    EXPECT(triangle->vertices == 0x03000010 && triangle->loadTextureBlock == 0x03000040);
    EXPECT(triangle->textureIndex == -1 && triangle->keySetCount == 1);
    EXPECT(triangle->s[2] == 3 && triangle->t[0] == 4);
    key = &triangle->keySets[0].keys[0];
    EXPECT(triangle->keySets[0].count == 2);
    EXPECT(triangle->keySets[0].keys[1].textureIndex == -1);
    EXPECT(textures[-1].address == Mapped(0xC00));
    EXPECT(key->frame == 5 && key->textureIndex == 1);
    EXPECT(key->s[0] == 7 && key->t[2] == 12);
    EXPECT(textures[1].address == Mapped(0xB00));
    EXPECT(textures[1].width == 16 && textures[1].height == 8);
    EXPECT(textures[0].width == 32);
    EXPECT(animations[0].frameCount == 40 && animations[1].index < 0);
}

static void TestUnpacksPoints(void) {
    CharacterPoint *points = FindTable(CHARACTER_MODEL_POINTS);
    f32 *last = FindTable(CHARACTER_MODEL_PORTRAIT_CAMERA + 5);

    EXPECT(points[0].id == CHARACTER_POINT_SHADOW);
    EXPECT(points[0].offset[AXIS_X] == 1.5f && points[0].offset[AXIS_Y] == -2.0f);
    EXPECT(points[0].jointPath[1] == 3 && points[0].jointPath[2] == END_OF_JOINT_PATH);
    EXPECT(points[1].jointPath == NULL);
    EXPECT(*(f32 *)FindTable(CHARACTER_MODEL_PORTRAIT_CAMERA + 2) == 1.0f);
    EXPECT(*last == 0.0f);
}

// Two slots may hold the same model in their own buffers: the one that the
// TLB maps is the one whose tables the game reads
static void TestFindsMappedSlot(void) {
    JointAnimation *joints;

    Put16(f_Images[1], TRANSLATION_FRAMES + 2, 31);
    UnpackCharacterTables(1, &f_Model, f_Images[1], IMAGE_SIZE);
    osMapTLB(
        0,
        OS_PM_4K,
        (void *)(uintptr_t)CHARACTER_MODEL_ADDRESS,
        osVirtualToPhysical(f_Images[1]),
        -1,
        -1
    );
    joints = FindTable(CHARACTER_MODEL_JOINTS);
    EXPECT(joints[0].translationFrames[1] == 31);
    osMapTLB(
        0,
        OS_PM_4K,
        (void *)(uintptr_t)CHARACTER_MODEL_ADDRESS,
        osVirtualToPhysical(f_Images[0]),
        -1,
        -1
    );
    joints = FindTable(CHARACTER_MODEL_JOINTS);
    EXPECT(joints[0].translationFrames[1] == 30);
    EXPECT(FindCharacterTable(&f_OtherModel, CHARACTER_MODEL_JOINTS) == NULL);

    // A forgotten slot's tables are found no more
    ForgetCharacterTables(0);
    joints = FindTable(CHARACTER_MODEL_JOINTS);
    EXPECT(joints[0].translationFrames[1] == 31);
    ForgetCharacterTables(1);
    EXPECT(FindTable(CHARACTER_MODEL_JOINTS) == NULL);
    osUnmapTLBAll();
}

// The least model: every table empty, and a texture table that no triangle
// uses, whose address is anything
static void TestUnpacksLeastModel(void) {
    u8 *image = f_Images[2];
    CharacterModel model;
    s32 i;

    SDL_memset(image, 0, IMAGE_SIZE);
    Put32(image, CHOICES, END_OF_DISPLAY_LIST_CHOICES);
    Put16(image, JOINTS, END_OF_JOINT_ANIMATIONS);
    Put16(image, TEXTURE_ANIMATIONS, -1);
    BuildModel(&model);
    model.segmentAddresses[CHARACTER_MODEL_TEXTURES] = 0;
    for (i = 0; i < PORTRAIT_CAMERA_VALUE_COUNT; i++) {
        model.segmentAddresses[CHARACTER_MODEL_PORTRAIT_CAMERA + i] = 0;
    }

    EXPECT(!FailsToUnpack(&model, image));
    UnpackCharacterTables(2, &model, image, IMAGE_SIZE);
    EXPECT(
        ((u32 *)FindCharacterTable(&model, CHARACTER_MODEL_DISPLAY_LIST_CHOICES))[0] ==
        END_OF_DISPLAY_LIST_CHOICES
    );
    EXPECT(
        ((CharacterAnimation *)FindCharacterTable(&model, CHARACTER_MODEL_ANIMATIONS))[0].events ==
        NULL
    );
    EXPECT(
        ((CharacterPoint *)FindCharacterTable(&model, CHARACTER_MODEL_POINTS))[0].jointPath == NULL
    );
    EXPECT(
        ((JointAnimation *)FindCharacterTable(&model, CHARACTER_MODEL_JOINTS))[0]
            .translationKeyCount == END_OF_JOINT_ANIMATIONS
    );
    EXPECT(
        ((AnimatedTriangle **)FindCharacterTable(&model, CHARACTER_MODEL_ANIMATED_TRIANGLES))[0] ==
        NULL
    );
    EXPECT(FindCharacterTable(&model, CHARACTER_MODEL_TEXTURES) != NULL);
    ForgetCharacterTables(2);
}

// A table may end at the image's last byte, but not one past it
static void TestTablesEndWithinImage(void) {
    u8 *image = f_Images[2];
    u32 lastEvent = IMAGE_SIZE - N64_EVENT_SIZE_FOR_TEST;

    BuildImage(image);
    Put32(image, ANIMATIONS, Mapped(lastEvent - N64_EVENT_SIZE_FOR_TEST));
    Put16(image, lastEvent - N64_EVENT_SIZE_FOR_TEST, 12);
    Put16(image, lastEvent, END_OF_ANIMATION_EVENTS);
    EXPECT(!FailsToUnpack(&f_Model, image));

    Put32(image, ANIMATIONS, Mapped(lastEvent));
    Put16(image, lastEvent, 12);
    EXPECT(FailsToUnpack(&f_Model, image));
}

// Malformed models: pointers below and past the mapping, and a negative key
// count
static void TestRejectsMalformedModels(void) {
    u8 *image = f_Images[2];

    BuildImage(image);
    Put32(image, JOINTS + 8, 0x01FFFFFE);
    EXPECT(FailsToUnpack(&f_Model, image));

    BuildImage(image);
    Put32(image, POINTS, Mapped(IMAGE_SIZE));
    EXPECT(FailsToUnpack(&f_Model, image));

    BuildImage(image);
    Put16(image, JOINTS + 2, -2);
    EXPECT(FailsToUnpack(&f_Model, image));

    BuildImage(image);
    EXPECT(!FailsToUnpack(&f_Model, image));
}

// Loading another model into a slot replaces its tables: the earlier
// model's are found no more, and the arena's reuse leaves the new ones whole
static void TestReloadReplacesTables(void) {
    JointAnimation *joints;

    BuildImage(f_Images[2]);
    UnpackCharacterTables(2, &f_Model, f_Images[2], IMAGE_SIZE);
    Put16(f_Images[2], TRANSLATION_FRAMES + 2, 44);
    UnpackCharacterTables(2, &f_OtherModel, f_Images[2], IMAGE_SIZE);
    EXPECT(FindTable(CHARACTER_MODEL_JOINTS) == NULL);
    joints = FindCharacterTable(&f_OtherModel, CHARACTER_MODEL_JOINTS);
    EXPECT(joints[0].translationFrames[1] == 44 && joints[0].rotationFrames[0] == 5);
    EXPECT(joints[1].translationKeyCount == END_OF_JOINT_ANIMATIONS);
    ForgetCharacterTables(2);
}

int main(void) {
    BuildImage(f_Images[0]);
    BuildImage(f_Images[1]);
    BuildModel(&f_Model);
    BuildModel(&f_OtherModel);
    UnpackCharacterTables(0, &f_Model, f_Images[0], IMAGE_SIZE);
    TestUnpacksAnimations();
    TestUnpacksJoints();
    TestUnpacksTriangles();
    TestUnpacksPoints();
    TestFindsMappedSlot();
    TestUnpacksLeastModel();
    TestTablesEndWithinImage();
    TestRejectsMalformedModels();
    TestReloadReplacesTables();
    return ReportTestResults("character_tables_test");
}
