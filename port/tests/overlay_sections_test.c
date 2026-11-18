/*
 * Tests port/cmake/OverlaySections.cmake: that the linker script it writes
 * gathers each overlay's variables between the bounds that
 * GetOverlaySections gives, on the platform's own linker and loader. The
 * program has three overlays (see overlay_sections/): A with variables of
 * every kind, B with none, which some linkers would drop a section for, and
 * C, with variables that follow A's in the snapshot.
 * And that port/src/os/memory.c's snapshot puts an overlay's variables back
 * as they were linked, which it does by copying their sections whole.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <ultra64.h>

#include "os_internal.h"
#include "overlay_sections/overlays.h"
#include "port/os_host.h"
#include "port/overlay_sections.h"
#include "test_support.h"

enum {
    SEGMENT_A = 1,
    SEGMENT_B = 2,
    SEGMENT_C = 3,
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
    EXPECT(count == 3);
    EXPECT(FindSections(SEGMENT_A) != NULL);
    EXPECT(FindSections(SEGMENT_B) != NULL);
    EXPECT(FindSections(SEGMENT_C) != NULL);
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
    EXPECT(IsWithin(GetOverlayAOtherZero(), sizeof(int), a->bssStart, a->bssEnd));
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

// The snapshot can't be taken here: the test's only way out
void PcFatalError(const char *format, ...) {
    (void)format;
    EXPECT(false);
    exit(ReportTestResults("overlay_sections_test"));
}

// Resetting an overlay's segment gives back its variables' linked values,
// and leaves the program's own alone. Its copies span the whole of each
// section, the padding that a sanitizer puts between variables included.
static void TestResetsOverlayVariables(void) {
    int **pointers = GetOverlayAPointers();
    int i;

    PcInitSegments();
    *GetOverlayAValue() = 70;
    *GetOverlayAZero() = 5;
    *GetOverlayAOtherZero() = 6;
    pointers[0] = NULL;
    f_MainValue = 30;

    osPcResetSegment(SEGMENT_B);
    EXPECT(*GetOverlayAValue() == 70);
    osPcResetSegment(SEGMENT_A);
    EXPECT(*GetOverlayAValue() == 7);
    EXPECT(*GetOverlayAZero() == 0);
    EXPECT(*GetOverlayAOtherZero() == 0);
    for (i = 0; i < SECTIONS_A_POINTER_COUNT; i++) {
        EXPECT(pointers[i] == GetOverlayAValue());
    }
    EXPECT(f_MainValue == 30);
}

// Resetting the overlay after A finds its own variables in the snapshot,
// past A's
static void TestResetsLaterOverlay(void) {
    *GetOverlayCValue() = 110;
    *GetOverlayCZero() = 12;
    *GetOverlayAValue() = 71;

    osPcResetSegment(SEGMENT_C);
    EXPECT(*GetOverlayCValue() == 11);
    EXPECT(*GetOverlayCZero() == 0);
    EXPECT(*GetOverlayAValue() == 71);
}

int main(void) {
    TestListsEachOverlay();
    TestBoundsOverlayVariables();
    TestLeavesOtherVariablesOut();
    TestGivesEmptyBoundsWithoutVariables();
    TestLoadsInitialValues();
    TestResetsOverlayVariables();
    TestResetsLaterOverlay();
    return ReportTestResults("overlay_sections_test");
}
