#include "PR/os_internal.h"
#include "internal/viint.h"

__OSViContext *__osViGetCurrentContext(void) { return __osViCurr; }
