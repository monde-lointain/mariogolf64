/*
 * The performance meter, which bars the time the CPU and the RCP take each
 * frame.
 */

#ifndef PERFORMANCE_METER_H
#define PERFORMANCE_METER_H

#include <ultra64.h>

// Bits of the readout mode. The readout prints the lengths of the CPU, RSP
// and RDP bars, as of this frame, at their maximum or on average; the free
// memory can replace the CPU line.
#define METER_READOUT_CURRENT (1 << 0)
#define METER_READOUT_MAXIMUM (1 << 1)
#define METER_READOUT_AVERAGE (1 << 2)
#define METER_READOUT_FREE_MEMORY (1 << 3)

void DrawPerformanceMeter(
    u32 extraFrameCount,
    u32 y,
    u32 cpuStartTime,
    u32 cpuEndTime,
    s32 taskFlags,
    s32 readoutMode
);
void DrawMemoryMeter(u32 y, s32 taskFlags);

#endif
