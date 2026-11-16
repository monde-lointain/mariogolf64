/*
 * The N64's single CPU, shared among libultra's threads.
 *
 * Each OSThread runs on a host thread of its own, but only the one that
 * holds the CPU runs game code: the highest-priority thread that isn't
 * waiting, as on the N64. The CPU changes hands only inside libultra's
 * calls, at a preemption point, so that no two threads of the game's ever
 * run at once. An interrupt that wakes a thread of higher priority than the
 * running one asks for a switch, which the running thread makes at its next
 * libultra call or osPcPreempt.
 *
 * Every routine here but LockScheduler expects the caller to hold the
 * scheduler's lock.
 */

#ifndef PORT_OS_SCHEDULER_H
#define PORT_OS_SCHEDULER_H

#include <ultra64.h>

// What a waiting thread waits for
typedef enum WaitKind {
    WAIT_FOR_MESSAGE, // A message in an empty queue
    WAIT_FOR_ROOM, // Room in a full queue
    WAIT_FOR_INTERRUPT // Any interrupt, in osPcWaitForInterrupt
} WaitKind;

// Takes and gives back the lock that guards every thread's and queue's state
void LockScheduler(void);
void UnlockScheduler(void);

// The OSThread that the calling host thread runs, or NULL for a host thread
// of the platform's, which acts as the hardware
OSThread *GetCallingThread(void);

// Blocks the calling thread until another wakes it for what it waits for
// and it holds the CPU again. queue is NULL for WAIT_FOR_INTERRUPT.
void WaitForWakeUp(OSMesgQueue *queue, WaitKind kind);

// Makes the highest-priority thread waiting on the queue for what kind names
// runnable, if any, and tells whether there was one. The caller then reaches
// a preemption point, or calls RequestPreemption from the platform's threads.
int WakeWaitingThread(OSMesgQueue *queue, WaitKind kind);

// Makes every thread parked in osPcWaitForInterrupt runnable
void WakeInterruptWaiters(void);

// For the platform's threads, after waking threads: gives an idle CPU to
// the best of them at once, or asks the running thread to give it up if one
// outranks it
void RequestPreemption(void);

// For a thread of the game's: hands the CPU to a thread of higher priority
// if one is runnable, and returns once the caller holds it again
void ReachPreemptionPoint(void);

#endif
