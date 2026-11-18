/*
 * overlay_sections_test's overlay after A, which has variables of its own,
 * to find in the snapshot past A's.
 */

#include "overlays.h"

static int f_Value = 11;
static int f_Zero;

int *GetOverlayCValue(void) {
    return &f_Value;
}

int *GetOverlayCZero(void) {
    return &f_Zero;
}
