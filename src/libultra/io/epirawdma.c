/*
 * epirawdma.c -- start a raw EPI (Enhanced PI) DMA by poking PI registers.
 *
 * The low-level transfer kick used by the PI manager: it programs the PI DMA
 * registers directly rather than queueing a request. The caller is responsible
 * for serializing access (no busy check here beyond EPI_SYNC).
 */
#include "piint.h"
#include "PR/ultraerror.h"
#ident "$Revision: 1.17 $"

s32 __osEPiRawStartDma(OSPiHandle* pihandle, s32 direction, u32 devAddr,
                       void* dramAddr, u32 size) {
  u32 stat;
  u32 domain;

  /* Same alignment/range rules as the queued path; checked only in debug. */
#ifdef _DEBUG
  if ((direction != OS_READ) && (direction != OS_WRITE)) {
    __osError(ERR_OSPIRAWSTARTDMA_DIR, 1, direction);
    return -1;
  }
  if (devAddr & 0x1) {
    __osError(ERR_OSPIRAWSTARTDMA_DEVADDR, 1, devAddr);
    return -1;
  }
  if ((u32)dramAddr & 0x7) {
    __osError(ERR_OSPIRAWSTARTDMA_ADDR, 1, dramAddr);
    return -1;
  }
  if (size & 0x1) {
    __osError(ERR_OSPIRAWSTARTDMA_SIZE, 1, size);
    return -1;
  }
  if ((size == 0) || (size > (16 * 1024 * 1024))) {
    __osError(ERR_OSPIRAWSTARTDMA_RANGE, 1, size);
    return -1;
  }
#endif

  /* Wait for the PI to go idle, then load this device's bus timing into the
   * domain registers if the previous transfer used a different device. */
  EPI_SYNC(pihandle, stat, domain);

  /* Program the two endpoints of the transfer (physical addresses). */
  IO_WRITE(PI_DRAM_ADDR_REG, osVirtualToPhysical(dramAddr));
  IO_WRITE(PI_CART_ADDR_REG, K1_TO_PHYS(pihandle->baseAddress | devAddr));

  /* Writing the length register (as size-1, the hardware's convention) starts
   * the DMA; the register chosen selects the direction. */
  switch (direction) {
    case OS_READ:
      IO_WRITE(PI_WR_LEN_REG, size - 1);
      break;
    case OS_WRITE:
      IO_WRITE(PI_RD_LEN_REG, size - 1);
      break;
    default:
      return -1;
  }
  return 0;
}
