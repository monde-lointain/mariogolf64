/*
 * epilinkhandle.c -- register an EPI (Enhanced PI) device handle.
 */
#include "piint.h"

/* Prepend the handle to the global PI device list so later transfers can find
 * its bus-timing settings. Done with interrupts disabled because the list is
 * shared with the PI manager and interrupt context. */
s32 osEPiLinkHandle(OSPiHandle* EPiHandle) {
  u32 saveMask = __osDisableInt();
  EPiHandle->next = __osPiTable;
  __osPiTable = EPiHandle;
  __osRestoreInt(saveMask);
  return 0;
}
