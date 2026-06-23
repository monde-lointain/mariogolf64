/*
 * aigetstat.c -- expose the raw AI (Audio Interface) status word.
 */
#include "PR/rcp.h"

/* Returns the 32-bit AI status register. Only two bits carry meaning to
 * callers: FIFO_FULL (both DMA registers programmed) and DMA_BUSY (a transfer
 * is in progress). */
u32 osAiGetStatus() { return IO_READ(AI_STATUS_REG); }
