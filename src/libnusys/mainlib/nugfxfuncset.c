/*
 * Retrace callback registration.
 *
 * Installs the application's per-retrace graphics callback in the graphics
 * thread. The callback receives the count of still-pending graphics tasks so it
 * can skip work while the previous frame's task is unfinished.
 */
#include <nusys.h>

/*
 * Register func as the retrace callback.
 *
 * Existing tasks are drained first so the swap cannot race a callback already
 * in flight, and the pointer is then written with interrupts masked so the
 * graphics thread never observes a half-updated pointer.
 */
void nuGfxFuncSet(NUGfxFunc func) {
  OSIntMask mask;

  nuGfxTaskAllEndWait();
  mask = osSetIntMask(OS_IM_NONE);
  nuGfxFunc = func;
  osSetIntMask(mask);
}
