/*
 * Graphics task drain.
 *
 * Blocks the caller until every graphics task queued through nuGfxTaskStart has
 * finished, so it is safe to touch resources the RSP/RDP might still be
 * reading.
 */
#include <nusys.h>

/*
 * Spin until the outstanding-task count reaches zero.
 *
 * nuGfxTaskSpool is decremented by the Graphics Manager as tasks complete; the
 * busy-wait is acceptable here because callers use it at coarse sync points
 * (e.g. just after init or before swapping the retrace callback).
 */
void nuGfxTaskAllEndWait(void) {
  while (nuGfxTaskSpool) {
    ;
  }
}
