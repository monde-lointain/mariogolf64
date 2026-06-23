/*
 * ai.c -- Audio Interface busy check: a thin predicate over the AI hardware
 * status register, used to gate the next DMA program in osAiSetNextBuffer.
 */
#include "PR/os_internal.h"
#include "PR/rcp.h"

/* Reports whether both audio DMA address/length registers are already loaded.
 * FIFO_FULL is the AI's "no room for another buffer" flag, so a true result
 * means a caller must wait before queuing another transfer. */
s32 __osAiDeviceBusy(void) {
  register s32 status = IO_READ(AI_STATUS_REG);
  if (status & AI_STATUS_FIFO_FULL) {
    return TRUE;
  } else {
    return FALSE;
  }
}
