/*
 * Faulted-thread accessor.
 *
 * Returns the thread the fault handler last recorded as having taken an
 * unhandled CPU exception, or NULL if none is pending. Used by debug/fault
 * tooling to report which thread crashed.
 */

#include "PR/os_internal.h"
#include "osint.h"

OSThread* __osGetCurrFaultedThread(void) { return __osFaultedThread; }
