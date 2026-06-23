/*
 * Report whether an RSP task that was asked to yield has actually finished
 * yielding (its state safely saved), so the scheduler knows if it can be
 * resumed later.
 */
#include "PR/os_internal.h"
#include "PR/sptask.h"
#include "PR/rcp.h"

/*
 * Returns OS_TASK_YIELDED if the task completed a yield (state saved), else 0.
 *
 * SP_STATUS_YIELDED means the save finished; SP_STATUS_YIELD means a yield is
 * in progress. When a yield is in flight, record the outcome into the task
 * flags and drop the DP-wait bit so the task can be re-loaded from its saved
 * state.
 */
OSYieldResult osSpTaskYielded(OSTask* tp) {
  u32 status;
  OSYieldResult result;
  status = __osSpGetStatus();
  result = (status & SP_STATUS_YIELDED) ? OS_TASK_YIELDED : 0;
  if (status & SP_STATUS_YIELD) {
    tp->t.flags |= result;
    tp->t.flags &= ~(OS_TASK_DP_WAIT);
  }
  return result;
}
