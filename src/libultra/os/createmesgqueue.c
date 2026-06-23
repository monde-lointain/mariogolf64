/*
 * Message-queue initialization.
 *
 * Sets up a caller-allocated OSMesgQueue around a caller-supplied message
 * buffer. After this call the queue is empty and ready for osSendMesg /
 * osRecvMesg traffic.
 */

#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "osint.h"

void osCreateMesgQueue(OSMesgQueue* mq, OSMesg* msg, s32 msgCount) {
#ifdef _DEBUG
  if (msgCount <= 0) {
    __osError(ERR_OSCREATEMESGQUEUE, 1, msgCount);
    return 0;
  }
#endif

  // Both the message-waiting and full-waiting thread lists start empty, each
  // anchored at the shared thread-tail sentinel.
  mq->mtqueue = (OSThread*)&__osThreadTail.next;
  mq->fullqueue = (OSThread*)&__osThreadTail.next;

  // No messages buffered yet; index and capacity describe the ring buffer.
  mq->validCount = 0;
  mq->first = 0;
  mq->msgCount = msgCount;
  mq->msg = msg;
}
