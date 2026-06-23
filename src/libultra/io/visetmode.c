/*
 * Select the VI display mode (resolution, color depth, interlace, TV format).
 *
 * Stages a new OSViMode into the pending VI context; it takes effect at the
 * next retrace. The control register seed is copied from the mode's common
 * regs.
 */

#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "assert.h"
#include "viint.h"

void osViSetMode(OSViMode* modep) {
  register u32 saveMask;

#ifdef _DEBUG
  if (!__osViDevMgr.active) {
    __osError(ERR_OSVISETMODE, 0);
    return 0;
  }
  assert(modep != NULL);
#endif

  // Stage the mode and prime the control word from it; MODE_UPDATED makes the
  // retrace handler reprogram every VI register from the new mode.
  saveMask = __osDisableInt();
  __osViNext->modep = modep;
  __osViNext->state = VI_STATE_MODE_UPDATED;
  __osViNext->control = __osViNext->modep->comRegs.ctrl;
  __osRestoreInt(saveMask);
}
