/*
 * dp.c -- query whether the RDP (Display Processor) is mid-DMA.
 */
#include "PR/os_internal.h"
#include "PR/rcp.h"

/* TRUE while the RDP is busy transferring a command list, so a caller can hold
 * off handing it the next buffer. */
int __osDpDeviceBusy(void) {
  register u32 stat = IO_READ(DPC_STATUS_REG);
  if (stat & DPC_STATUS_DMA_BUSY) {
    return TRUE;
  }
  return FALSE;
}
