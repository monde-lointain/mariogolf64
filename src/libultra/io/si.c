/*
 * si.c -- Serial Interface (SI) busy probe.
 *
 * The SI channel drives the PIF, which services the controller ports. Callers
 * must not start a new SI DMA while a transfer is still in flight, so this is
 * the gate they check first.
 */
#include "PR/os_internal.h"
#include "siint.h"

/* Report whether the SI is mid-transfer: TRUE if either a DMA or an I/O read is
 * still outstanding, FALSE if the channel is idle and ready for a new request.
 */
int __osSiDeviceBusy() {
  register u32 stat = IO_READ(SI_STATUS_REG);
  if (stat & (SI_STATUS_DMA_BUSY | SI_STATUS_RD_BUSY)) {
    return TRUE;
  }
  return FALSE;
}
