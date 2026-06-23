/*
 * Controller-data latch.
 *
 * The Controller Manager refreshes its internal pad buffer every retrace. These
 * two calls toggle a flag the retrace callback honors, letting an application
 * freeze the buffer (e.g. to read a consistent snapshot across a frame) and
 * release it again. The lock does not affect the explicit read-start path.
 */
#include <nusys.h>

/*
 * Stop the Controller Manager from overwriting the shared pad buffer.
 *
 * The flag is set under a full interrupt mask so the retrace callback cannot
 * observe a half-written value; the previous mask is restored on the way out.
 */
void nuContDataLock(void) {
  OSIntMask mask;

  mask = osSetIntMask(OS_IM_NONE);
  nuContDataLockKey = NU_CONT_DATA_LOCK;
  osSetIntMask(mask);
}

/* Re-enable the per-retrace buffer refresh latched by nuContDataLock. */
void nuContDataUnLock(void) {
  OSIntMask mask;

  mask = osSetIntMask(OS_IM_NONE);
  nuContDataLockKey = NU_CONT_DATA_UNLOCK;
  osSetIntMask(mask);
}
