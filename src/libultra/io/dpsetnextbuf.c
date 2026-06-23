/*
 * dpsetnextbuf.c -- point the RDP (Display Processor) at its next command list.
 */
#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "PR/rcp.h"
#include "osint.h"

/* Queue a command buffer [bufPtr, bufPtr+size) for the RDP to fetch from RDRAM.
 * Returns 0 on success, -1 if the RDP is already busy (caller should retry) or,
 * in a debug build, if the buffer is not 8-byte aligned. */
s32 osDpSetNextBuffer(void* bufPtr, u64 size) {
  register u32 stat;

  /* The RDP requires both the start address and the length to be 8-byte
   * aligned. */
#ifdef _DEBUG
  if ((u32)bufPtr & 0x7) {
    __osError(ERR_OSDPSETNEXTBUFFER_ADDR, 1, bufPtr);
    return -1;
  }
  if (size & 0x7) {
    __osError(ERR_OSDPSETNEXTBUFFER_SIZE, 1, size);
    return -1;
  }
#endif

  /* Bail if a previous command list is still running; the registers below can
   * only be reloaded while the RDP is idle. */
  if (__osDpDeviceBusy()) {
    return -1;
  }

  /* Force fetch-from-RDRAM mode: clear the XBUS bit (which would otherwise read
   * the list out of the RSP's DMEM) and wait for the hardware to acknowledge.
   */
  IO_WRITE(DPC_STATUS_REG, DPC_CLR_XBUS_DMEM_DMA);
  while (TRUE) {
    stat = IO_READ(DPC_STATUS_REG);
    if ((stat & DPC_STATUS_XBUS_DMEM_DMA) == 0) {
      break;
    }
  }

  /* Program the command-list span (physical addresses); writing END kicks off
   * the fetch. */
  IO_WRITE(DPC_START_REG, osVirtualToPhysical(bufPtr));
  IO_WRITE(DPC_END_REG, osVirtualToPhysical(bufPtr) + size);
  return 0;
}
