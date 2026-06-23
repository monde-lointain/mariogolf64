/*
 * Real-time counter read.
 *
 * Returns the current 64-bit OS time. The hardware only provides a free-running
 * 32-bit cycle counter; the full 64-bit value is reconstructed from the last
 * timer-interrupt snapshot (__osCurrentTime) plus the cycles elapsed since the
 * snapshot was taken.
 */

#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "osint.h"
#include "viint.h"

OSTime osGetTime() {
  u32 tmptime;
  u32 elapseCount;
  OSTime currentCount;
  register u32 saveMask;

#ifdef _DEBUG
  if (!__osViDevMgr.active) {
    __osError(ERR_OSGETTIME, 0);
    return 0;
  }
#endif

  // Sample the running counter and the last interrupt snapshot together, with
  // interrupts disabled, so the two halves cannot be split by a timer tick.
  saveMask = __osDisableInt();
  tmptime = osGetCount();
  elapseCount = tmptime - __osBaseCounter;
  currentCount = __osCurrentTime;
  __osRestoreInt(saveMask);

  return currentCount + elapseCount;
}
