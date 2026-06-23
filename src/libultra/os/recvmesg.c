/*
 * Message receive.
 *
 * Pulls the oldest message off a queue's ring buffer. If the queue is empty the
 * caller either returns immediately or blocks until a message arrives,
 * depending on `flags`. A NULL `msg` discards the message but still consumes
 * it.
 */

#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "osint.h"

s32 osRecvMesg(OSMesgQueue* mq, OSMesg* msg, s32 flags) {
  register u32 saveMask;

#ifdef _DEBUG
  if ((flags != OS_MESG_NOBLOCK) && (flags != OS_MESG_BLOCK)) {
    __osError(ERR_OSRECVMESG, 1, flags);
    return -1;
  }
#endif

  saveMask = __osDisableInt();

  // Queue empty: bail out for a non-blocking caller, otherwise block on the
  // "message arrived" list. Loop because a wakeup does not guarantee a message.
  while (MQ_IS_EMPTY(mq)) {
    if (flags == OS_MESG_NOBLOCK) {
      __osRestoreInt(saveMask);
      return -1;
    } else {
      __osRunningThread->state = OS_STATE_WAITING;
      __osEnqueueAndYield(&mq->mtqueue);
    }
  }

  // Hand back the oldest message and advance the head index modulo capacity.
  if (msg != NULL) {
    *msg = mq->msg[mq->first];
  }
  mq->first = (mq->first + 1) % mq->msgCount;
  mq->validCount--;

  // A slot just freed up; wake a thread that was blocked on a full queue.
  if (mq->fullqueue->next != NULL) {
    osStartThread(__osPopThread(&mq->fullqueue));
  }

  __osRestoreInt(saveMask);
  return 0;
}
