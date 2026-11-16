/*
 * A link seam's double: the program's table of overlays, which libultra's
 * memory reads and the tests don't have.
 */

#include <stddef.h>

#include "port/overlay_sections.h"

const OverlaySections *GetOverlaySections(int *count) {
    *count = 0;
    return NULL;
}
