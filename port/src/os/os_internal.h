/*
 * What the parts of the port's libultra share among themselves.
 */

#ifndef PORT_OS_INTERNAL_H
#define PORT_OS_INTERNAL_H

#include <ultra64.h>

// Sends an interrupt's message, dropping it when the queue is full, as the
// N64 does. The caller holds the scheduler's lock.
void PcSendInterruptMessage(OSMesgQueue *queue, OSMesg message);

// Readies the scheduler's lock and the threads' slots
void PcInitScheduler(void);

// Takes a snapshot of every overlay's variables, data and bss, as they are
// now, for osPcResetSegment
void PcInitSegments(void);

// Starts the clock that osGetTime and osGetCount read
void PcInitClock(void);

#endif
