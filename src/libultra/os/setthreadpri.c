/*
 * Thread priority change.
 *
 * Updates a thread's scheduling priority and re-sorts it into its queue at the
 * new priority. A NULL argument targets the running thread. If the change makes
 * some other thread higher-priority than the caller, the caller yields so the
 * scheduler can switch to it. Runs under interrupt lock.
 */

#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "osint.h"

void osSetThreadPri(OSThread* t, OSPri pri) {
  register u32 saveMask;

#ifdef _DEBUG
  if ((pri < OS_PRIORITY_IDLE) || (pri > OS_PRIORITY_MAX)) {
    __osError(ERR_OSSETTHREADPRI, 1, pri);
    return 0;
  }
#endif

  saveMask = __osDisableInt();
  if (t == NULL) {
    t = __osRunningThread;
  }

  if (t->priority != pri) {
    t->priority = pri;

    // The queues are priority-ordered, so a queued (not running, not stopped)
    // thread must be pulled out and reinserted at its new sort position.
    if (t != __osRunningThread && t->state != OS_STATE_STOPPED) {
      __osDequeueThread(t->queue, t);
      __osEnqueueThread(t->queue, t);
    }

    // If lowering our own priority (or raising another's) left a runnable
    // thread ranked above us, yield so the scheduler can preempt to it.
    if (__osRunningThread->priority < __osRunQueue->priority) {
      __osRunningThread->state = OS_STATE_RUNNABLE;
      __osEnqueueAndYield(&__osRunQueue);
    }
  }

  __osRestoreInt(saveMask);
}
