/*
 * Characterizes where AssignCharacterBuffers puts the buffers of the three
 * character slots that share g_WorkArea. SetCharacterBuffer rounds each up
 * to a page, and the original's buffers lie at fixed offsets from the work
 * area's first page boundary. Every build must put them there, wherever its
 * layout puts g_WorkArea.
 */

#include <ultra64.h>

#include "characters/character.h"
#include "characters/character_model.h"
#include "course/work_area.h"
#include "math/math_util.h"
#include "system/main.h"
#include "unity_fixture.h"

// The slots that get a buffer in g_WorkArea, and where the original puts
// each one's, from the work area's first page boundary
#define FIRST_BUFFER_SLOT 1
#define BUFFER_SLOT_COUNT 3
static const u32 f_OriginalBufferOffsets[BUFFER_SLOT_COUNT] = {0x0, 0x47000, 0x24000};

// The slots as they were before each test, which the test puts back
static CharacterSlot f_SavedSlots[CHARACTER_SLOT_COUNT];

// The offset of a slot's buffer from the work area's first page boundary
static u32 BufferOffset(s32 slotIndex) {
    u32 firstPage = RoundUpTo((u32)g_WorkArea.bytes, SMALLEST_PAGE_SIZE);

    return (u32)g_CharacterSlots[slotIndex].modelBuffer - firstPage;
}

TEST_GROUP(CharacterBuffers);

TEST_SETUP(CharacterBuffers) {
    s32 slotIndex;

    for (slotIndex = 0; slotIndex < CHARACTER_SLOT_COUNT; slotIndex++) {
        f_SavedSlots[slotIndex] = g_CharacterSlots[slotIndex];
        g_CharacterSlots[slotIndex].modelIndex = NO_CHARACTER_MODEL;
    }
    AssignCharacterBuffers();
}

TEST_TEAR_DOWN(CharacterBuffers) {
    s32 slotIndex;

    for (slotIndex = 0; slotIndex < CHARACTER_SLOT_COUNT; slotIndex++) {
        g_CharacterSlots[slotIndex] = f_SavedSlots[slotIndex];
    }
}

TEST(CharacterBuffers, LieAtTheOriginalsOffsetsFromTheFirstPage) {
    u32 offsets[BUFFER_SLOT_COUNT];
    s32 i;

    for (i = 0; i < BUFFER_SLOT_COUNT; i++) {
        offsets[i] = BufferOffset(FIRST_BUFFER_SLOT + i);
    }
    TEST_ASSERT_EQUAL_HEX32_ARRAY(f_OriginalBufferOffsets, offsets, BUFFER_SLOT_COUNT);
}

TEST(CharacterBuffers, EachEndsInTheWorkArea) {
    u32 workAreaEnd = (u32)g_WorkArea.bytes + WORK_AREA_SIZE;
    s32 i;

    for (i = FIRST_BUFFER_SLOT; i < FIRST_BUFFER_SLOT + BUFFER_SLOT_COUNT; i++) {
        u32 bufferEnd = (u32)g_CharacterSlots[i].modelBuffer + GetCharacterBufferSize();

        TEST_ASSERT_TRUE(bufferEnd <= workAreaEnd);
    }
}

// SetCharacterBuffer leaves a slot whose model is loaded where it was
TEST(CharacterBuffers, LoadedSlotKeepsItsBuffer) {
    void *buffer = g_CharacterSlots[FIRST_BUFFER_SLOT].modelBuffer;

    g_CharacterSlots[FIRST_BUFFER_SLOT].modelIndex = 0;
    SetCharacterBuffer(FIRST_BUFFER_SLOT, g_WorkArea.bytes + WORK_AREA_SIZE / 2);
    TEST_ASSERT_EQUAL_PTR(buffer, g_CharacterSlots[FIRST_BUFFER_SLOT].modelBuffer);
}

TEST_GROUP_RUNNER(CharacterBuffers) {
    RUN_TEST_CASE(CharacterBuffers, LieAtTheOriginalsOffsetsFromTheFirstPage);
    RUN_TEST_CASE(CharacterBuffers, EachEndsInTheWorkArea);
    RUN_TEST_CASE(CharacterBuffers, LoadedSlotKeepsItsBuffer);
}
