/*
 * What overlay_sections_test's overlays hold, which the test reaches
 * through these functions, as their variables are static.
 */

#ifndef PORT_TEST_OVERLAYS_H
#define PORT_TEST_OVERLAYS_H

#include <stddef.h>

#define SECTIONS_A_POINTER_COUNT 64

// Overlay A's variables: one with an initial value, one cleared, and a
// table of pointers to the first, which a loader that relocates the
// program must fix up
int *GetOverlayAValue(void);
int *GetOverlayAZero(void);
int *GetOverlayAOtherZero(void);
int **GetOverlayAPointers(void);

// Overlay B has no variables, only this
int AddInOverlayB(int a, int b);

// Overlay C's variables: one with an initial value and one cleared
int *GetOverlayCValue(void);
int *GetOverlayCZero(void);

#endif
