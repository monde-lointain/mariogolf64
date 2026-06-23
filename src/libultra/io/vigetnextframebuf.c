/*
 * Read back the frame buffer queued for the next display.
 *
 * Returns the address most recently handed to osViSwapBuffer, i.e. the buffer
 * the VI manager will latch at the next vertical retrace.
 */

#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "viint.h"

void* osViGetNextFramebuffer(void) {
  register u32 saveMask;
  void* framep;

#ifdef _DEBUG
  if (!__osViDevMgr.active) {
    __osError(ERR_OSVIGETNEXTFRAMEBUFFER, 0);
    return NULL;
  }
#endif

  // The VI manager thread mutates __osViNext at retrace, so read it with
  // interrupts masked to get a coherent pointer.
  saveMask = __osDisableInt();
  framep = __osViNext->framep;
  __osRestoreInt(saveMask);
  return framep;
}
