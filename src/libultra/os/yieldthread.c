/*
 * Voluntary CPU yield.
 *
 * The running thread gives up the CPU without blocking: it goes back on the run
 * queue as RUNNABLE and the scheduler picks the highest-priority thread (which
 * may be this same thread again). Runs under interrupt lock so the requeue and
 * context switch are atomic.
 */

#include "PR/os_internal.h"
#include "osint.h"

void osYieldThread(void) {
  register u32 saveMask = __osDisableInt();
  __osRunningThread->state = OS_STATE_RUNNABLE;
  __osEnqueueAndYield(&__osRunQueue);
  __osRestoreInt(saveMask);
}
