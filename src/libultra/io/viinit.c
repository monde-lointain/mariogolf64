/*
 * Initialize the VI manager's double-buffered context state.
 *
 * Sets up the two VI contexts (current + next), picks a default display mode
 * for the console's TV type, blacks the screen, and resets the VI hardware
 * before the first swap.
 */

#include "PR/os_internal.h"
#include "PR/R4300.h"
#include "PR/rcp.h"
#include "viint.h"

// The two display contexts the manager ping-pongs between.
extern __OSViContext vi[2];

void __osViInit(void) {
  bzero(vi, sizeof(vi));
  __osViCurr = &vi[0];
  __osViNext = &vi[1];
  __osViNext->retraceCount = 1;
  __osViCurr->retraceCount = 1;

  // Point both contexts at a benign default frame buffer (start of cached
  // RDRAM) until the application supplies a real one.
  __osViNext->framep = (void*)K0BASE;
  __osViCurr->framep = (void*)K0BASE;

  // Boot mode follows the console's TV standard.
  if (osTvType == OS_TV_TYPE_PAL) {
    __osViNext->modep = &osViModePalLan1;
  } else if (osTvType == OS_TV_TYPE_MPAL) {
    __osViNext->modep = &osViModeMpalLan1;
  } else {
    __osViNext->modep = &osViModeNtscLan1;
  }
  __osViNext->state = VI_STATE_BLACK;
  __osViNext->control = __osViNext->modep->comRegs.ctrl;

  // Wait for the VI to reach the top of the frame, then disable output before
  // loading the first context, so the reset is not seen mid-scan.
  while (IO_READ(VI_CURRENT_REG) > 10) {
  }
  IO_WRITE(VI_CONTROL_REG, 0);
  __osViSwapContext();
}
