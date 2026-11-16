/*
 * libultra's message queues and the events that interrupts send to them.
 *
 * A queue is a ring of validCount messages from msg[first]. The threads that
 * wait on it are kept by the scheduler, so the queue's mtqueue and fullqueue
 * stay empty.
 */

#include <SDL3/SDL.h>

#include "os_internal.h"
#include "port/os_host.h"
#include "scheduler.h"

typedef struct EventMessage {
    OSMesgQueue *queue;
    OSMesg message;
} EventMessage;

static EventMessage f_EventMessages[OS_NUM_EVENTS];

static int IsQueueFull(const OSMesgQueue *queue) {
    return queue->validCount >= queue->msgCount;
}

// Adds the message at the queue's back, or at its front if jams, and wakes
// the thread that waits longest for one. The queue must have room.
static void InsertMessage(OSMesgQueue *queue, OSMesg message, int jams) {
    s32 index;

    if (jams) {
        queue->first = (queue->first + queue->msgCount - 1) % queue->msgCount;
        index = queue->first;
    }
    else {
        index = (queue->first + queue->validCount) % queue->msgCount;
    }

    queue->msg[index] = message;
    queue->validCount++;
    WakeWaitingThread(queue, WAIT_FOR_MESSAGE);
}

static s32 SendMessage(OSMesgQueue *queue, OSMesg message, s32 flag, int jams) {
    SDL_assert(queue != NULL && queue->msgCount > 0);
    LockScheduler();
    while (IsQueueFull(queue)) {
        if (flag == OS_MESG_NOBLOCK || GetCallingThread() == NULL) {
            UnlockScheduler();
            return -1;
        }

        WaitForWakeUp(queue, WAIT_FOR_ROOM);
    }

    InsertMessage(queue, message, jams);
    ReachPreemptionPoint();
    UnlockScheduler();
    return 0;
}

void osCreateMesgQueue(OSMesgQueue *queue, OSMesg *messages, s32 count) {
    SDL_assert(queue != NULL && messages != NULL && count > 0);
    queue->mtqueue = NULL;
    queue->fullqueue = NULL;
    queue->validCount = 0;
    queue->first = 0;
    queue->msgCount = count;
    queue->msg = messages;
}

s32 osSendMesg(OSMesgQueue *queue, OSMesg message, s32 flag) {
    return SendMessage(queue, message, flag, FALSE);
}

s32 osJamMesg(OSMesgQueue *queue, OSMesg message, s32 flag) {
    return SendMessage(queue, message, flag, TRUE);
}

s32 osRecvMesg(OSMesgQueue *queue, OSMesg *message, s32 flag) {
    SDL_assert(queue != NULL && queue->msgCount > 0);
    LockScheduler();
    while (queue->validCount == 0) {
        if (flag == OS_MESG_NOBLOCK || GetCallingThread() == NULL) {
            UnlockScheduler();
            return -1;
        }

        WaitForWakeUp(queue, WAIT_FOR_MESSAGE);
    }

    if (message != NULL) {
        *message = queue->msg[queue->first];
    }

    queue->first = (queue->first + 1) % queue->msgCount;
    queue->validCount--;
    WakeWaitingThread(queue, WAIT_FOR_ROOM);
    ReachPreemptionPoint();
    UnlockScheduler();
    return 0;
}

void osSetEventMesg(OSEvent event, OSMesgQueue *queue, OSMesg message) {
    SDL_assert(event < OS_NUM_EVENTS);
    LockScheduler();
    f_EventMessages[event].queue = queue;
    f_EventMessages[event].message = message;
    UnlockScheduler();
}

// Sends an interrupt's message, dropping it when the queue is full as the
// N64 does. The caller holds the scheduler's lock.
void PcSendInterruptMessage(OSMesgQueue *queue, OSMesg message) {
    if (queue != NULL && !IsQueueFull(queue)) {
        InsertMessage(queue, message, FALSE);
    }
}

void PcRaiseEvent(OSEvent event) {
    SDL_assert(event < OS_NUM_EVENTS);
    LockScheduler();
    PcSendInterruptMessage(f_EventMessages[event].queue, f_EventMessages[event].message);
    WakeInterruptWaiters();
    ReachPreemptionPoint();
    UnlockScheduler();
}
