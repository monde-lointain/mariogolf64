/*
 * Thread stop.
 *
 * Marks a thread STOPPED so the scheduler will not run it until a later
 * osStartThread. A NULL argument stops the caller itself, which then yields the
 * CPU and does not return until restarted. A queued or waiting thread is simply
 * unlinked in place.
 */

#include "PR/os_internal.h"
#include "osint.h"

void osStopThread(OSThread* t) {
  register u32 saveMask = __osDisableInt();
  register u16 state;

  // NULL means "stop me"; the running thread's state is implicit, so synthesize
  // RUNNING here to drive the self-stop path below.
  state = (t != NULL) ? t->state : OS_STATE_RUNNING;
  switch (state) {
    case OS_STATE_RUNNING:
      // Stopping ourselves: give up the CPU with no requeue, so the scheduler
      // moves on and does not return here until we are restarted.
      __osRunningThread->state = OS_STATE_STOPPED;
      __osEnqueueAndYield(NULL);
      break;

    case OS_STATE_RUNNABLE:
    case OS_STATE_WAITING:
      // Another thread, already queued: mark it stopped and pull it off its
      // queue so it is skipped over.
      t->state = OS_STATE_STOPPED;
      __osDequeueThread(t->queue, t);
      break;
  }

  __osRestoreInt(saveMask);
}
