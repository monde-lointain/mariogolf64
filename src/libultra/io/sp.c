/*
 * sp.c -- predicate for whether the RSP (signal processor) is busy with a DMA
 * or has a pending register I/O, used to gate the SP task-load DMAs.
 */
#include "PR/os_internal.h"
#include "PR/rcp.h"

/* True while an SP DMA is in flight or its address/length FIFO still has work
 * queued; callers spin on this before starting a new SP transfer. */
int __osSpDeviceBusy(void) {
  register u32 stat = IO_READ(SP_STATUS_REG);
  if (stat & (SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL | SP_STATUS_IO_FULL)) {
    return TRUE;
  }
  return FALSE;
}
