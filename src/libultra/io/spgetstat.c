/*
 * spgetstat.c -- read the raw RSP status register. Thin accessor used by the SP
 * task helpers to test yield/halt/DMA bits.
 */
#include "PR/os_internal.h"
#include "PR/rcp.h"

#ident "$Revision: 1.17 $"

u32 __osSpGetStatus(void) { return IO_READ(SP_STATUS_REG); }
