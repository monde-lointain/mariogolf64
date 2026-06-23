/*
 * Thread start / resume.
 *
 * Makes a thread eligible to run. A WAITING thread simply moves to the run
 * queue; a freshly created STOPPED thread either joins the run queue or, if it
 * was created blocked on a wait queue, is parked there while one waiter is
 * promoted in its place. If the newly runnable thread outranks the caller, the
 * caller yields so it can be dispatched.
 */

#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "osint.h"

void osStartThread(OSThread* t) {
  register u32 saveMask = __osDisableInt();

  switch (t->state) {
    case OS_STATE_WAITING:
      // Already blocked somewhere: just make it runnable.
      t->state = OS_STATE_RUNNABLE;
      __osEnqueueThread(&__osRunQueue, t);
      break;

    case OS_STATE_STOPPED:
      // Newly created (or stopped). With no wait queue, or one that is the run
      // queue, it can run directly.
      if (t->queue == NULL || t->queue == &__osRunQueue) {
        t->state = OS_STATE_RUNNABLE;
        __osEnqueueThread(&__osRunQueue, t);
      } else {
        // Otherwise it was created already blocked on a real wait queue: park
        // it there and promote that queue's head waiter onto the run queue
        // instead.
        t->state = OS_STATE_WAITING;
        __osEnqueueThread(t->queue, t);
        __osEnqueueThread(&__osRunQueue, __osPopThread(t->queue));
      }
      break;

#ifdef _DEBUG
    default:
      __osError(ERR_OSSTARTTHREAD, 0);
      __osRestoreInt(saveMask);
      return 0;
#endif
  }

  // Dispatch if nothing was running yet; otherwise preempt the caller only when
  // the run queue now holds a higher-priority thread.
  if (__osRunningThread == NULL) {
    __osDispatchThread();
  } else if (__osRunningThread->priority < __osRunQueue->priority) {
    __osRunningThread->state = OS_STATE_RUNNABLE;
    __osEnqueueAndYield(&__osRunQueue);
  }

  __osRestoreInt(saveMask);
}
