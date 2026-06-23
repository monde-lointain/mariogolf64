/*
 * Global Video Interface state: the double-buffered VI context pair the VI
 * manager and the os*Vi* calls share.
 *
 * vi[0]/vi[1] are the two contexts; __osViCurr points at the one currently
 * driving the display, __osViNext at the one being edited. The two are swapped
 * at each retrace, so changes are staged into "next" and take effect on the
 * following field.
 */
#include "PRinternal/macros.h"
#include "PR/os_internal.h"
#include "PR/rcp.h"
#include "viint.h"

__OSViContext vi[2] ALIGNED(0x8) = {0};
__OSViContext* __osViCurr = &vi[0];
__OSViContext* __osViNext = &vi[1];
