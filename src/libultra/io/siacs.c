/*
 * siacs.c -- Serial Interface (SI) mutual exclusion.
 *
 * The PIF/controller hardware behind the SI serves one transaction at a time,
 * so access is serialized through a one-slot message queue used as a binary
 * semaphore: holding the lone message means owning the SI. Get blocks until
 * the message is free; Rel returns it.
 */
#include "common.h"
#include "macros.h"
#include "PR/os_internal.h"

#define SI_Q_BUF_LEN 1

/* Set the first time the lock is taken, so the queue is created lazily. */
u32 __osSiAccessQueueEnabled = 0;
extern OSMesgQueue __osSiAccessQueue;
extern OSMesg siAccessBuf[];

/* Build the access queue and prime it with the single token that represents an
 * unowned SI. Called once, on the first __osSiGetAccess. */
void __osSiCreateAccessQueue(void) {
  __osSiAccessQueueEnabled = 1;
  osCreateMesgQueue(&__osSiAccessQueue, siAccessBuf, SI_Q_BUF_LEN);
  osSendMesg(&__osSiAccessQueue, NULL, OS_MESG_NOBLOCK);
}

/* Acquire exclusive SI access, blocking until the token is available. */
void __osSiGetAccess(void) {
  OSMesg dummyMesg;
  if (!__osSiAccessQueueEnabled) {
    __osSiCreateAccessQueue();
  }
  osRecvMesg(&__osSiAccessQueue, &dummyMesg, OS_MESG_BLOCK);
}

/* Release SI access by returning the token for the next waiter. */
void __osSiRelAccess(void) {
  osSendMesg(&__osSiAccessQueue, NULL, OS_MESG_NOBLOCK);
}
