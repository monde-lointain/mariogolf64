/*
 * Where a thread of the game spins or works at length without calling
 * libultra. On the N64 an interrupt can take the CPU from it at any moment,
 * so the spinning costs only the thread's own time. The PC port runs the
 * game's threads one at a time, changing threads only in libultra calls,
 * so these places must give the CPU up themselves. The N64's builds leave
 * them as they were.
 */

#ifndef THREAD_WAIT_H
#define THREAD_WAIT_H

#ifdef TARGET_PC
#    include <ultra64.h>

// In a loop that waits for another thread or an interrupt to change
// something: blocks until the next interrupt
#    define WaitForOtherThreads() osPcWaitForInterrupt()

// In a long computation: lets a thread that an interrupt has woken, and
// that comes before this one, run now
#    define LetWokenThreadsRun() osPcReachPreemptionPoint()
#else
#    define WaitForOtherThreads() ((void)0)
#    define LetWokenThreadsRun() ((void)0)
#endif

#endif
