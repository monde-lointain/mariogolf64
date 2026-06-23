/*
 * Blank or unblank the video output. Toggles the "black" flag on the pending VI
 * context so the screen is forced black (or restored) at the next retrace.
 */
#include "PR/os_internal.h"
#include "viint.h"

/*
 * active != 0 forces the display black, active == 0 restores normal output. The
 * change lands on __osViNext (the context applied at the next retrace), and
 * runs with interrupts disabled because the retrace handler also touches it.
 */
void osViBlack(u8 active) {
  register u32 saveMask = __osDisableInt();
  if (active) {
    __osViNext->state |= VI_STATE_BLACK;
  } else {
    __osViNext->state &= ~VI_STATE_BLACK;
  }
  __osRestoreInt(saveMask);
}
