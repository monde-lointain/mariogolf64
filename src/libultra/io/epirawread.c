/*
 * epirawread.c -- read one 32-bit word from an EPI (Enhanced PI) device.
 *
 * Programmed I/O (no DMA): used for small register-style reads from a cartridge
 * or peripheral on the PI bus.
 */
#include "piint.h"
#include "PR/ultraerror.h"
#include "assert.h"

s32 __osEPiRawReadIo(OSPiHandle* pihandle, u32 devAddr, u32* data) {
  register u32 stat;
  register u32 domain;

  /* A word read must be 4-byte aligned, and there must be somewhere to put it.
   */
#ifdef _DEBUG
  if (devAddr & 0x3) {
    __osError(ERR_OSPIRAWREADIO, 1, devAddr);
    return -1;
  }
  assert(data != NULL);
#endif

  /* Wait for the bus and select this device's timing before touching it. */
  EPI_SYNC(pihandle, stat, domain);
  *data = IO_READ(pihandle->baseAddress | devAddr);
  return 0;
}
