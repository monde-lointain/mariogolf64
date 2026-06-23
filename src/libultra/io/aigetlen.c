/*
 * aigetlen.c -- query how much of the current audio DMA buffer is left.
 */
#include "PR/rcp.h"

/* Returns the bytes still to be transferred from the active DRAM buffer to the
 * AI. Drains toward zero as the hardware consumes the queued samples. */
u32 osAiGetLength(void) { return IO_READ(AI_LEN_REG); }
