/*
 * What the parts of the port's NuSystem share among themselves.
 */

#ifndef PORT_NUSYS_INTERNAL_H
#define PORT_NUSYS_INTERNAL_H

#include <nusys.h>

// The scheduler's event messages, which come into nusched.retraceMQ
#define SCHEDULER_EVENT_RETRACE ((OSMesg)1)
#define SCHEDULER_EVENT_PRENMI ((OSMesg)2)

// Starts the scheduler's threads, which pass the retraces and the reset
// button to the clients and finish the audio tasks
void NuStartScheduler(void);

// Opens the cartridge for NuSystem's reads
void nuPiInit(void);

#endif
