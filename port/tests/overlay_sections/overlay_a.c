/*
 * overlay_sections_test's overlay with variables of every kind.
 */

#include "overlays.h"

static int f_Value = 7;
static int f_Zero;
static int *f_Pointers[SECTIONS_A_POINTER_COUNT] = {
    [0 ... SECTIONS_A_POINTER_COUNT - 1] = &f_Value,
};

int *GetOverlayAValue(void) {
    return &f_Value;
}

int *GetOverlayAZero(void) {
    return &f_Zero;
}

int **GetOverlayAPointers(void) {
    return f_Pointers;
}
