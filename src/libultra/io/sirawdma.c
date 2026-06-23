/*
 * sirawdma.c -- raw 64-byte DMA between RDRAM and PIF RAM over the Serial
 * Interface.
 *
 * Lowest-level SI DMA: it kicks off the transfer of the whole PIF RAM block but
 * does not wait for completion (the caller blocks on the SI interrupt). No
 * locking, so the caller must already hold SI access.
 */
#include "PR/os_internal.h"
#include "assert.h"
#include "siint.h"

#define PIF_RAM_SIZE (PIF_RAM_END + 1 - PIF_RAM_START)

/* Start a PIF-RAM DMA in `direction` (OS_READ from PIF, OS_WRITE to PIF) using
 * the RDRAM buffer dramAddr. Returns -1 if the SI is still busy, else 0.
 * dramAddr must be word-aligned. */
s32 __osSiRawStartDma(s32 direction, void* dramAddr) {
#ifdef _DEBUG
  assert(((u32)dramAddr & 0x3) == 0);
#endif

#if BUILD_VERSION >= VERSION_J
  if (IO_READ(SI_STATUS_REG) & (SI_STATUS_DMA_BUSY | SI_STATUS_RD_BUSY)) {
    return -1;
  }
#else
  if (__osSiDeviceBusy()) {
    return -1;
  }
#endif

  /* On a write, flush the source buffer so the SI sees current data; on a read,
   * the cache is invalidated afterward so the CPU re-fetches the DMA result. */
  if (direction == OS_WRITE) {
    osWritebackDCache(dramAddr, PIF_RAM_SIZE);
  }
  IO_WRITE(SI_DRAM_ADDR_REG, osVirtualToPhysical(dramAddr));
  if (direction == OS_READ) {
    IO_WRITE(SI_PIF_ADDR_RD64B_REG, PIF_RAM_START);
  } else {
    IO_WRITE(SI_PIF_ADDR_WR64B_REG, PIF_RAM_START);
  }
  if (direction == OS_READ) {
    osInvalDCache(dramAddr, PIF_RAM_SIZE);
  }
  return 0;
}
