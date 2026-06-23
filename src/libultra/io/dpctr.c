/*
 * dpctr.c -- snapshot the RDP (Display Processor) performance counters.
 */
#include "PR/os_internal.h"
#include "PR/rcp.h"

/* Read the four RDP timing counters into array[0..3] in register order: total
 * clocks, command-buffer-busy, pipe-busy, and TMEM-load cycles. Used to profile
 * how the rasterizer spends a frame. */
void osDpGetCounters(u32* array) {
  *array++ = IO_READ(DPC_CLOCK_REG);
  *array++ = IO_READ(DPC_BUFBUSY_REG);
  *array++ = IO_READ(DPC_PIPEBUSY_REG);
  *array++ = IO_READ(DPC_TMEM_REG);
}
