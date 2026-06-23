/*
 * Register the callback the scheduler runs when it receives a pre-NMI (reset)
 * event.
 */

#include <nusys.h>

/*
 * Install func as the pre-NMI handler. The store is bracketed by an interrupt
 * mask so the scheduler can't fire mid-update and read a half-written pointer.
 */
void nuPreNMIFuncSet(NUScPreNMIFunc func) {
  OSIntMask mask;

  mask = osSetIntMask(OS_IM_NONE);
  nuScPreNMIFunc = (NUScPreNMIFunc)func;
  osSetIntMask(mask);
}
