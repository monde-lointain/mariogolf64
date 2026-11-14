#ifndef PERFORMANCE_METER_H
#define PERFORMANCE_METER_H

#include <ultra64.h>

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
