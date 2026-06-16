#include "PRinternal/macros.h"
#include "PR/os_internal.h"
#include "PR/rcp.h"
#include "viint.h"

__OSViContext vi[2] ALIGNED(0x8) = { 0 };
__OSViContext* __osViCurr = &vi[0];
__OSViContext* __osViNext = &vi[1];
