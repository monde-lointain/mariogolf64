/*
 * Message send.
 *
 * Appends a message to the tail of a queue's ring buffer, the normal-priority
 * counterpart to osJamMesg. If the queue is full the caller either blocks until
 * a slot frees up or returns immediately, depending on `flags`.
 */

#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "osint.h"

s32 osSendMesg(OSMesgQueue* mq, OSMesg msg, s32 flags) {
  register u32 saveMask;
  register s32 last;

#ifdef _DEBUG
  if ((flags != OS_MESG_NOBLOCK) && (flags != OS_MESG_BLOCK)) {
    __osError(ERR_OSSENDMESG, 1, flags);
    return -1;
  }
#endif

  saveMask = __osDisableInt();

  // Queue full: block this thread on the "slot freed" list, or bail out for a
  // non-blocking caller. Loop because a wakeup does not guarantee a free slot.
  while (MQ_IS_FULL(mq)) {
    if (flags == OS_MESG_BLOCK) {
      __osRunningThread->state = OS_STATE_WAITING;
      __osEnqueueAndYield(&mq->fullqueue);
    } else {
      __osRestoreInt(saveMask);
      return -1;
    }
  }

  // Store at the tail slot (head + count, modulo capacity).
  last = (mq->first + mq->validCount) % mq->msgCount;
  mq->msg[last] = msg;
  mq->validCount++;

  // Wake a thread that was blocked waiting for a message to arrive.
  if (mq->mtqueue->next != NULL) {
    osStartThread(__osPopThread(&mq->mtqueue));
  }

  __osRestoreInt(saveMask);
  return 0;
}
