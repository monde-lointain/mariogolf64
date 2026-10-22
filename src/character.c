#include "common.h"
#include "character_slot.h"
#include "club.h"
#include "debug_text.h"
#include "functions.h"
#include "game_flags.h"
#include "heap.h"
#include "host_file.h"
#include "main.h"

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
    char text[0x80];
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

INCLUDE_ASM("asm/nonmatchings/character", func_800550D8);

INCLUDE_ASM("asm/nonmatchings/character", func_80055368);

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

INCLUDE_ASM("asm/nonmatchings/character", func_80055828);

INCLUDE_ASM("asm/nonmatchings/character", LoadCharacter);

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

INCLUDE_ASM("asm/nonmatchings/character", AnimateCharacter);

/* As above, so the data below lands in .rodata */
__asm__(".section .rodata");

/* The double 1.0, which func_800568CC loads as two words */
const u32 D_800D07E8 = 0x3FF00000;
const u32 D_800D07EC = 0;

INCLUDE_ASM("asm/nonmatchings/character", func_800568CC);

INCLUDE_ASM("asm/nonmatchings/character", func_800574A8);

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

INCLUDE_ASM("asm/nonmatchings/character", func_80057914);

/* As above, so the data below lands in .rodata */
__asm__(".section .rodata");

const u8 D_800D0834[4] = { 0x9B, 0x78, 0x78, 0x00 };
const u8 D_800D0838[8] = { 0x96, 0x96, 0x96, 0x00, 0x00, 0x00, 0x00, 0x00 };

INCLUDE_ASM("asm/nonmatchings/character", func_80057FFC);

INCLUDE_ASM("asm/nonmatchings/character", func_80058ACC);

INCLUDE_ASM("asm/nonmatchings/character", func_80058B34);

INCLUDE_ASM("asm/nonmatchings/character", func_80058C58);

INCLUDE_ASM("asm/nonmatchings/character", func_80058D04);

