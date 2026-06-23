/*
 * Register the retrace-notification message for the VI manager.
 *
 * Tells the VI manager which message queue to post to (and how often) on
 * vertical retrace, so the application can synchronize to the display refresh.
 */

#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "assert.h"
#include "viint.h"

/* mq/m: the queue and message posted on retrace. retraceCount divides the rate:
 * 1 posts every retrace (~60Hz), 2 every other (~30Hz), and so on. */
void osViSetEvent(OSMesgQueue* mq, OSMesg m, u32 retraceCount) {
  register u32 saveMask;

#ifdef _DEBUG
  if (!__osViDevMgr.active) {
    __osError(ERR_OSVISETEVENT, 0);
    return 0;
  }
  assert(mq != NULL);
#endif

  // Stage into the pending context under interrupt mask, since the VI manager
  // thread reads these fields at retrace.
  saveMask = __osDisableInt();
  __osViNext->msgq = mq;
  __osViNext->msg = m;
  __osViNext->retraceCount = retraceCount;
  __osRestoreInt(saveMask);
}
