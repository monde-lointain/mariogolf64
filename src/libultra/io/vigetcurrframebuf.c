/*
 * Return the framebuffer currently being scanned out to the display. The
 * scheduler uses this (against the next framebuffer) to tell when a pending
 * buffer swap has taken effect.
 */
#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "viint.h"

/*
 * Reads __osViCurr->framep with interrupts disabled, since the retrace handler
 * updates it on a swap. Returns 0 (and, in debug builds, raises an error) if
 * the VI manager has not been started.
 */
void* osViGetCurrentFramebuffer(void) {
  register u32 saveMask;
  void* framep;
#ifdef _DEBUG
  if (!__osViDevMgr.active) {
    __osError(ERR_OSVIGETCURRENTFRAMEBUFFER, 0);
    return 0;
  }
#endif
  saveMask = __osDisableInt();
  framep = __osViCurr->framep;
  __osRestoreInt(saveMask);
  return framep;
}
