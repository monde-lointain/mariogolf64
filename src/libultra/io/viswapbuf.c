/*
 * Queue a frame buffer to be displayed at the next vertical retrace.
 *
 * Records the buffer in the pending VI context; the VI manager latches it into
 * the hardware at the next retrace interrupt (double-buffered presentation).
 */

#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "assert.h"
#include "viint.h"

void osViSwapBuffer(void* frameBufPtr) {
  u32 saveMask;

#ifdef _DEBUG
  if (!__osViDevMgr.active) {
    __osError(ERR_OSVISWAPBUFFER_VIMGR, 0);
    return;
  }
  assert(frameBufPtr != NULL);
  // The VI origin register needs a 64-byte aligned buffer.
  if ((u32)frameBufPtr & 0x3f) {
    __osError(ERR_OSVISWAPBUFFER_ADDR, 1, frameBufPtr);
    return;
  }
#endif

  // Stage the buffer in __osViNext under interrupt mask; the retrace handler
  // picks it up via the BUFFER_UPDATED flag.
  saveMask = __osDisableInt();
  __osViNext->framep = frameBufPtr;
  __osViNext->state |= VI_STATE_BUFFER_UPDATED;
  __osRestoreInt(saveMask);
}
