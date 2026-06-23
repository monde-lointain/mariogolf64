/*
 * sirawread.c -- raw single-word read from the Serial Interface device.
 *
 * Lowest-level SI read: no queueing or locking, the caller is responsible for
 * holding SI access and for the device being idle.
 */
#include "PR/os_internal.h"
#include "assert.h"
#include "siint.h"

/* Read one 32-bit word from SI device register devAddr into *data. Returns -1
 * if the SI is still busy; 0 on success. devAddr must be word-aligned. */
s32 __osSiRawReadIo(u32 devAddr, u32* data) {
#ifdef _DEBUG
  assert((devAddr & 0x3) == 0);
  assert(data != NULL);
#endif
  if (__osSiDeviceBusy()) {
    return -1;
  }
  *data = IO_READ(devAddr);
  return 0;
}
