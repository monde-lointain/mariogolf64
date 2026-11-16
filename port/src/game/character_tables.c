/*
 * Unpacks the tables of a character model's file into the host's layout.
 *
 * The file's tables are the N64's structures: 32-bit pointers into the
 * model's mapping at CHARACTER_MODEL_ADDRESS, and big-endian values. Each
 * table is copied into the slot's arena, its values converted and its
 * pointers replaced by pointers to the copies of what they point at. The
 * arena is reused when the slot loads another model.
 */

#include "characters/character_tables.h"

#include <SDL3/SDL.h>
#include <ultra64.h>

#include "characters/character_model.h"
#include "port/os_host.h"

// The bytes that CHARACTER_MODEL_ADDRESS maps, which hold every table
#define MODEL_MAPPING_SIZE (CHARACTER_MODEL_PAGE_COUNT * SMALLEST_PAGE_SIZE)
// Enough for the copies of every table that the mapping can hold, whose
// structures at most double in size with the host's pointers
#define ARENA_SIZE (MODEL_MAPPING_SIZE * 2)
#define ARENA_ALIGNMENT 8

// The N64's sizes and offsets of the tables' structures
#define N64_POINTER_SIZE 4
#define N64_JOINT_SIZE 0x34
#define N64_JOINT_POINTERS 0x08
#define N64_ANIMATION_SIZE 0x0C
#define N64_ANIMATION_START_FRAME 0x04
#define N64_ANIMATION_FRAME_COUNT 0x06
#define N64_ANIMATION_ID 0x08
#define N64_EVENT_SIZE 0x04
#define N64_TEXTURE_ANIMATION_SIZE 0x04
#define N64_TRIANGLE_SIZE 0x1C
#define N64_TRIANGLE_KEY_SETS 0x08
#define N64_TRIANGLE_KEY_SET_COUNT 0x0C
#define N64_TRIANGLE_TEXTURE_INDEX 0x0E
#define N64_TRIANGLE_COORDINATES 0x10
#define N64_KEY_SET_SIZE 0x08
#define N64_KEY_SET_KEYS 0x04
#define N64_KEY_SIZE 0x10
#define N64_KEY_COORDINATES 0x04
#define N64_TEXTURE_SIZE 0x08
#define N64_POINT_SIZE 0x14
#define N64_POINT_OFFSET 0x04
#define N64_POINT_ID 0x10

#define END_OF_TEXTURE_ANIMATIONS 0 // Any negative index ends them

// The model's file as the N64 mapped it
typedef struct ModelImage {
    const u8 *bytes;
    u32 size;
} ModelImage;

// A slot's copies of its model's tables
typedef struct SlotTables {
    const CharacterModel *model; // NULL while the slot holds none
    const void *buffer;
    void *tables[CHARACTER_MODEL_SEGMENT_ADDRESS_COUNT];
    u32 arenaUsed;
    _Alignas(ARENA_ALIGNMENT) u8 arena[ARENA_SIZE];
} SlotTables;

static SlotTables f_Slots[CHARACTER_SLOT_COUNT];

static s16 ReadS16(const u8 *bytes) {
    return (s16)(((u32)bytes[0] << 8) | bytes[1]);
}

static u32 ReadU32(const u8 *bytes) {
    return ((u32)bytes[0] << 24) | ((u32)bytes[1] << 16) | ((u32)bytes[2] << 8) | bytes[3];
}

static f32 ReadF32(const u8 *bytes) {
    u32 bits = ReadU32(bytes);
    f32 value;

    SDL_memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool HoldsImageBytes(const ModelImage *image, u32 address, u32 size) {
    u32 offset = address - CHARACTER_MODEL_ADDRESS;

    return address >= CHARACTER_MODEL_ADDRESS &&
        offset <= image->size &&
        size <= image->size - offset;
}

// The bytes of the image at a mapped address, which must hold size bytes
static const u8 *FindImageBytes(const ModelImage *image, u32 address, u32 size) {
    if (!HoldsImageBytes(image, address, size)) {
        PcFatalError("A character model's table points outside it, at %#x", (unsigned)address);
    }

    return &image->bytes[address - CHARACTER_MODEL_ADDRESS];
}

static void *AllocateCopy(SlotTables *slot, u32 size) {
    u32 start = (slot->arenaUsed + ARENA_ALIGNMENT - 1) & ~(u32)(ARENA_ALIGNMENT - 1);
    void *copy;

    if (size > ARENA_SIZE - start) {
        PcFatalError("A character model's tables need more than %u bytes", (unsigned)ARENA_SIZE);
    }

    copy = &slot->arena[start];
    slot->arenaUsed = start + size;
    SDL_memset(copy, 0, size);
    return copy;
}

// An empty array, whose address may be anything, as a zeroed element, which
// the game reads as the first key even of a joint without keys
static void *CopyEmptyArray(SlotTables *slot, s32 count, u32 elementSize) {
    if (count < 0) {
        PcFatalError("A character model's table has %d entries", (int)count);
    }

    return AllocateCopy(slot, elementSize);
}

static s16 *CopyHalves(SlotTables *slot, const ModelImage *image, u32 address, s32 count) {
    const u8 *bytes;
    s16 *copy;
    s32 i;

    if (count <= 0) {
        return CopyEmptyArray(slot, count, sizeof(s16));
    }

    bytes = FindImageBytes(image, address, (u32)count * sizeof(s16));
    copy = AllocateCopy(slot, (u32)count * sizeof(s16));
    for (i = 0; i < count; i++) {
        copy[i] = ReadS16(&bytes[i * sizeof(s16)]);
    }

    return copy;
}

static f32 *CopyFloats(SlotTables *slot, const ModelImage *image, u32 address, s32 count) {
    const u8 *bytes;
    f32 *copy;
    s32 i;

    if (count <= 0) {
        return CopyEmptyArray(slot, count, sizeof(f32));
    }

    bytes = FindImageBytes(image, address, (u32)count * sizeof(f32));
    copy = AllocateCopy(slot, (u32)count * sizeof(f32));
    for (i = 0; i < count; i++) {
        copy[i] = ReadF32(&bytes[i * sizeof(f32)]);
    }

    return copy;
}

// The number of entries of the given size from address up to the one whose
// first half is the end marker, which is counted
static s32 CountUpToHalf(const ModelImage *image, u32 address, u32 entrySize, s16 endMarker) {
    s32 count = 1;

    while (ReadS16(FindImageBytes(image, address, sizeof(s16))) != endMarker) {
        address += entrySize;
        count++;
    }

    return count;
}

// Likewise up to the entry whose first word is zero
static s32 CountUpToNullWord(const ModelImage *image, u32 address, u32 entrySize) {
    s32 count = 1;

    while (ReadU32(FindImageBytes(image, address, sizeof(u32))) != 0) {
        address += entrySize;
        count++;
    }

    return count;
}

static void CopyKeyframeSpan(KeyframeSpan *span, const u8 *bytes) {
    span->startFrame = ReadS16(&bytes[0]);
    span->endFrame = ReadS16(&bytes[2]);
    span->startKeyIndex = ReadS16(&bytes[4]);
}

static JointAnimation *UnpackJoints(SlotTables *slot, const ModelImage *image, u32 address) {
    s32 count = CountUpToHalf(image, address, N64_JOINT_SIZE, END_OF_JOINT_ANIMATIONS);
    JointAnimation *joints = AllocateCopy(slot, (u32)count * sizeof(JointAnimation));
    s32 i;

    for (i = 0; i < count; i++) {
        const u8 *bytes = FindImageBytes(image, address + (i * N64_JOINT_SIZE), N64_JOINT_SIZE);
        const u8 *pointers = &bytes[N64_JOINT_POINTERS];
        JointAnimation *joint = &joints[i];

        joint->translationKeyCount = ReadS16(&bytes[0]);
        joint->rotationKeyCount = ReadS16(&bytes[2]);
        joint->scaleKeyCount = ReadS16(&bytes[4]);
        CopyKeyframeSpan(&joint->translationSpan, &pointers[6 * N64_POINTER_SIZE]);
        CopyKeyframeSpan(&joint->rotationSpan, &pointers[6 * N64_POINTER_SIZE + 6]);
        CopyKeyframeSpan(&joint->scaleSpan, &pointers[6 * N64_POINTER_SIZE + 12]);
        if (joint->translationKeyCount == END_OF_JOINT_ANIMATIONS) {
            break;
        }

        joint->translationFrames =
            CopyHalves(slot, image, ReadU32(&pointers[0]), joint->translationKeyCount);
        joint->rotationFrames =
            CopyHalves(slot, image, ReadU32(&pointers[4]), joint->rotationKeyCount);
        joint->scaleFrames = CopyHalves(slot, image, ReadU32(&pointers[8]), joint->scaleKeyCount);
        joint->translations =
            (Vec3 *)CopyFloats(slot, image, ReadU32(&pointers[12]), joint->translationKeyCount * 3);
        joint->rotations = (s16(*)[QUATERNION_COMPONENT_COUNT])CopyHalves(
            slot,
            image,
            ReadU32(&pointers[16]),
            joint->rotationKeyCount * QUATERNION_COMPONENT_COUNT
        );
        joint->scales =
            (Vec3 *)CopyFloats(slot, image, ReadU32(&pointers[20]), joint->scaleKeyCount * 3);
    }

    return joints;
}

static AnimationEvent *UnpackEvents(SlotTables *slot, const ModelImage *image, u32 address) {
    s32 count = CountUpToHalf(image, address, N64_EVENT_SIZE, END_OF_ANIMATION_EVENTS);
    AnimationEvent *events = AllocateCopy(slot, (u32)count * sizeof(AnimationEvent));
    const u8 *bytes = FindImageBytes(image, address, (u32)count * N64_EVENT_SIZE);
    s32 i;

    for (i = 0; i < count; i++) {
        events[i].frame = ReadS16(&bytes[i * N64_EVENT_SIZE]);
        events[i].kind = bytes[(i * N64_EVENT_SIZE) + 2];
        events[i].value = bytes[(i * N64_EVENT_SIZE) + 3];
    }

    return events;
}

static CharacterAnimation *UnpackAnimations(
    SlotTables *slot,
    const ModelImage *image,
    u32 address
) {
    s32 count = CountUpToNullWord(image, address, N64_ANIMATION_SIZE);
    CharacterAnimation *animations = AllocateCopy(slot, (u32)count * sizeof(CharacterAnimation));
    const u8 *bytes = FindImageBytes(image, address, (u32)count * N64_ANIMATION_SIZE);
    s32 i;

    for (i = 0; i < count; i++) {
        const u8 *entry = &bytes[i * N64_ANIMATION_SIZE];
        u32 events = ReadU32(entry);

        animations[i].events = (events != 0) ? UnpackEvents(slot, image, events) : NULL;
        animations[i].startFrame = ReadS16(&entry[N64_ANIMATION_START_FRAME]);
        animations[i].frameCount = ReadS16(&entry[N64_ANIMATION_FRAME_COUNT]);
        animations[i].id = entry[N64_ANIMATION_ID];
    }

    return animations;
}

// Up to the first negative index, which is copied, or TEXTURE_ANIMATION_COUNT
static TextureAnimation *UnpackTextureAnimations(
    SlotTables *slot,
    const ModelImage *image,
    u32 address
) {
    TextureAnimation *animations =
        AllocateCopy(slot, TEXTURE_ANIMATION_COUNT * sizeof(TextureAnimation));
    s32 i;

    for (i = 0; i < TEXTURE_ANIMATION_COUNT; i++) {
        const u8 *entry = FindImageBytes(
            image,
            address + (i * N64_TEXTURE_ANIMATION_SIZE),
            N64_TEXTURE_ANIMATION_SIZE
        );

        animations[i].index = ReadS16(&entry[0]);
        animations[i].frameCount = ReadS16(&entry[2]);
        if (animations[i].index < END_OF_TEXTURE_ANIMATIONS) {
            break;
        }
    }

    return animations;
}

static void CopyTriangleCoordinates(s16 *s, s16 *t, const u8 *bytes) {
    s32 i;

    for (i = 0; i < TRIANGLE_VERTEX_COUNT; i++) {
        s[i] = ReadS16(&bytes[i * sizeof(s16)]);
        t[i] = ReadS16(&bytes[(TRIANGLE_VERTEX_COUNT + i) * sizeof(s16)]);
    }
}

static TriangleTextureKeys *UnpackKeySets(
    SlotTables *slot,
    const ModelImage *image,
    u32 address,
    s32 count
) {
    TriangleTextureKeys *keySets;
    const u8 *bytes;
    s32 i;
    s32 k;

    if (count <= 0) {
        return CopyEmptyArray(slot, count, sizeof(TriangleTextureKeys));
    }

    keySets = AllocateCopy(slot, (u32)count * sizeof(TriangleTextureKeys));
    bytes = FindImageBytes(image, address, (u32)count * N64_KEY_SET_SIZE);
    for (i = 0; i < count; i++) {
        const u8 *entry = &bytes[i * N64_KEY_SET_SIZE];
        TriangleTextureKeys *keySet = &keySets[i];
        const u8 *keys;

        keySet->count = ReadS16(&entry[0]);
        keySet->textureAnimationIndex = ReadS16(&entry[2]);
        if (keySet->count <= 0) {
            keySet->keys = CopyEmptyArray(slot, keySet->count, sizeof(TriangleTextureKey));
            continue;
        }

        keySet->keys = AllocateCopy(slot, (u32)keySet->count * sizeof(TriangleTextureKey));
        keys = FindImageBytes(
            image,
            ReadU32(&entry[N64_KEY_SET_KEYS]),
            (u32)keySet->count * N64_KEY_SIZE
        );
        for (k = 0; k < keySet->count; k++) {
            const u8 *key = &keys[k * N64_KEY_SIZE];

            keySet->keys[k].frame = ReadS16(&key[0]);
            keySet->keys[k].textureIndex = ReadS16(&key[2]);
            CopyTriangleCoordinates(
                keySet->keys[k].s,
                keySet->keys[k].t,
                &key[N64_KEY_COORDINATES]
            );
        }
    }

    return keySets;
}

static AnimatedTriangle *UnpackTriangle(SlotTables *slot, const ModelImage *image, u32 address) {
    const u8 *bytes = FindImageBytes(image, address, N64_TRIANGLE_SIZE);
    AnimatedTriangle *triangle = AllocateCopy(slot, sizeof(AnimatedTriangle));

    triangle->vertices = ReadU32(&bytes[0]);
    triangle->loadTextureBlock = ReadU32(&bytes[4]);
    triangle->keySetCount = ReadS16(&bytes[N64_TRIANGLE_KEY_SET_COUNT]);
    triangle->textureIndex = ReadS16(&bytes[N64_TRIANGLE_TEXTURE_INDEX]);
    CopyTriangleCoordinates(triangle->s, triangle->t, &bytes[N64_TRIANGLE_COORDINATES]);
    triangle->keySets =
        UnpackKeySets(slot, image, ReadU32(&bytes[N64_TRIANGLE_KEY_SETS]), triangle->keySetCount);
    return triangle;
}

// The pointers to the triangles, ended by NULL
static AnimatedTriangle **UnpackTriangles(SlotTables *slot, const ModelImage *image, u32 address) {
    s32 count = CountUpToNullWord(image, address, N64_POINTER_SIZE);
    AnimatedTriangle **triangles = AllocateCopy(slot, (u32)count * sizeof(AnimatedTriangle *));
    s32 i;

    for (i = 0; i < count - 1; i++) {
        u32 triangle = ReadU32(FindImageBytes(image, address + (i * N64_POINTER_SIZE), 4));

        triangles[i] = UnpackTriangle(slot, image, triangle);
    }

    return triangles;
}

// The texture indices that the triangles and their keys use, first to last
static void FindTextureIndexRange(AnimatedTriangle **triangles, s32 *first, s32 *last) {
    s32 i;
    s32 k;
    s32 key;

    *first = 0;
    *last = -1;
    for (i = 0; triangles[i] != NULL; i++) {
        *last = SDL_max(*last, triangles[i]->textureIndex);
        for (k = 0; k < triangles[i]->keySetCount; k++) {
            const TriangleTextureKeys *keySet = &triangles[i]->keySets[k];

            for (key = 0; key < keySet->count; key++) {
                *first = SDL_min(*first, keySet->keys[key].textureIndex);
                *last = SDL_max(*last, keySet->keys[key].textureIndex);
            }
        }
    }
}

// The textures that the triangles use, which the table doesn't count. A
// key's negative index read the bytes before the table on the N64, so the
// copy covers those too, and the table's pointer points into it.
static AnimatedTexture *UnpackTextures(
    SlotTables *slot,
    const ModelImage *image,
    u32 address,
    AnimatedTriangle **triangles
) {
    s32 first;
    s32 last;
    s32 count;
    AnimatedTexture *textures;
    const u8 *bytes;
    s32 i;

    FindTextureIndexRange(triangles, &first, &last);
    if (last < first) {
        // A model whose triangles use no textures may point anywhere
        return CopyEmptyArray(slot, 0, sizeof(AnimatedTexture));
    }

    count = last - first + 1;
    address += first * N64_TEXTURE_SIZE;
    textures = AllocateCopy(slot, (u32)count * sizeof(AnimatedTexture));
    bytes = FindImageBytes(image, address, (u32)count * N64_TEXTURE_SIZE);
    for (i = 0; i < count; i++) {
        textures[i].address = ReadU32(&bytes[i * N64_TEXTURE_SIZE]);
        textures[i].width = bytes[(i * N64_TEXTURE_SIZE) + 4];
        textures[i].height = bytes[(i * N64_TEXTURE_SIZE) + 5];
    }

    return textures - first;
}

static CharacterPoint *UnpackPoints(SlotTables *slot, const ModelImage *image, u32 address) {
    s32 count = CountUpToNullWord(image, address, N64_POINT_SIZE);
    CharacterPoint *points = AllocateCopy(slot, (u32)count * sizeof(CharacterPoint));
    const u8 *bytes = FindImageBytes(image, address, (u32)count * N64_POINT_SIZE);
    s32 i;

    for (i = 0; i < count; i++) {
        const u8 *entry = &bytes[i * N64_POINT_SIZE];
        u32 path = ReadU32(entry);
        s32 axis;

        for (axis = 0; axis < 3; axis++) {
            points[i].offset[axis] = ReadF32(&entry[N64_POINT_OFFSET + (axis * sizeof(f32))]);
        }

        points[i].id = ReadS16(&entry[N64_POINT_ID]);
        if (path != 0) {
            points[i].jointPath =
                CopyHalves(slot, image, path, CountUpToHalf(image, path, 2, END_OF_JOINT_PATH));
        }
    }

    return points;
}

// The display list choices' addresses, ended by END_OF_DISPLAY_LIST_CHOICES
static u32 *UnpackDisplayListChoices(SlotTables *slot, const ModelImage *image, u32 address) {
    s32 count = 1;
    u32 *choices;
    s32 i;

    while (ReadU32(FindImageBytes(image, address + ((count - 1) * 4), 4)) !=
        END_OF_DISPLAY_LIST_CHOICES) {
        count++;
    }

    choices = AllocateCopy(slot, (u32)count * sizeof(u32));
    for (i = 0; i < count; i++) {
        choices[i] = ReadU32(FindImageBytes(image, address + (i * 4), 4));
    }

    return choices;
}

void UnpackCharacterTables(
    s32 slotIndex,
    const CharacterModel *model,
    const void *buffer,
    u32 bufferSize
) {
    const u32 *addresses = model->segmentAddresses;
    ModelImage image;
    SlotTables *slot;
    s32 i;

    SDL_assert_always(slotIndex >= 0 && slotIndex < CHARACTER_SLOT_COUNT);
    SDL_assert_always(model != NULL && buffer != NULL);
    slot = &f_Slots[slotIndex];
    image.bytes = buffer;
    image.size = SDL_min(bufferSize, (u32)MODEL_MAPPING_SIZE);
    slot->model = model;
    slot->buffer = buffer;
    slot->arenaUsed = 0;
    SDL_memset(slot->tables, 0, sizeof(slot->tables));

    slot->tables[CHARACTER_MODEL_DISPLAY_LIST_CHOICES] =
        UnpackDisplayListChoices(slot, &image, addresses[CHARACTER_MODEL_DISPLAY_LIST_CHOICES]);
    slot->tables[CHARACTER_MODEL_ANIMATIONS] =
        UnpackAnimations(slot, &image, addresses[CHARACTER_MODEL_ANIMATIONS]);
    slot->tables[CHARACTER_MODEL_POINTS] =
        UnpackPoints(slot, &image, addresses[CHARACTER_MODEL_POINTS]);
    slot->tables[CHARACTER_MODEL_JOINTS] =
        UnpackJoints(slot, &image, addresses[CHARACTER_MODEL_JOINTS]);
    slot->tables[CHARACTER_MODEL_TEXTURE_ANIMATIONS] =
        UnpackTextureAnimations(slot, &image, addresses[CHARACTER_MODEL_TEXTURE_ANIMATIONS]);
    slot->tables[CHARACTER_MODEL_ANIMATED_TRIANGLES] =
        UnpackTriangles(slot, &image, addresses[CHARACTER_MODEL_ANIMATED_TRIANGLES]);
    slot->tables[CHARACTER_MODEL_TEXTURES] = UnpackTextures(
        slot,
        &image,
        addresses[CHARACTER_MODEL_TEXTURES],
        slot->tables[CHARACTER_MODEL_ANIMATED_TRIANGLES]
    );
    // Nothing reads the portrait camera, which some models may not have:
    // a value outside the model reads as zero
    for (i = 0; i < PORTRAIT_CAMERA_VALUE_COUNT; i++) {
        u32 address = addresses[CHARACTER_MODEL_PORTRAIT_CAMERA + i];

        slot->tables[CHARACTER_MODEL_PORTRAIT_CAMERA + i] =
            CopyFloats(slot, &image, address, HoldsImageBytes(&image, address, sizeof(f32)));
    }
}

void *FindCharacterTable(const CharacterModel *model, s32 tableIndex) {
    const void *mappedBuffer = osPcTranslate(CHARACTER_MODEL_ADDRESS);
    s32 i;

    SDL_assert_always(tableIndex >= 0 && tableIndex < CHARACTER_MODEL_SEGMENT_ADDRESS_COUNT);
    for (i = 0; i < CHARACTER_SLOT_COUNT; i++) {
        if (f_Slots[i].model == model && f_Slots[i].buffer == mappedBuffer) {
            return f_Slots[i].tables[tableIndex];
        }
    }

    for (i = 0; i < CHARACTER_SLOT_COUNT; i++) {
        if (f_Slots[i].model == model) {
            return f_Slots[i].tables[tableIndex];
        }
    }

    return NULL;
}

void ForgetCharacterTables(s32 slotIndex) {
    SDL_assert_always(slotIndex >= 0 && slotIndex < CHARACTER_SLOT_COUNT);
    f_Slots[slotIndex].model = NULL;
    f_Slots[slotIndex].buffer = NULL;
}
