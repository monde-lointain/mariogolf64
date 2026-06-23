/*
 * pigetcmdq.c -- accessor for the PI manager's command queue.
 *
 * Clients post DMA requests to this queue, but only once osCreatePiManager has
 * brought the manager up.
 */
#include "PR/os_internal.h"
#include "PRinternal/piint.h"

/* Return the PI device manager's command queue, or NULL if the manager has not
 * been started yet (no queue to post to). */
OSMesgQueue* osPiGetCmdQueue(void) {
  if (!__osPiDevMgr.active) {
    return NULL;
  }
  return __osPiDevMgr.cmdQueue;
}
