/*
 * Global interrupt-mask clear.
 *
 * Clears the requested handler bits from the OS-wide interrupt mask so the
 * corresponding sources are no longer delivered. RCP bits are preserved (the
 * RCP mask is managed separately), and the update runs with interrupts
 * disabled so the read-modify-write of the shared mask is atomic.
 */

#include "PR/os_internal.h"

void __osResetGlobalIntMask(OSHWIntr mask) {
  register u32 saveMask = __osDisableInt();
  __OSGlobalIntMask &= ~(mask & ~OS_IM_RCP);
  __osRestoreInt(saveMask);
}
