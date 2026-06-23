/*
 * Set the RSP program counter, used to point the SP at the boot microcode in
 * IMEM before it is released to run.
 */
#include "PR/rcp.h"

#ident "$Revision: 1.17 $"

/*
 * Load pc into the SP program counter. The SP must be halted for the write to
 * take effect, so this returns -1 (leaving the PC untouched) if it is running,
 * else 0.
 */
s32 __osSpSetPc(u32 pc) {
  register u32 status = IO_READ(SP_STATUS_REG);
  if (!(status & SP_STATUS_HALT)) {
    return -1;
  }
  IO_WRITE(SP_PC_REG, pc);
  return 0;
}
