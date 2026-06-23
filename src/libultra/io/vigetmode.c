/*
 * Return the type identifier of the VI mode currently in effect (one of the
 * OS_VI_* constants), so callers can tell NTSC/PAL/MPAL and resolution apart.
 */
#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "viint.h"

/*
 * Reads the active context's mode type with interrupts disabled, since the
 * retrace handler may swap the context. Returns -1 (and, in debug builds,
 * raises an error) if the VI manager has not been started.
 */
u32 osViGetCurrentMode(void) {
  register u32 saveMask;
  register u32 modeType;
#ifdef _DEBUG
  if (!__osViDevMgr.active) {
    __osError(ERR_OSVIGETCURRENTMODE, 0);
    return -1;
  }
#endif
  saveMask = __osDisableInt();
  modeType = (u32)__osViCurr->modep->type;
  __osRestoreInt(saveMask);
  return modeType;
}
