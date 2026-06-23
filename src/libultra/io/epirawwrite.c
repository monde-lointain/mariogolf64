/*
 * epirawwrite.c -- write one 32-bit word to an EPI (Enhanced PI) device.
 *
 * Programmed I/O (no DMA): used for small register-style writes to a cartridge
 * or peripheral on the PI bus.
 */
#include "piint.h"
#include "PR/ultraerror.h"

s32 __osEPiRawWriteIo(OSPiHandle* pihandle, u32 devAddr, u32 data) {
  register u32 stat;
  register u32 domain;

  /* A word write must be 4-byte aligned. */
#ifdef _DEBUG
  if (devAddr & 0x3) {
    __osError(ERR_OSPIRAWWRITEIO, 1, devAddr);
    return -1;
  }
#endif

  /* Wait for the bus and select this device's timing before touching it. */
  EPI_SYNC(pihandle, stat, domain);
  IO_WRITE(pihandle->baseAddress | devAddr, data);
  return 0;
}
