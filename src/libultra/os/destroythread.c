/*
 * Thread teardown.
 *
 * Removes a thread from the scheduler's run/wait queues and from the global
 * active-thread list, then reclaims the CPU if the thread destroyed itself. A
 * NULL argument means "destroy the running thread", which never returns.
 */

#include "PR/os_internal.h"
#include "osint.h"

void osDestroyThread(OSThread* t) {
  register u32 saveMask;
  register OSThread* pred;
  register OSThread* succ;

  saveMask = __osDisableInt();

  // Resolve the target, and pull it off whatever wait/run queue it is on. A
  // STOPPED thread is already unlinked, so it needs no dequeue.
  if (t == NULL) {
    t = __osRunningThread;
  } else if (t->state != OS_STATE_STOPPED) {
    __osDequeueThread(t->queue, t);
  }

  // Unlink from the global active-thread list (singly linked by `tlnext`). The
  // list ends at a sentinel whose priority is -1, which bounds the walk.
  if (__osActiveQueue == t) {
    __osActiveQueue = __osActiveQueue->tlnext;
  } else {
    pred = __osActiveQueue;
    while (pred->priority != -1) {
      succ = pred->tlnext;
      if (succ == t) {
        pred->tlnext = t->tlnext;
        break;
      }
      pred = succ;
    }
  }

  // If a thread destroyed itself, there is nothing left to run on; dispatch the
  // next runnable thread, which does not return to here.
  if (t == __osRunningThread) {
    __osDispatchThread();
  }

  __osRestoreInt(saveMask);
}
