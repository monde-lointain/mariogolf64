/*
 * piacs.c -- Parallel Interface (PI) mutual exclusion.
 *
 * The PI moves data between RDRAM and the cartridge/peripheral bus and serves
 * one DMA at a time, so access is serialized through a one-slot message queue
 * used as a binary semaphore: holding the lone message means owning the PI.
 * Get blocks until it is free; Rel returns it.
 */
#include "common.h"
#include "macros.h"
#include "PR/os_internal.h"

#define PI_Q_BUF_LEN 1

extern u32 __osPiAccessQueueEnabled;
extern OSMesgQueue __osPiAccessQueue;
extern OSMesg piAccessBuf[];

/* Build the access queue and prime it with the single token that represents an
 * unowned PI. Called once, on the first __osPiGetAccess. */
void __osPiCreateAccessQueue(void) {
  __osPiAccessQueueEnabled = 1;
  osCreateMesgQueue(&__osPiAccessQueue, piAccessBuf, PI_Q_BUF_LEN);
  osSendMesg(&__osPiAccessQueue, NULL, OS_MESG_NOBLOCK);
}

/* Acquire exclusive PI access, blocking until the token is available. */
void __osPiGetAccess(void) {
  OSMesg dummyMesg;
  if (!__osPiAccessQueueEnabled) {
    __osPiCreateAccessQueue();
  }
  osRecvMesg(&__osPiAccessQueue, &dummyMesg, OS_MESG_BLOCK);
}

/* Release PI access by returning the token for the next waiter. */
void __osPiRelAccess(void) {
  osSendMesg(&__osPiAccessQueue, NULL, OS_MESG_NOBLOCK);
}
