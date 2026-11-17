/*
 * Tests port/cmake/OverlaySections.cmake: that the linker script it writes
 * gathers each overlay's variables between the bounds that
 * GetOverlaySections gives, on the platform's own linker and loader. The
 * program has two overlays (see overlay_sections/): A with variables of
 * every kind, and B with none, which some linkers would drop a section for.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "overlay_sections/overlays.h"
#include "port/overlay_sections.h"
#include "test_support.h"

enum {
    SEGMENT_A = 1,
    SEGMENT_B = 2,
};

// A variable of the program's own, outside every overlay
static int f_MainValue = 3;

static const OverlaySections *FindSections(unsigned int segmentNumber) {
    int count;
    const OverlaySections *sections = GetOverlaySections(&count);
    int i;

    for (i = 0; i < count; i++) {
        if (sections[i].segmentNumber == segmentNumber) {
            return &sections[i];
        }
    }
    return NULL;
}

static bool IsWithin(
    const void *address,
    size_t size,
    const unsigned char *start,
    const unsigned char *end
) {
    uintptr_t first = (uintptr_t)address;

    return first >= (uintptr_t)start && first + size <= (uintptr_t)end;
}

static void TestListsEachOverlay(void) {
    int count;

    EXPECT(GetOverlaySections(&count) != NULL);
    EXPECT(count == 2);
    EXPECT(FindSections(SEGMENT_A) != NULL);
    EXPECT(FindSections(SEGMENT_B) != NULL);
}

static void TestBoundsOverlayVariables(void) {
    const OverlaySections *a = FindSections(SEGMENT_A);

    if (a == NULL) {
        return;
    }
    EXPECT(IsWithin(GetOverlayAValue(), sizeof(int), a->dataStart, a->dataEnd));
    EXPECT(IsWithin(
        GetOverlayAPointers(),
        SECTIONS_A_POINTER_COUNT * sizeof(int *),
        a->dataStart,
        a->dataEnd
    ));
    EXPECT(IsWithin(GetOverlayAZero(), sizeof(int), a->bssStart, a->bssEnd));
}

static void TestLeavesOtherVariablesOut(void) {
    const OverlaySections *a = FindSections(SEGMENT_A);

    if (a == NULL) {
        return;
    }
    EXPECT(!IsWithin(&f_MainValue, sizeof(f_MainValue), a->dataStart, a->dataEnd));
    EXPECT(!IsWithin(&f_MainValue, sizeof(f_MainValue), a->bssStart, a->bssEnd));
}

static void TestGivesEmptyBoundsWithoutVariables(void) {
    const OverlaySections *b = FindSections(SEGMENT_B);

    EXPECT(AddInOverlayB(2, 3) == 5);
    if (b == NULL) {
        return;
    }
    EXPECT(b->dataStart == b->dataEnd);
    EXPECT(b->bssStart == b->bssEnd);
}

// Where the loader maps the overlays' variables, they hold what the
// program was linked with, pointers fixed up included
static void TestLoadsInitialValues(void) {
    int **pointers = GetOverlayAPointers();
    int i;

    EXPECT(*GetOverlayAValue() == 7);
    EXPECT(*GetOverlayAZero() == 0);
    EXPECT(f_MainValue == 3);
    for (i = 0; i < SECTIONS_A_POINTER_COUNT; i++) {
        EXPECT(pointers[i] == GetOverlayAValue());
    }
}

int main(void) {
    TestListsEachOverlay();
    TestBoundsOverlayVariables();
    TestLeavesOtherVariablesOut();
    TestGivesEmptyBoundsWithoutVariables();
    TestLoadsInitialValues();
    return ReportTestResults("overlay_sections_test");
}
