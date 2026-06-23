/*
 * pirawdma.c -- raw DMA between RDRAM and the cartridge (ROM) bus over the
 * Parallel Interface.
 *
 * Lowest-level PI DMA: it programs the PI registers and returns immediately
 * without waiting for completion. No locking, so the caller must already hold
 * PI access and ensure the PI is idle.
 */
#include "PRinternal/piint.h"
#include "PR/ultraerror.h"
#ident "$Revision: 1.17 $"

/* Start a PI DMA of `size` bytes in `direction` (OS_READ cart->RDRAM,
 * OS_WRITE RDRAM->cart) between cartridge offset devAddr and dramAddr.
 * Returns -1 on a bad argument (debug builds only) or unknown direction, 0
 * once the transfer is queued in hardware. devAddr is relative to osRomBase;
 * size must be even, 1..16 MB. */
s32 __osPiRawStartDma(s32 direction, u32 devAddr, void* dramAddr, u32 size) {
  register u32 stat;

  /* Argument validation is debug-only; the shipping path trusts the caller. */
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

  /* Wait out any in-progress PI access, then latch the DMA endpoints. The cart
   * address is converted from osRomBase + offset to a physical bus address. */
  WAIT_ON_IOBUSY(stat);
  IO_WRITE(PI_DRAM_ADDR_REG, osVirtualToPhysical(dramAddr));
  IO_WRITE(PI_CART_ADDR_REG, K1_TO_PHYS((u32)osRomBase | devAddr));

  /* Writing the length register starts the transfer; the register chosen
   * encodes the direction. Hardware programs length as size - 1. */
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
