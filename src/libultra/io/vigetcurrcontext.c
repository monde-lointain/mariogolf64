/*
 * Return the VI context currently driving the display (__osViCurr), for the VI
 * manager's internal use.
 */
#include "PR/os_internal.h"
#include "viint.h"

__OSViContext* __osViGetCurrentContext(void) { return __osViCurr; }
