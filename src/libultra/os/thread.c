/*
 * Scheduler global state.
 *
 * Defines the kernel's thread bookkeeping that every scheduling primitive reads
 * and writes: the run queue, the global active-thread list, and the currently
 * running and faulted threads.
 */

#include "PR/os_internal.h"
#include "osint.h"

// Shared list terminator. Its priority of -1 sorts below any real thread, so it
// acts as a permanent sentinel tail for the priority-ordered queues and bounds
// every list walk without a NULL check. The run and active queues both start
// empty, pointing straight at it.
struct __osThreadTail __osThreadTail = {NULL, -1};
OSThread* __osRunQueue = (OSThread*)&__osThreadTail;
OSThread* __osActiveQueue = (OSThread*)&__osThreadTail;

// No thread is running until the first dispatch; no fault has occurred yet.
OSThread* __osRunningThread = NULL;
OSThread* __osFaultedThread = NULL;
