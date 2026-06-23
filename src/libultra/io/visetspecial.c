/*
 * Toggle optional VI image-quality features (gamma, dither, divot, AA filter).
 *
 * Each call applies one or more ON/OFF flags to the pending VI control word;
 * the change takes effect at the next retrace.
 */

#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "PR/rcp.h"
#include "viint.h"

// The set of every valid feature flag, used to range-check the argument.
#define OS_VI_SPECIAL_MAX                                      \
  (OS_VI_GAMMA_ON | OS_VI_GAMMA_OFF | OS_VI_GAMMA_DITHER_ON |  \
   OS_VI_GAMMA_DITHER_OFF | OS_VI_DIVOT_ON | OS_VI_DIVOT_OFF | \
   OS_VI_DITHER_FILTER_ON | OS_VI_DITHER_FILTER_OFF)

/* func is a bitwise OR of OS_VI_* feature flags; ON/OFF flags for the same
 * feature should not both be set. */
void osViSetSpecialFeatures(u32 func) {
  register u32 saveMask;

#ifdef _DEBUG
  if (!__osViDevMgr.active) {
    __osError(ERR_OSVISETSPECIAL_VIMGR, 0);
    return 0;
  }
  if ((func < OS_VI_GAMMA_ON) || (func > OS_VI_SPECIAL_MAX)) {
    __osError(ERR_OSVISETSPECIAL_VALUE, 1, func);
    return 0;
  }
#endif

  saveMask = __osDisableInt();

  // Each flag sets or clears its control bit independently.
  if ((func & OS_VI_GAMMA_ON) != 0) {
    __osViNext->control |= VI_CTRL_GAMMA_ON;
  }
  if ((func & OS_VI_GAMMA_OFF) != 0) {
    __osViNext->control &= ~VI_CTRL_GAMMA_ON;
  }
  if ((func & OS_VI_GAMMA_DITHER_ON) != 0) {
    __osViNext->control |= VI_CTRL_GAMMA_DITHER_ON;
  }
  if ((func & OS_VI_GAMMA_DITHER_OFF) != 0) {
    __osViNext->control &= ~VI_CTRL_GAMMA_DITHER_ON;
  }
  if ((func & OS_VI_DIVOT_ON) != 0) {
    __osViNext->control |= VI_CTRL_DIVOT_ON;
  }
  if ((func & OS_VI_DIVOT_OFF) != 0) {
    __osViNext->control &= ~VI_CTRL_DIVOT_ON;
  }

  // The dither filter and anti-aliasing share hardware: enabling the filter
  // forces the AA mode bits off; disabling it restores the mode's AA setting.
  if ((func & OS_VI_DITHER_FILTER_ON) != 0) {
    __osViNext->control |= VI_CTRL_DITHER_FILTER_ON;
    __osViNext->control &= ~VI_CTRL_ANTIALIAS_MASK;
  }
  if ((func & OS_VI_DITHER_FILTER_OFF) != 0) {
    __osViNext->control &= ~VI_CTRL_DITHER_FILTER_ON;
    __osViNext->control |=
        __osViNext->modep->comRegs.ctrl & VI_CTRL_ANTIALIAS_MASK;
  }

  __osViNext->state |= VI_STATE_CTRL_UPDATED;
  __osRestoreInt(saveMask);
}
