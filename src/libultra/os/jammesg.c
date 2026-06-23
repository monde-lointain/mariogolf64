/*
 * Priority message send (jam-to-front).
 *
 * Inserts a message at the *head* of a queue's ring buffer so it is received
 * before any already-queued messages, the high-priority counterpart to
 * osSendMesg. If the queue is full the caller either blocks until space frees
 * up or returns immediately, depending on `flag`.
 */

#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "osint.h"

s32 osJamMesg(OSMesgQueue* mq, OSMesg msg, s32 flag) {
  register u32 saveMask;

#ifdef _DEBUG
  if ((flag != OS_MESG_NOBLOCK) && (flag != OS_MESG_BLOCK)) {
    __osError(ERR_OSJAMMESG, 1, flag);
    return -1;
  }
#endif

  saveMask = __osDisableInt();

  // Queue full: block this thread on the "message taken" list, or bail out for
  // a non-blocking caller. Loop because a wakeup does not guarantee free space.
  while (mq->validCount >= mq->msgCount) {
    if (flag == OS_MESG_BLOCK) {
      __osRunningThread->state = OS_STATE_WAITING;
      __osEnqueueAndYield(&mq->fullqueue);
    } else {
      __osRestoreInt(saveMask);
      return -1;
    }
  }

  // Step the head index backwards (modulo capacity) so the new message lands
  // ahead of the current oldest entry.
  mq->first = (mq->first + mq->msgCount - 1) % mq->msgCount;
  mq->msg[mq->first] = msg;
  mq->validCount++;

  // Wake a thread that was blocked waiting for a message to arrive.
  if (mq->mtqueue->next != NULL) {
    osStartThread(__osPopThread(&mq->mtqueue));
  }

  __osRestoreInt(saveMask);
  return 0;
}
