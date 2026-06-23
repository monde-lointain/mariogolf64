/*
 * Request that the currently running RSP task yield. Setting SP_SET_YIELD asks
 * the microcode to save its state at the next yield point so a higher-priority
 * task (typically audio) can take the processor.
 */
#include "PR/os_internal.h"
#include "PR/rcp.h"

void osSpTaskYield(void) { __osSpSetStatus(SP_SET_YIELD); }
