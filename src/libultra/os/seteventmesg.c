/*
 * Event-to-message-queue binding.
 *
 * Registers the queue and message the kernel should post when a given system
 * event fires (interrupt sources, faults, the pre-NMI signal, etc.), letting a
 * thread wait on hardware/OS events through the normal message machinery.
 */

#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "osint.h"

// True once a pre-NMI has already been observed, so its notification is sent at
// most once.
u32 __osPreNMI = FALSE;

void osSetEventMesg(OSEvent event, OSMesgQueue* mq, OSMesg msg) {
  register u32 saveMask;
  __OSEventState* es;

#ifdef _DEBUG
  if (event >= OS_NUM_EVENTS) {
    __osError(ERR_OSSETEVENTMESG, 1, event);
    return;
  }
#endif

  saveMask = __osDisableInt();

  // Record the binding in the per-event table the interrupt path consults.
  es = &__osEventStateTab[event];
  es->messageQueue = mq;
  es->message = msg;

  // Pre-NMI is special: a reset may already be pending when a thread registers
  // late. If a shutdown is in flight and we have not delivered yet, fire the
  // notification immediately so the thread cannot miss the reset warning.
  if (event == OS_EVENT_PRENMI) {
    if (__osShutdown && !__osPreNMI) {
      osSendMesg(mq, msg, OS_MESG_NOBLOCK);
    }
    __osPreNMI = TRUE;
  }

  __osRestoreInt(saveMask);
}
