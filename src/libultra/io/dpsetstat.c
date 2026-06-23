/*
 * dpsetstat.c -- write the RDP (Display Processor) status/command register.
 *
 * Thin wrapper that lets a caller poke the DPC status register directly (e.g.
 * to set or clear the freeze/flush mode bits).
 */
#include "PR/os_internal.h"
#include "PR/rcp.h"

void osDpSetStatus(u32 data) { IO_WRITE(DPC_STATUS_REG, data); }
