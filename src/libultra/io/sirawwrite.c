/*
 * sirawwrite.c -- low-level write of a single word to a Serial Interface device
 * register (the SI bus reaches the controllers and their PIF). The caller is
 * responsible for the surrounding read/write protocol; this is the raw register
 * poke.
 */
#include "PR/os_internal.h"
#include "siint.h"
#include "assert.h"

/* Write data to the SI device register at devAddr. Fails fast rather than
 * blocking: returns -1 if the SI is mid-transfer, 0 once the write is issued.
 * devAddr must be word-aligned. */
s32 __osSiRawWriteIo(u32 devAddr, u32 data) {
#ifdef _DEBUG
  assert((devAddr & 0x3) == 0);
#endif
  if (__osSiDeviceBusy()) {
    return -1;
  }
  IO_WRITE(devAddr, data);
  return 0;
}
