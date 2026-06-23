/*
 * Kick off a raw DMA between RDRAM and the RSP's DMEM/IMEM. The "raw" form does
 * no queuing: it programs the SP DMA registers directly and returns
 * immediately.
 */
#include "PR/os_internal.h"
#include "PR/rcp.h"
#include "osint.h"
#include "assert.h"

#ident "$Revision: 1.17 $"

/*
 * Start an SP DMA of size bytes between devAddr (DMEM/IMEM) and dramAddr.
 * Returns -1 without starting if the SP is busy, else 0. All three of devAddr,
 * dramAddr, and size must be 8-byte aligned.
 *
 * Note the register names are from the RSP's point of view, so an OS_READ
 * (RDRAM -> SP) is programmed via SP_WR_LEN_REG and a write via SP_RD_LEN_REG.
 * Lengths use the hardware's "bytes minus one" convention.
 */
s32 __osSpRawStartDma(s32 direction, u32 devAddr, void* dramAddr, u32 size) {
#ifdef _DEBUG
  assert(((u32)devAddr & 0x7) == 0);
  assert(((u32)dramAddr & 0x7) == 0);
  assert(((u32)size & 0x7) == 0);
#endif
  if (__osSpDeviceBusy()) {
    return -1;
  }
  IO_WRITE(SP_MEM_ADDR_REG, devAddr);
  IO_WRITE(SP_DRAM_ADDR_REG, osVirtualToPhysical(dramAddr));
  if (direction == OS_READ) {
    IO_WRITE(SP_WR_LEN_REG, size - 1);
  } else {
    IO_WRITE(SP_RD_LEN_REG, size - 1);
  }
  return 0;
}
