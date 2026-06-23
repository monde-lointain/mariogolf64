/*
 * Write the RSP status register. Used with the SP set/clear bit commands to
 * clear interrupt and yield flags and start or stop the processor.
 */
#include "PR/os_internal.h"
#include "PR/rcp.h"

#ident "$Revision: 1.17 $"

void __osSpSetStatus(u32 data) { IO_WRITE(SP_STATUS_REG, data); }
