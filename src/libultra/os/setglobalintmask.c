/*
 * Global interrupt-mask set.
 *
 * Adds the requested handler bits to the OS-wide interrupt mask, re-enabling
 * those sources. The read-modify-write of the shared mask runs with interrupts
 * disabled so it cannot be torn by an interrupt mid-update. The clearing
 * counterpart is __osResetGlobalIntMask.
 */

#include "PR/os_internal.h"

void __osSetGlobalIntMask(OSHWIntr mask) {
  register u32 saveMask = __osDisableInt();
  __OSGlobalIntMask |= mask;
  __osRestoreInt(saveMask);
}
