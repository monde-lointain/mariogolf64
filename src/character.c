#include "common.h"
#include "camera.h"
#include "character_slot.h"
#include "club.h"
#include "current_shot.h"
#include "debug_text.h"
#include "functions.h"
#include "game_flags.h"
#include "heap.h"
#include "hole_loader.h"
#include "host_file.h"
#include "libc.h"
#include "main.h"
#include "math_util.h"
#include "variables.h"

/* Room for a model's files and, FRAME_DATA_OFFSET in, its frame data copies */
#define CHARACTER_BUFFER_SIZE 0x23680
#define FRAME_DATA_OFFSET 0x20000
#define FRAME_DATA_COPY_SIZE 0x780
/* The buffer each slot is given before AssignCharacterBuffers gives it its own */
#define DEFAULT_CHARACTER_BUFFER 0x8030F800
/* SetCharacterBuffer's warning is printed this many times, so it stands out */
#define WARNING_REPEAT_COUNT 10
/* Long enough ago that IsCharacterSlotBusy finds a slot not busy */
#define LONG_AGO_FRAME -4

#define ANIMATIONS(model) ((CharacterAnimation *)(model)->segmentAddresses[CHARACTER_MODEL_ANIMATIONS])
#define JOINT_ANIMATIONS(model) ((JointAnimation *)(model)->segmentAddresses[CHARACTER_MODEL_JOINTS])
#define ANIMATED_TRIANGLES(model) ((AnimatedTriangle **)(model)->segmentAddresses[CHARACTER_MODEL_ANIMATED_TRIANGLES])
#define TEXTURE_ANIMATIONS(model) ((TextureAnimation *)(model)->segmentAddresses[CHARACTER_MODEL_TEXTURE_ANIMATIONS])
#define ANIMATED_TEXTURES(model) ((AnimatedTexture *)(model)->segmentAddresses[CHARACTER_MODEL_TEXTURES])

/* Where a segment address into the frame data points in the slot's copy being written */
#define FRAME_DATA_POINTER(slot, address) \
    ((u8 *)(slot)->frameData[(slot)->frameDataIndex] + SEGMENT_OFFSET(address))

/*
 * Patches the slot's frame data copy being written so the triangle shows the
 * texture with the texture coordinates. Uses the locals gfx, vertex and i.
 */
#define SET_TRIANGLE_TEXTURE(slot, triangle, texture, sCoordinates, tCoordinates)                              \
    gfx = (Gfx *)FRAME_DATA_POINTER(slot, (triangle)->loadTextureBlock + LOAD_TEXTURE_BLOCK_OFFSET);          \
    gDPLoadTextureBlock(gfx++, (texture)->address & ~(TEXTURE_ALIGNMENT - 1), G_IM_FMT_RGBA, G_IM_SIZ_16b,     \
                        (texture)->width, (texture)->height, 0, G_TX_NOMIRROR | G_TX_CLAMP,                    \
                        G_TX_NOMIRROR | G_TX_CLAMP, ANIMATED_TEXTURE_MASK, ANIMATED_TEXTURE_MASK, G_TX_NOLOD,  \
                        G_TX_NOLOD);                                                                           \
    for (i = 0; i < TRIANGLE_VERTEX_COUNT; i++) {                                                              \
        vertex = (Vtx *)FRAME_DATA_POINTER(slot, (triangle)->vertices + i * VTX_SIZE);                         \
        vertex->v.tc[0] = (sCoordinates)[i];                                                                   \
        vertex->v.tc[1] = (tCoordinates)[i];                                                                   \
    }

/* The size of a Vtx */
#define VTX_SIZE 0x10
/* From AnimatedTriangle.loadTextureBlock to the gDPLoadTextureBlock */
#define LOAD_TEXTURE_BLOCK_OFFSET 0x10
/* The alignment the RDP needs of a texture's address */
#define TEXTURE_ALIGNMENT 8
/* The texture coordinate mask of an animated triangle's tiles, 64 texels each way */
#define ANIMATED_TEXTURE_MASK 6

/* Makes a slot's model readable at CHARACTER_MODEL_ADDRESS, where its segment addresses point */
#define MAP_MODEL_BUFFER(slot)                                                                     \
    SetMultiTlb(0, TLB_PAGE_MODE_4K, CHARACTER_MODEL_PAGE_COUNT, CHARACTER_MODEL_ADDRESS,         \
                osVirtualToPhysical((slot)->modelBuffer))

/* The models with a row of sBallDistances, the playable characters */
#define PLAYABLE_CHARACTER_COUNT 18
/* sBallDistances columns: one per club kind, then this one */
#define BALL_DISTANCE_WOOD_APPROACH 4
#define BALL_DISTANCE_COUNT 5

/* The default ball distance for each club kind */
#define WOOD_BALL_DISTANCE 0x3600
#define IRON_BALL_DISTANCE 0x2400
#define PUTTER_BALL_DISTANCE 0x1800

/* The character points whose depth gives the ball distance for each club kind */
#define WOOD_DISTANCE_POINT 3
#define IRON_DISTANCE_POINT 4
#define WEDGE_DISTANCE_POINT 5
#define PUTTER_DISTANCE_POINT 6

/* A ball distance is this times the point's depth in world units */
#define POINT_DEPTH_TO_BALL_DISTANCE 1228.8f

/* The animations whose last frame WriteBallDistances measures for each club kind */
#define WOOD_DISTANCE_ANIMATION 0
#define IRON_DISTANCE_ANIMATION 3
#define WEDGE_DISTANCE_ANIMATION 6
#define PUTTER_DISTANCE_ANIMATION 9
/* The animation whose first frame it measures for the approach column */
#define WOOD_APPROACH_DISTANCE_ANIMATION 2

/* Where GetBallDistance shows the measured point position */
#define POINT_POSITION_TEXT_COLUMN 2
#define POINT_POSITION_TEXT_ROW 13

#define BALL_DISTANCE_SOURCE_SIZE 0x8000
#define BALL_DISTANCE_LINE_SIZE 0x100
/* The display list choices that hold each kind of club */
#define WOOD_DISPLAY_LIST_CHOICE 0
#define IRON_DISPLAY_LIST_CHOICE 1
#define WEDGE_DISPLAY_LIST_CHOICE 2
#define PUTTER_DISPLAY_LIST_CHOICE 3

/* The animation whose ANIMATION_EVENT_UNK6 frame GetUnk6EventFrame returns */
#define UNK6_EVENT_ANIMATION_INDEX 2

/*
 * How far PlaceAndViewCharacter puts the camera from the character, and how
 * far below it the view sits, in course units
 */
#define VIEW_DISTANCE 61440.0f
#define VIEW_DROP 15360.0f
#define COURSE_TO_WORLD_SCALE (1.0f / 1024.0f)
#define QUARTER_TURN_RADIANS 1.5707964f

void AdvanceTextureAnimations(u32 slotIndex, s32 unused, s32 frameLastDigit, s32 frame);

/* Set to the model buffer of the slot last set up or animated */
extern void *D_800C1DF4;
/* The animation LoadCharacter starts */
extern s32 D_800C1DF0;

/* How a slot starts out when LoadCharacter loads it */
#define LOADED_ALPHA 0x60
#define LOADED_RANDOM_TEXTURE_ANIMATION_DELAY 27

/* Lit along the scene's light direction, in colors that change near the end of the round */
extern Lights2 g_RoundCharacterLights[DRAW_BUFFER_COUNT];
/* Lit along the scene's light direction, in colors that never change */
extern Lights2 g_FixedCharacterLights[DRAW_BUFFER_COUNT];
/* The direction of each set's second light */
extern s32 D_800C1F90;
extern s32 D_800C1F94;
extern s32 D_800C1F98;

/* SetCharacterLights' colors near the end of the round */
#define LATE_AMBIENT_LEVEL 120
#define LATE_LIGHT_RED 200
#define LATE_LIGHT_GREEN 80
#define LATE_LIGHT_BLUE 10
/* SetCharacterLights' colors through the rest of the round */
#define DAY_AMBIENT_RED 130
#define DAY_AMBIENT_GREEN 130
#define DAY_AMBIENT_BLUE 120
#define DAY_LIGHT_RED 130
#define DAY_LIGHT_GREEN 120
#define DAY_LIGHT_BLUE 110
/* The colors of the lights that never change */
#define FIXED_AMBIENT_LEVEL 100
#define FIXED_LIGHT_RED 155
#define FIXED_LIGHT_GREEN 120
#define FIXED_LIGHT_BLUE 120
#define FIXED_SECOND_LIGHT_LEVEL 150

/* The size of a line of debug text */
#define DEBUG_LINE_SIZE 0x80

/* Where AnimateCharacter prints a slot's frame when GAME_FLAG_SHOW_ANIMATION_FRAMES is set */
#define ANIMATION_FRAME_TEXT_COLUMN 10
#define ANIMATION_FRAME_TEXT_FIRST_ROW 5

/* How many steps go by between plays of a slot's random texture animation */
#define RANDOM_TEXTURE_ANIMATION_DELAY_MIN 30
#define RANDOM_TEXTURE_ANIMATION_DELAY_MASK 0x1F

/* The slot that WriteBallDistances loads each character into */
#define MEASURED_SLOT 0
/* The shot mode that WriteBallDistances measures every column with */
#define MEASURED_SHOT_MODE SHOT_MODE_NORMAL

/* The source lines of pcsub.c that ReportFailedAssertion reports */
#define BALL_DISTANCE_SOURCE_ASSERT_LINE 1525
#define TEXTURE_ANIMATION_ASSERT_LINE 1956

/*
 * How far to the side of the ball each playable character stands, in course
 * units, for each club kind and then for an approach shot with a wood.
 * WriteBallDistances measures the models and writes a table like this as pcdist.c.
 */
static const s32 sBallDistances[PLAYABLE_CHARACTER_COUNT][BALL_DISTANCE_COUNT] = {
    { 0x2BDD, 0x2070, 0x1903, 0x114D, 0x28F4 },
    { 0x3502, 0x2910, 0x21DD, 0x1A86, 0x2DAA },
    { 0x2B74, 0x1EDA, 0x17DA, 0x10B1, 0x287A },
    { 0x2F33, 0x279F, 0x2578, 0x1F5D, 0x2F33 },
    { 0x3967, 0x258A, 0x1FDF, 0x15C7, 0x2B1C },
    { 0x3017, 0x24B5, 0x2229, 0x17C7, 0x249B },
    { 0x3624, 0x2B1D, 0x24D4, 0x1891, 0x33E9 },
    { 0x39E2, 0x3049, 0x2D05, 0x20E4, 0x3279 },
    { 0x4248, 0x2E12, 0x27B9, 0x1B35, 0x32A7 },
    { 0x34EC, 0x2A0B, 0x2311, 0x18AB, 0x341D },
    { 0x2BCD, 0x1EF7, 0x197B, 0x1636, 0x2807 },
    { 0x5D47, 0x5119, 0x4DB7, 0x37C5, 0x5D47 },
    { 0x5418, 0x4540, 0x3EC1, 0x3AF8, 0x5418 },
    { 0x34EC, 0x2A0B, 0x2311, 0x18AB, 0x341D },
    { 0x2CD0, 0x1DAC, 0x183A, 0x1321, 0x224E },
    { 0x271C, 0x198F, 0x161D, 0x1433, 0x271C },
    { 0x334B, 0x276E, 0x1F84, 0x17E4, 0x3017 },
    { 0x3295, 0x2776, 0x1FD8, 0x1931, 0x2BC9 },
};

/*
 * Returns how far to the side of the ball the character in the slot stands
 * for the club and shot mode. Playable characters' distances come from
 * sBallDistances unless GAME_FLAG_GENERATE_BALL_DISTANCES is set; otherwise the
 * model is measured, with a default for the club kind if that fails.
 * GetVectorLength is an unused nested function, a GNU C extension.
 */
#ifdef NON_MATCHING
s32 GetBallDistance(u32 slotIndex, s32 club, s32 isClubKind, s32 shotMode) {
    f32 position[3];
    char text[DEBUG_LINE_SIZE];
    s32 distance;
    s32 pointId = WOOD_DISTANCE_POINT;
    s32 column;
    s32 modelIndex;
    CharacterSlot *slot;
    s32 kind;
    f32 depth;

    f32 GetVectorLength(f32 x, f32 y, f32 z) {
        return sqrtf(x * x + y * y + z * z);
    }

    slot = GetUsedCharacterSlot(slotIndex);
    column = CLUB_KIND_WOOD;
    if (!isClubKind) {
        kind = GetCharacterClubKind(slotIndex, club);
    } else {
        kind = club;
    }
    switch (kind) {
        case CLUB_KIND_WOOD:
            distance = WOOD_BALL_DISTANCE;
            pointId = WOOD_DISTANCE_POINT;
            column = (shotMode == SHOT_MODE_APPROACH) * BALL_DISTANCE_WOOD_APPROACH;
            break;
        case CLUB_KIND_IRON:
            distance = IRON_BALL_DISTANCE;
            pointId = IRON_DISTANCE_POINT;
            column = CLUB_KIND_IRON;
            break;
        case CLUB_KIND_WEDGE:
            distance = IRON_BALL_DISTANCE;
            pointId = WEDGE_DISTANCE_POINT;
            column = CLUB_KIND_WEDGE;
            break;
        case CLUB_KIND_PUTTER:
            distance = PUTTER_BALL_DISTANCE;
            pointId = PUTTER_DISTANCE_POINT;
            column = CLUB_KIND_PUTTER;
            break;
    }
    if (!IsGameFlagSet(GAME_FLAG_GENERATE_BALL_DISTANCES) && slot != NULL && (modelIndex = slot->modelIndex) >= 0 &&
        (modelIndex %= CHARACTER_MODELS_PER_KIND) < PLAYABLE_CHARACTER_COUNT) {
        distance = sBallDistances[modelIndex][column];
    } else if (GetCharacterPointModelPosition(slotIndex, pointId, position) == 0) {
        sprintf(text, "pres %f,%f,%f", position[0], position[1], position[2]);
        PrintDebugText(text, POINT_POSITION_TEXT_COLUMN, POINT_POSITION_TEXT_ROW);
        depth = position[2] * POINT_DEPTH_TO_BALL_DISTANCE;
        if (depth < 0.0f) {
            depth = -depth;
        }
        distance = depth;
        osSyncPrintf(":");
    } else {
        osSyncPrintf("]");
    }
    return distance;
}

#else
INCLUDE_ASM("asm/nonmatchings/character", GetVectorLength);
INCLUDE_ASM("asm/nonmatchings/character", GetBallDistance);
#endif

/*
 * GCC last wrote sBallDistances, so it thinks the assembler is still in
 * .rodata, but INCLUDE_ASM left it in .text. Agree with GCC so the data below
 * lands in .rodata.
 */
__asm__(".section .rodata");

/* Assertion strings that WriteBallDistances and PlayTextureAnimation share */
static const char sAssertionExpression[] = "EX";
static const char sFileName[] = "c:/usgolf64/polychara/pcsub.c";

/*
 * Loads each playable character, measures its ball distance in each column of
 * sBallDistances and writes the source of that table to the host as pcdist.c.
 * AppendDistanceText is a nested function, a GNU C extension.
 */
void WriteBallDistances(void) {
    char line[BALL_DISTANCE_LINE_SIZE];
    char *cursor;
    char *end;
    char *source;
    s32 modelIndex;
    s32 column;
    s32 animationId;
    s32 kind;

    void AppendDistanceText(char *text) {
        char character;

        while (TRUE) {
            character = *text++;
            if (character == '\0') {
                break;
            }
            if (cursor < end) {
                *cursor++ = character;
            }
        }
    }

    cursor = source = AllocMemory(BALL_DISTANCE_SOURCE_SIZE);
    end = source + BALL_DISTANCE_SOURCE_SIZE;
    if (source == NULL) {
        ReportFailedAssertion(sAssertionExpression, sFileName, BALL_DISTANCE_SOURCE_ASSERT_LINE);
    }
    sprintf(line, "/* Caution! This file is automatically generated by pcsub.c*/\n"
                  "static const DISTPARA distp[PLAYABLE_NUM][5] = {\n");
    AppendDistanceText(line);
    for (modelIndex = 0; modelIndex < PLAYABLE_CHARACTER_COUNT; modelIndex++) {
        s32 distances[BALL_DISTANCE_COUNT];

        LoadCharacter(MEASURED_SLOT, modelIndex, 0);
        g_CharacterModelIndex = g_CharacterSlots[MEASURED_SLOT].modelIndex;
        for (column = 0; column < BALL_DISTANCE_COUNT; column++) {
            switch (column) {
                case CLUB_KIND_WOOD:
                    animationId = WOOD_DISTANCE_ANIMATION;
                    kind = CLUB_KIND_WOOD;
                    break;
                case CLUB_KIND_IRON:
                    animationId = IRON_DISTANCE_ANIMATION;
                    kind = CLUB_KIND_IRON;
                    break;
                case CLUB_KIND_WEDGE:
                    animationId = WEDGE_DISTANCE_ANIMATION;
                    kind = CLUB_KIND_WEDGE;
                    break;
                case CLUB_KIND_PUTTER:
                    animationId = PUTTER_DISTANCE_ANIMATION;
                    kind = CLUB_KIND_PUTTER;
                    break;
                case BALL_DISTANCE_WOOD_APPROACH:
                    animationId = WOOD_APPROACH_DISTANCE_ANIMATION;
                    kind = CLUB_KIND_WOOD;
                    break;
            }
            SetAnimationSpeed(MEASURED_SLOT, 0.0f);
            SetCharacterAnimation(MEASURED_SLOT, animationId);
            if (column < BALL_DISTANCE_WOOD_APPROACH) {
                SkipToAnimationEnd(MEASURED_SLOT);
            }
            AnimateCharacter(MEASURED_SLOT);
            distances[column] = GetBallDistance(MEASURED_SLOT, kind, TRUE, MEASURED_SHOT_MODE);
        }
        osSyncPrintf("\n");
        sprintf(line, "{%d, %d, %d, %d, %d},\n", distances[0], distances[1], distances[2], distances[3],
                distances[4]);
        osSyncPrintf(line);
        AppendDistanceText(line);
        FreeCharacterSlot(MEASURED_SLOT);
    }
    sprintf(line, "};\n");
    AppendDistanceText(line);
    line[0] = END_OF_FILE_CHARACTER;
    line[1] = '\0';
    AppendDistanceText(line);
    sprintf(line, "pcdist.c");
    WriteHostFile(line, cursor - source);
    FreeMemory(&source);
}

/* As above, so the data below lands in .rodata */
__asm__(".section .rodata");

/* Unreferenced; left by debug output that the routines below no longer have */
static const char sUnusedFrameFormat[] = "TA %d : FRAME %d    ";

void WriteBallDistancesIfEnabled(void) {
    if (IsGameFlagSet(GAME_FLAG_GENERATE_BALL_DISTANCES)) {
        WriteBallDistances();
    }
}

/* Has the character in the slot hold the club, by choosing the display list for its kind */
void SetCharacterClub(u32 slotIndex, s32 clubIndex) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    s32 choice;

    if (slot != NULL) {
        g_CharacterModelIndex = slot->modelIndex;
        switch (GetCharacterClubKind(slotIndex, clubIndex)) {
            case CLUB_KIND_WOOD:
                choice = WOOD_DISPLAY_LIST_CHOICE;
                break;
            case CLUB_KIND_IRON:
                choice = IRON_DISPLAY_LIST_CHOICE;
                break;
            case CLUB_KIND_WEDGE:
                choice = WEDGE_DISPLAY_LIST_CHOICE;
                break;
            case CLUB_KIND_PUTTER:
                choice = PUTTER_DISPLAY_LIST_CHOICE;
                break;
            default:
                choice = NO_DISPLAY_LIST_CHOICE;
                break;
        }
        MAP_MODEL_BUFFER(slot);
        SelectCharacterDisplayList(slotIndex, choice);
    }
}

/*
 * Copies the kind and value of up to EVENT_ACTION_COUNT_MAX of the
 * animation's events on the frame into actions, and returns how many it
 * copied
 */
s32 GetAnimationEventActions(u32 slotIndex, s32 animationIndex, s32 frame, AnimationEventAction *actions) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    s32 count = 0;
    AnimationEvent *event;
    s32 i;

    if (slot != NULL && animationIndex < slot->animationCount) {
        event = ANIMATIONS(slot->model)[animationIndex].events;
        if (event != NULL) {
            for (i = 0; event[i].frame != END_OF_ANIMATION_EVENTS; i++) {
                if (event[i].frame == frame) {
                    if (count < EVENT_ACTION_COUNT_MAX) {
                        actions[count].kind = event[i].kind;
                        actions[count].value = event[i].value;
                        count++;
                    }
                }
            }
        }
    }
    return count;
}

/* Returns the frame of the animation's first event of the kind, from its start, or -1 if none */
s32 GetAnimationEventFrame(u32 slotIndex, s32 animationIndex, s32 kind) {
    s32 frame = -1;
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    AnimationEvent *event;
    s32 i;

    if (slot != NULL && animationIndex < slot->animationCount) {
        event = ANIMATIONS(slot->model)[animationIndex].events;
        if (event != NULL) {
            for (i = 0; event[i].frame != END_OF_ANIMATION_EVENTS; i++) {
                if (event[i].kind == kind) {
                    frame = event[i].frame - ANIMATIONS(slot->model)[animationIndex].startFrame;
                    break;
                }
            }
        }
    }
    return frame;
}

/* Unreferenced */
s32 GetUnk6EventFrame(u32 slotIndex) {
    return GetAnimationEventFrame(slotIndex, UNK6_EVENT_ANIMATION_INDEX, ANIMATION_EVENT_UNK6);
}

/* Gives each of the slot's animated triangles its default texture */
#ifdef NON_MATCHING
void ResetTriangleTextures(u32 slotIndex) {
    s32 triangleIndex = 0;
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    AnimatedTriangle *triangle;
    AnimatedTexture *texture;
    Gfx *gfx;
    Vtx *vertex;
    s32 i;

    if (slot != NULL) {
        MAP_MODEL_BUFFER(slot);
        for (triangle = ANIMATED_TRIANGLES(slot->model)[0]; triangle != NULL;
             triangle = ANIMATED_TRIANGLES(slot->model)[++triangleIndex]) {
            texture = ANIMATED_TEXTURES(slot->model);
            if (triangle->textureIndex >= 0) {
                texture += triangle->textureIndex;
            }
            SET_TRIANGLE_TEXTURE(slot, triangle, texture, triangle->s, triangle->t);
        }
    }
}

#else
INCLUDE_ASM("asm/nonmatchings/character", ResetTriangleTextures);
#endif

/*
 * Shows the keyed textures of the slot's playing texture animations, then
 * advances each one a frame. The arguments after the slot are unused.
 */
#ifdef NON_MATCHING
void AdvanceTextureAnimations(u32 slotIndex, s32 unused, s32 frameLastDigit, s32 frame) {
    s32 triangleIndex = 0;
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    AnimatedTriangle *triangle;
    TriangleTextureKeys *keySet;
    TriangleTextureKey *key;
    TextureAnimationState *state;
    AnimatedTexture *texture;
    TextureAnimation *animation;
    Gfx *gfx;
    Vtx *vertex;
    s32 keySetIndex;
    s32 keyIndex;
    s32 i;

    if (slot != NULL) {
        MAP_MODEL_BUFFER(slot);
        for (triangle = ANIMATED_TRIANGLES(slot->model)[0]; triangle != NULL;
             triangle = ANIMATED_TRIANGLES(slot->model)[++triangleIndex]) {
            for (keySetIndex = 0; keySetIndex < triangle->keySetCount; keySetIndex++) {
                keySet = &triangle->keySets[keySetIndex];
                state = &slot->textureAnimations[keySet->textureAnimationIndex];
                if (state->pendingCopyCount != 0) {
                    for (keyIndex = 0; keyIndex < keySet->count; keyIndex++) {
                        key = &keySet->keys[keyIndex];
                        if (state->frame >= key->frame) {
                            texture = &ANIMATED_TEXTURES(slot->model)[key->textureIndex];
                            SET_TRIANGLE_TEXTURE(slot, triangle, texture, key->s, key->t);
                        }
                    }
                }
            }
        }
        for (i = 0; i < TEXTURE_ANIMATION_COUNT; i++) {
            animation = &TEXTURE_ANIMATIONS(slot->model)[i];
            if (animation->index < 0) {
                break;
            }
            state = &slot->textureAnimations[animation->index];
            if (state->pendingCopyCount >= TEXTURE_ANIMATION_PLAYING) {
                if (++state->frame >= animation->frameCount) {
                    state->pendingCopyCount = FRAME_DATA_COPY_COUNT;
                    if (animation->index == RANDOM_TEXTURE_ANIMATION) {
                        slot->textureAnimations[RANDOM_TEXTURE_ANIMATION].frame = 0;
                    } else {
                        state->frame = animation->frameCount;
                    }
                }
            } else if (state->pendingCopyCount != 0) {
                state->pendingCopyCount--;
            }
        }
    }
}

#else
INCLUDE_ASM("asm/nonmatchings/character", AdvanceTextureAnimations);
#endif

s32 SetTextureAnimationFrame(u32 slotIndex, s32 index, s32 frame) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->textureAnimations[index].frame = frame;
        slot->textureAnimations[index].pendingCopyCount = FRAME_DATA_COPY_COUNT;
    }
}

void StopTextureAnimations(u32 slotIndex) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    s32 i;

    for (i = TEXTURE_ANIMATION_COUNT - 1; i >= 0; i--) {
        slot->textureAnimations[i].pendingCopyCount = 0;
    }
}

s32 PlayTextureAnimation(u32 slotIndex, s32 index) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (index >= TEXTURE_ANIMATION_COUNT) {
        ReportFailedAssertion(sAssertionExpression, sFileName, TEXTURE_ANIMATION_ASSERT_LINE);
    }
    if (slot != NULL) {
        slot->textureAnimations[index].pendingCopyCount = TEXTURE_ANIMATION_PLAYING;
        slot->textureAnimations[index].frame = 0;
    }
}

/*
 * Gives the slot's triangles that the texture animation changes their default
 * texture. Nothing calls it.
 */
#ifdef NON_MATCHING
void ResetAnimationTriangleTextures(u32 slotIndex, s32 textureAnimationIndex) {
    s32 triangleIndex = 0;
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    AnimatedTriangle *triangle;
    AnimatedTexture *texture;
    Gfx *gfx;
    Vtx *vertex;
    s32 keySetIndex;
    s32 i;

    if (slot != NULL) {
        MAP_MODEL_BUFFER(slot);
        for (triangle = ANIMATED_TRIANGLES(slot->model)[0]; triangle != NULL;
             triangle = ANIMATED_TRIANGLES(slot->model)[++triangleIndex]) {
            texture = &ANIMATED_TEXTURES(slot->model)[triangle->textureIndex];
            for (keySetIndex = 0; keySetIndex < triangle->keySetCount; keySetIndex++) {
                if (triangle->keySets[keySetIndex].textureAnimationIndex == textureAnimationIndex) {
                    SET_TRIANGLE_TEXTURE(slot, triangle, texture, triangle->s, triangle->t);
                }
            }
        }
    }
}

#else
INCLUDE_ASM("asm/nonmatchings/character", ResetAnimationTriangleTextures);
#endif

/*
 * Loads the model of the character of the kind into the slot, sets up its
 * frame data copies, and starts its first animation
 */
#ifdef NON_MATCHING
/* As above, so the data below lands in .rodata */
__asm__(".section .rodata");

static const char sNotDisposedFormat[] = "Polychara slot %d not disposed yet!!!!!!!\n";
/* Unreferenced; left by debug output that LoadCharacter no longer has */
static const char sUnusedReadRomFormat[] = "nuPiReadRom(%x, %x, %x) [bufsiz %x]\n";
static const char sUnusedCompleteText[] = "complete\n";

void LoadCharacter(u32 slotIndex, s32 character, s32 kind) {
    s32 isPatched = FALSE;
    CharacterSlot *slot = &g_CharacterSlots[slotIndex];
    JointAnimation *joint;
    CharacterAnimation *animation;
    u32 frameDataSize;
    s32 count;

    if (kind < CHARACTER_MODEL_KIND_COUNT) {
        g_CharacterModelIndex = (kind & (CHARACTER_MODEL_KIND_COUNT - 1)) * CHARACTER_MODELS_PER_KIND +
                                character % CHARACTER_MODELS_PER_KIND;
        slot->model = &g_CharacterModels[0][g_CharacterModelIndex];
    } else {
        g_CharacterModelIndex = (u32)character % OTHER_CHARACTER_MODEL_COUNT;
        slot->model = &g_OtherCharacterModels[g_CharacterModelIndex];
    }
    if (IsCharacterSlotBusy(slotIndex)) {
        /* Printed five times, so it stands out */
        osSyncPrintf(sNotDisposedFormat, slotIndex);
        osSyncPrintf(sNotDisposedFormat, slotIndex);
        osSyncPrintf(sNotDisposedFormat, slotIndex);
        osSyncPrintf(sNotDisposedFormat, slotIndex);
        osSyncPrintf(sNotDisposedFormat, slotIndex);
    } else if (slot->modelBuffer != NULL) {
        if (slot->model->romStart != slot->model->baseRomStart) {
            isPatched = TRUE;
        }
        slot->modelIndex = g_CharacterModelIndex;
        slot->alpha = LOADED_ALPHA;
        slot->nextAnimationId = NO_NEXT_ANIMATION;
        slot->frameDataIndex = 0;
        slot->animationIndex = 0;
        slot->isTranslucent = FALSE;
        slot->lighting = 0;
        slot->isLooping = TRUE;
        slot->playsRandomTextureAnimation = TRUE;
        slot->randomTextureAnimationDelay = LOADED_RANDOM_TEXTURE_ANIMATION_DELAY;
        slot->unk188 = 0;
        slot->unk189 = 0;
        D_800C1DF4 = slot->modelBuffer;
        if (isPatched) {
            LoadLzData(slot->model->baseRomStart, D_800C1DF4, slot->model->baseRomEnd - slot->model->baseRomStart);
            LoadPatchedLzData(slot->model->romStart, D_800C1DF4, slot->model->romEnd - slot->model->romStart);
        } else {
            LoadLzData(slot->model->romStart, D_800C1DF4, slot->model->romEnd - slot->model->romStart);
        }
        SetMultiTlb(0, TLB_PAGE_MODE_4K, CHARACTER_MODEL_PAGE_COUNT, CHARACTER_MODEL_ADDRESS,
                    osVirtualToPhysical(D_800C1DF4));
        frameDataSize = slot->model->frameDataEnd - slot->model->frameDataStart;
        if (frameDataSize < FRAME_DATA_COPY_SIZE) {
            frameDataSize = FRAME_DATA_COPY_SIZE;
        }
        slot->frameData[0] = OS_PHYSICAL_TO_K0(osVirtualToPhysical((void *)slot->model->frameDataStart));
        slot->frameData[1] = (u8 *)slot->frameData[0] + frameDataSize;
        slot->frameData[2] = (u8 *)slot->frameData[1] + frameDataSize;
        memcpy(slot->frameData[1], slot->frameData[0], frameDataSize);
        memcpy(slot->frameData[2], slot->frameData[0], frameDataSize);
        g_CurrentJointMatrices = slot->frameData[slot->frameDataIndex];
        D_800C1DF0 = slot->animationIndex;
        if (IsGameFlagSet(GAME_FLAG_LOG_CHARACTER_SETUP)) {
            osSyncPrintf("complete2 anims_addr %x\n", ANIMATIONS(slot->model));
        }
        slot->animationFrame = slot->animationStartFrame = ANIMATIONS(slot->model)[D_800C1DF0].startFrame;
        slot->animationEndFrame = slot->animationStartFrame + ANIMATIONS(slot->model)[D_800C1DF0].frameCount;
        g_CurrentAnimationFrame = slot->animationFrame;
        g_CurrentAnimationStartFrame = slot->animationStartFrame;
        g_CurrentAnimationEndFrame = slot->animationEndFrame;
        if (IsGameFlagSet(GAME_FLAG_LOG_CHARACTER_SETUP)) {
            osSyncPrintf("loop %d, loop_min = %d, loop_max = %d\n", D_800C1DF0, g_CurrentAnimationStartFrame,
                         g_CurrentAnimationEndFrame);
        }
        ResetTriangleTextures(slotIndex);
        slot->frameDataIndex = 1;
        ResetTriangleTextures(slotIndex);
        slot->frameDataIndex = 2;
        ResetTriangleTextures(slotIndex);
        slot->frameDataIndex = 0;
        StopTextureAnimations(slotIndex);
        AnimateCharacterJoints(slotIndex);
        joint = JOINT_ANIMATIONS(slot->model);
        for (; joint->translationKeyCount != END_OF_JOINT_ANIMATIONS; joint++) {
            joint->translationSpan.endFrame = UNSET_KEYFRAME_SPAN;
            joint->translationSpan.startFrame = UNSET_KEYFRAME_SPAN;
            joint->rotationSpan.endFrame = UNSET_KEYFRAME_SPAN;
            joint->rotationSpan.startFrame = UNSET_KEYFRAME_SPAN;
            joint->scaleSpan.endFrame = UNSET_KEYFRAME_SPAN;
            joint->scaleSpan.startFrame = UNSET_KEYFRAME_SPAN;
        }
        slot->jointMatrices = g_CurrentJointMatrices;
        slot->displayList = (Gfx *)slot->model->segmentAddresses[CHARACTER_MODEL_DISPLAY_LIST];
        slot->unk68[0] = *(f32 *)slot->model->segmentAddresses[CHARACTER_MODEL_UNK68];
        slot->unk68[1] = *(f32 *)slot->model->segmentAddresses[CHARACTER_MODEL_UNK68 + 1];
        slot->unk68[2] = *(f32 *)slot->model->segmentAddresses[CHARACTER_MODEL_UNK68 + 2];
        slot->unk68[3] = *(f32 *)slot->model->segmentAddresses[CHARACTER_MODEL_UNK68 + 3];
        slot->unk68[4] = *(f32 *)slot->model->segmentAddresses[CHARACTER_MODEL_UNK68 + 4];
        slot->unk68[5] = *(f32 *)slot->model->segmentAddresses[CHARACTER_MODEL_UNK68 + 5];
        slot->unk80 = slot->model->unk44;
        count = 0;
        for (animation = ANIMATIONS(slot->model); animation->events != NULL; animation++) {
            count++;
        }
        slot->animationCount = count;
        slot->animationSpeed = 1.0f;
        if (IsGameFlagSet(GAME_FLAG_LOG_CHARACTER_SETUP)) {
            osSyncPrintf("stage01 inited, char %d.\n", g_CharacterModelIndex);
        }
    }
}

#else
INCLUDE_ASM("asm/nonmatchings/character", LoadCharacter);
#endif

s32 GetAnimationIndex(u32 slotIndex, s32 id) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    s32 foundIndex = 0;
    CharacterAnimation *animation;
    s32 i;

    if (slot != NULL) {
        animation = ANIMATIONS(slot->model);
        for (i = 0; animation->events != NULL; i++, animation++) {
            if (animation->id == id) {
                foundIndex = i;
                break;
            }
        }
    }
    return foundIndex;
}

#ifdef NON_MATCHING
CharacterAnimation *FindAnimation(u32 slotIndex, s32 id) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot == NULL) {
        return NULL;
    }
    MAP_MODEL_BUFFER(slot);
    id = GetAnimationIndex(slotIndex, id);
    if (ANIMATIONS(slot->model)[id].events == NULL) {
        return NULL;
    }
    return &ANIMATIONS(slot->model)[id];
}

#else
INCLUDE_ASM("asm/nonmatchings/character", FindAnimation);
#endif

void SetCharacterAnimation(u32 slotIndex, s32 id) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    JointAnimation *joint;

    if (slot != NULL) {
        MAP_MODEL_BUFFER(slot);
        slot->animationIndex = GetAnimationIndex(slotIndex, id);
        if (ANIMATIONS(slot->model)[slot->animationIndex].events == NULL) {
            slot->animationIndex = 0;
        }
        slot->animationFrame = slot->animationStartFrame = ANIMATIONS(slot->model)[slot->animationIndex].startFrame;
        slot->animationEndFrame =
            slot->animationStartFrame + ANIMATIONS(slot->model)[slot->animationIndex].frameCount;
        joint = JOINT_ANIMATIONS(slot->model);
        for (; joint->translationKeyCount != END_OF_JOINT_ANIMATIONS; joint++) {
            joint->translationSpan.endFrame = UNSET_KEYFRAME_SPAN;
            joint->translationSpan.startFrame = UNSET_KEYFRAME_SPAN;
            joint->rotationSpan.endFrame = UNSET_KEYFRAME_SPAN;
            joint->rotationSpan.startFrame = UNSET_KEYFRAME_SPAN;
            joint->scaleSpan.endFrame = UNSET_KEYFRAME_SPAN;
            joint->scaleSpan.startFrame = UNSET_KEYFRAME_SPAN;
        }
    }
}

void SetPlaysRandomTextureAnimation(u32 slotIndex, s32 value) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->playsRandomTextureAnimation = value;
    }
}

void SkipToAnimationEnd(u32 slotIndex) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->animationFrame = slot->animationEndFrame;
    }
}

void AdvanceAnimation(u32 slotIndex, s32 frameCount) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->animationFrame += frameCount;
        if (slot->animationEndFrame <= slot->animationFrame) {
            slot->animationFrame = slot->animationEndFrame;
        }
    }
}

void SetAnimationSpeed(u32 slotIndex, f32 speed) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->animationSpeed = speed;
    }
}

void SetAnimationLooping(u32 slotIndex, s32 value) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->isLooping = value;
    }
}

void SetNextAnimation(u32 slotIndex, s32 value) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->nextAnimationId = value;
    }
}

void SetCharacterTranslucent(u32 slotIndex, s32 value) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->isTranslucent = value;
    }
}

void SetCharacterLighting(u32 slotIndex, s32 value) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->lighting = value;
    }
}

void SetCharacterSlotAlpha(u32 slotIndex, s32 value) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->alpha = value;
    }
}

s32 GetCurrentAnimationEndFrame(u32 slotIndex) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    s32 endFrame = -1;

    if (slot != NULL) {
        endFrame = slot->animationEndFrame;
    }
    return endFrame;
}

s32 GetAnimationEndFrame(u32 slotIndex, s32 id) {
    CharacterAnimation *animation = FindAnimation(slotIndex, id);

    if (animation != NULL) {
        return animation->startFrame + animation->frameCount;
    }
    return -1;
}

s32 GetAnimationStartFrame(u32 slotIndex, s32 id) {
    CharacterAnimation *animation = FindAnimation(slotIndex, id);

    if (animation != NULL) {
        return animation->startFrame;
    }
    return -1;
}

s32 GetAnimationFrameCount(u32 slotIndex, s32 id) {
    CharacterAnimation *animation = FindAnimation(slotIndex, id);

    if (animation != NULL) {
        return animation->frameCount;
    }
    return -1;
}

void SetCharacterUnk188(u32 slotIndex, s32 value) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->unk188 = value;
    }
}

s32 SetCharacterUnk189(u32 slotIndex) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->unk189 = TRUE;
    }
}

s32 GetAnimationFrame(u32 slotIndex) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    s32 frame = -1;

    if (slot != NULL) {
        frame = slot->animationFrame;
    }
    return frame;
}

/*
 * Steps the slot's animation: carries out the events of the frame, advances
 * the frame, and poses the joints into the next frame data copy
 */
void AnimateCharacter(u32 slotIndex) {
    AnimationEventAction actions[EVENT_ACTION_COUNT_MAX];
    char text[DEBUG_LINE_SIZE];
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    s32 i = 0;
    s32 actionCount;
    s32 hasLooped;

    if (slot != NULL) {
        hasLooped = FALSE;
        MAP_MODEL_BUFFER(slot);
        actionCount = GetAnimationEventActions(slotIndex, slot->animationIndex, slot->animationFrame, actions);
        for (; i < actionCount; i++) {
            switch (actions[i].kind) {
                case ANIMATION_EVENT_LOOP_START:
                    break;
                case ANIMATION_EVENT_LOOP_END:
                    if (slot->isLooping) {
                        hasLooped = TRUE;
                        slot->animationFrame =
                            GetAnimationEventFrame(slotIndex, slot->animationIndex, ANIMATION_EVENT_LOOP_START) +
                            ANIMATIONS(slot->model)[slot->animationIndex].startFrame;
                    }
                    break;
                case ANIMATION_EVENT_TEXTURE_ANIMATION:
                    if (actions[i].value == TEXTURE_ANIMATION_EVENT_START_RANDOM) {
                        slot->playsRandomTextureAnimation = TRUE;
                    } else if (actions[i].value == TEXTURE_ANIMATION_EVENT_STOP_RANDOM) {
                        slot->playsRandomTextureAnimation = FALSE;
                    } else {
                        PlayTextureAnimation(slotIndex, actions[i].value);
                    }
                    break;
            }
        }
        if (!hasLooped) {
            slot->animationFrame += slot->animationSpeed;
        }
        if (slot->animationFrame > slot->animationEndFrame) {
            if (slot->nextAnimationId != NO_NEXT_ANIMATION) {
                SetCharacterAnimation(slotIndex, slot->nextAnimationId);
                slot->nextAnimationId = NO_NEXT_ANIMATION;
            } else {
                slot->animationFrame = slot->animationEndFrame;
            }
        }
        if (slot->animationFrame < slot->animationStartFrame) {
            slot->animationFrame = slot->animationStartFrame;
        }
        g_CharacterModelIndex = slot->modelIndex;
        g_CurrentAnimationFrame = slot->animationFrame;
        g_CurrentAnimationEndFrame = slot->animationEndFrame;
        g_CurrentAnimationStartFrame = slot->animationStartFrame;
        if (IsGameFlagSet(GAME_FLAG_SHOW_ANIMATION_FRAMES)) {
            sprintf(text, "[PC%d Frame%d]         ", slotIndex,
                    (s32)(g_CurrentAnimationFrame - slot->animationStartFrame));
            PrintDebugText(text, ANIMATION_FRAME_TEXT_COLUMN, slotIndex + ANIMATION_FRAME_TEXT_FIRST_ROW);
        }
        if (slot->playsRandomTextureAnimation) {
            if (slot->randomTextureAnimationDelay == 0) {
                PlayTextureAnimation(slotIndex, RANDOM_TEXTURE_ANIMATION);
                slot->randomTextureAnimationDelay =
                    (rand() & RANDOM_TEXTURE_ANIMATION_DELAY_MASK) + RANDOM_TEXTURE_ANIMATION_DELAY_MIN;
            } else {
                slot->randomTextureAnimationDelay--;
            }
        }
        AdvanceTextureAnimations(slotIndex, 0, (s32)g_CurrentAnimationFrame % 10, g_CurrentAnimationFrame);
        g_CurrentJointMatrices = slot->frameData[slot->frameDataIndex];
        D_800C1DF4 = slot->modelBuffer;
        slot->frameDataIndex = (slot->frameDataIndex + 1) % FRAME_DATA_COPY_COUNT;
        AnimateCharacterJoints(slotIndex);
        slot->jointMatrices = g_CurrentJointMatrices;
    }
}

/* As above, so the data below lands in .rodata */
__asm__(".section .rodata");

/* The double 1.0, which func_800568CC loads as two words, aligned as a double is */
const u32 D_800D07E8 __attribute__((aligned(8))) = 0x3FF00000;
const u32 D_800D07EC = 0;

INCLUDE_ASM("asm/nonmatchings/character", func_800568CC);

/*
 * Steps the slot's animation as AnimateCharacter does, without the debug text,
 * but poses the joints into the frame data copy the next frame writes, without
 * moving on from it
 */
void AnimateCharacterInSameCopy(u32 slotIndex) {
    AnimationEventAction actions[EVENT_ACTION_COUNT_MAX];
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    s32 i = 0;
    s32 actionCount;
    s32 hasLooped;

    if (slot != NULL) {
        hasLooped = FALSE;
        MAP_MODEL_BUFFER(slot);
        actionCount = GetAnimationEventActions(slotIndex, slot->animationIndex, slot->animationFrame, actions);
        for (; i < actionCount; i++) {
            switch (actions[i].kind) {
                case ANIMATION_EVENT_LOOP_START:
                    break;
                case ANIMATION_EVENT_LOOP_END:
                    if (slot->isLooping) {
                        hasLooped = TRUE;
                        slot->animationFrame =
                            GetAnimationEventFrame(slotIndex, slot->animationIndex, ANIMATION_EVENT_LOOP_START) +
                            ANIMATIONS(slot->model)[slot->animationIndex].startFrame;
                    }
                    break;
                case ANIMATION_EVENT_TEXTURE_ANIMATION:
                    if (actions[i].value == TEXTURE_ANIMATION_EVENT_START_RANDOM) {
                        slot->playsRandomTextureAnimation = TRUE;
                    } else if (actions[i].value == TEXTURE_ANIMATION_EVENT_STOP_RANDOM) {
                        slot->playsRandomTextureAnimation = FALSE;
                    } else {
                        PlayTextureAnimation(slotIndex, actions[i].value);
                    }
                    break;
            }
        }
        if (!hasLooped) {
            slot->animationFrame += slot->animationSpeed;
        }
        if (slot->animationFrame > slot->animationEndFrame) {
            if (slot->nextAnimationId != NO_NEXT_ANIMATION) {
                SetCharacterAnimation(slotIndex, slot->nextAnimationId);
                slot->nextAnimationId = NO_NEXT_ANIMATION;
            } else {
                slot->animationFrame = slot->animationEndFrame;
            }
        }
        if (slot->animationFrame < slot->animationStartFrame) {
            slot->animationFrame = slot->animationStartFrame;
        }
        g_CharacterModelIndex = slot->modelIndex;
        g_CurrentAnimationFrame = slot->animationFrame;
        g_CurrentAnimationEndFrame = slot->animationEndFrame;
        g_CurrentAnimationStartFrame = slot->animationStartFrame;
        if (slot->playsRandomTextureAnimation) {
            if (slot->randomTextureAnimationDelay == 0) {
                PlayTextureAnimation(slotIndex, RANDOM_TEXTURE_ANIMATION);
                slot->randomTextureAnimationDelay =
                    (rand() & RANDOM_TEXTURE_ANIMATION_DELAY_MASK) + RANDOM_TEXTURE_ANIMATION_DELAY_MIN;
            } else {
                slot->randomTextureAnimationDelay--;
            }
        }
        AdvanceTextureAnimations(slotIndex, 0, (s32)g_CurrentAnimationFrame % 10, g_CurrentAnimationFrame);
        g_CurrentJointMatrices = slot->frameData[slot->frameDataIndex];
        D_800C1DF4 = slot->modelBuffer;
        AnimateCharacterJoints(slotIndex);
        slot->jointMatrices = g_CurrentJointMatrices;
    }
}

void AnimateAllCharacters(void) {
    s32 i;

    for (i = 0; i < CHARACTER_SLOT_COUNT; i++) {
        AnimateCharacter(i);
    }
}

s32 GetCharacterBufferSize(void) {
    return CHARACTER_BUFFER_SIZE;
}

void SetCharacterBuffer(s32 slotIndex, u8 *buffer) {
    /* Aligned for SetMultiTlb */
    u32 alignedBuffer = ((u32)buffer + SMALLEST_PAGE_SIZE - 1) & ~(SMALLEST_PAGE_SIZE - 1);
    s32 warningCount;
    s32 i;

    if (g_CharacterSlots[slotIndex].modelIndex != NO_CHARACTER_MODEL) {
        for (warningCount = 0; warningCount < WARNING_REPEAT_COUNT; warningCount++) {
            osSyncPrintf("Polychara %d still alive. You can't change buffer.\n", slotIndex);
        }
    } else {
        g_CharacterSlots[slotIndex].modelBuffer = (void *)alignedBuffer;
        for (i = 0; i < FRAME_DATA_COPY_COUNT; i++) {
            g_CharacterSlots[slotIndex].frameData[i] =
                (u8 *)g_CharacterSlots[slotIndex].modelBuffer + FRAME_DATA_OFFSET + i * FRAME_DATA_COPY_SIZE;
        }
    }
}

void ResetCharacterSlots(void) {
    s32 i;

    FreeAllCharacterSlots();
    for (i = 0; i < CHARACTER_SLOT_COUNT; i++) {
        g_CharacterSlots[i].lastDrawnFrame = LONG_AGO_FRAME;
        SetCharacterBuffer(i, (u8 *)DEFAULT_CHARACTER_BUFFER);
    }
}

/*
 * Sets the two sets of character lights for the frame being drawn, both lit
 * along the scene's light direction: one whose colors change near the end of
 * the round, and one whose colors never change
 */
void SetCharacterLights(void) {
    u8 ambientColor[3];
    u8 lightColor[3];
    s8 lightDirection[3];
    f32 lightPitch;
    f32 lightYaw;

    switch (g_HoleConditions.sceneColorIndex) {
        case SCENE_COLOR_INDEX_ROUND_LATE:
        case SCENE_COLOR_INDEX_ROUND_END:
            ambientColor[0] = LATE_AMBIENT_LEVEL;
            ambientColor[1] = LATE_AMBIENT_LEVEL;
            ambientColor[2] = LATE_AMBIENT_LEVEL;
            lightColor[0] = LATE_LIGHT_RED;
            lightColor[1] = LATE_LIGHT_GREEN;
            lightColor[2] = LATE_LIGHT_BLUE;
            break;
        default:
            ambientColor[0] = DAY_AMBIENT_RED;
            ambientColor[1] = DAY_AMBIENT_GREEN;
            ambientColor[2] = DAY_AMBIENT_BLUE;
            lightColor[0] = DAY_LIGHT_RED;
            lightColor[1] = DAY_LIGHT_GREEN;
            lightColor[2] = DAY_LIGHT_BLUE;
            break;
    }
    lightPitch = g_LightPitch * BINARY_ANGLE_TO_RADIANS;
    lightYaw = g_LightYaw * BINARY_ANGLE_TO_RADIANS;
    lightDirection[0] = -sinf(lightPitch) * cosf(lightYaw) * LIGHT_DIRECTION_SCALE;
    lightDirection[1] = -cosf(lightPitch) * LIGHT_DIRECTION_SCALE;
    lightDirection[2] = -sinf(lightPitch) * sinf(lightYaw) * LIGHT_DIRECTION_SCALE;
    g_RoundCharacterLights[g_DrawBufferIndex] = (Lights2)gdSPDefLights2(
        ambientColor[0], ambientColor[1], ambientColor[2], lightColor[0], lightColor[1], lightColor[2],
        lightDirection[0], lightDirection[1], lightDirection[2], lightColor[0], lightColor[1], lightColor[2],
        D_800C1F90, D_800C1F94, D_800C1F98);
    g_FixedCharacterLights[g_DrawBufferIndex] = (Lights2)gdSPDefLights2(
        FIXED_AMBIENT_LEVEL, FIXED_AMBIENT_LEVEL, FIXED_AMBIENT_LEVEL, FIXED_LIGHT_RED, FIXED_LIGHT_GREEN,
        FIXED_LIGHT_BLUE, lightDirection[0], lightDirection[1], lightDirection[2], FIXED_SECOND_LIGHT_LEVEL,
        FIXED_SECOND_LIGHT_LEVEL, FIXED_SECOND_LIGHT_LEVEL, D_800C1F90, D_800C1F94, D_800C1F98);
}

INCLUDE_ASM("asm/nonmatchings/character", func_80057FFC);

/* Places the character in the slot with func_800453E0, turned to the current shot's direction */
void PlaceCharacterForShot(u32 slotIndex) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        func_800453E0(slot->position, slot->rotation, slot->scale);
        slot->rotation[AXIS_Y] = ((-g_CurrentShot.unk0) & BINARY_ANGLE_MASK) * BINARY_ANGLE_TO_RADIANS;
    }
}

/*
 * Places the character in the slot with func_800989EC, then points the first
 * camera at it from VIEW_DISTANCE away along its facing
 */
void PlaceAndViewCharacter(u32 slotIndex) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        func_800989EC(slot->position, slot->rotation, slot->scale);
        g_Cameras[0].view.lookAt[AXIS_X] = slot->position[AXIS_X] * COURSE_TO_WORLD_SCALE;
        g_Cameras[0].view.lookAt[AXIS_Y] = (slot->position[AXIS_Y] - VIEW_DROP) * COURSE_TO_WORLD_SCALE;
        g_Cameras[0].view.lookAt[AXIS_Z] = slot->position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        g_Cameras[0].view.eye[AXIS_X] =
            (slot->position[AXIS_X] + cosf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * VIEW_DISTANCE) *
            COURSE_TO_WORLD_SCALE;
        g_Cameras[0].view.eye[AXIS_Y] = (slot->position[AXIS_Y] - VIEW_DROP) * COURSE_TO_WORLD_SCALE;
        g_Cameras[0].view.eye[AXIS_Z] =
            (slot->position[AXIS_Z] + sinf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * VIEW_DISTANCE) *
            COURSE_TO_WORLD_SCALE;
    }
}

/* Stands the character in the slot on the ground at the point, facing the heading, at full size */
void PlaceCharacter(u32 slotIndex, CoursePoint point, f32 heading) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->position[AXIS_X] = point[AXIS_X];
        slot->position[AXIS_Z] = point[AXIS_Z];
        slot->position[AXIS_Y] = GetGroundHeight(slot->position[AXIS_X], slot->position[AXIS_Z]);
        slot->rotation[AXIS_X] = 0.0f;
        slot->rotation[AXIS_Z] = 0.0f;
        slot->rotation[AXIS_Y] = -heading;
        slot->scale[AXIS_X] = 1.0f;
        slot->scale[AXIS_Y] = 1.0f;
        slot->scale[AXIS_Z] = 1.0f;
    }
}

INCLUDE_ASM("asm/nonmatchings/character", func_80058D04);

