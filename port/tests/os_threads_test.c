/*
 * Tests of the port's libultra threads and message queues: that threads run
 * one at a time in the N64's priority order, that messages and interrupts
 * wake them, and that a destroyed thread's slot can be used again.
 *
 * The test's main thread plays the hardware, as the platform layer does.
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "port/os_host.h"
#include "test_support.h"

#define LOW_PRIORITY 10
#define MIDDLE_PRIORITY 20
#define HIGH_PRIORITY 30
#define QUEUE_LENGTH 4
#define TRACE_LENGTH 64

static char f_Trace[TRACE_LENGTH];
static int f_TraceLength;
static SDL_Semaphore *f_TestDone;
static OSThread f_Threads[4];
static OSMesgQueue f_Queue;
static OSMesg f_Messages[QUEUE_LENGTH];
static SDL_AtomicInt f_RunningCount;
static SDL_AtomicInt f_HaltCounter;

static void Trace(char mark) {
    // More than one thread of the game's in here at once is a failure
    EXPECT(SDL_AddAtomicInt(&f_RunningCount, 1) == 0);
    if (f_TraceLength < TRACE_LENGTH - 1) {
        f_Trace[f_TraceLength++] = mark;
    }

    SDL_AddAtomicInt(&f_RunningCount, -1);
}

static void ResetTrace(void) {
    SDL_memset(f_Trace, 0, sizeof(f_Trace));
    f_TraceLength = 0;
}

static void RunHighThread(void *argument) {
    (void)argument;
    Trace('H');
}

// Starting a thread of higher priority runs it before the starter goes on
static void TestHigherPriorityRunsFirst(void *argument) {
    (void)argument;
    Trace('a');
    osCreateThread(&f_Threads[1], 2, RunHighThread, NULL, NULL, HIGH_PRIORITY);
    osStartThread(&f_Threads[1]);
    Trace('b');
    SDL_SignalSemaphore(f_TestDone);
}

static void RunEqualThread(void *argument) {
    (void)argument;
    Trace('E');
}

// A thread of equal priority waits until the starter blocks
static void TestEqualPriorityWaits(void *argument) {
    (void)argument;
    osCreateMesgQueue(&f_Queue, f_Messages, QUEUE_LENGTH);
    osCreateThread(&f_Threads[1], 2, RunEqualThread, NULL, NULL, LOW_PRIORITY);
    osStartThread(&f_Threads[1]);
    Trace('a');
    osYieldThread();
    Trace('b');
    SDL_SignalSemaphore(f_TestDone);
}

static void RunEchoThread(void *argument) {
    OSMesgQueue *replies = argument;
    OSMesg message;
    int i;

    for (i = 0; i < 3; i++) {
        osRecvMesg(&f_Queue, &message, OS_MESG_BLOCK);
        Trace((char)(uintptr_t)message);
        osSendMesg(replies, message, OS_MESG_BLOCK);
    }
}

// A higher-priority thread blocked on a queue runs as soon as a message
// arrives, and the sender resumes when it blocks again
static void TestMessagesWakeReceivers(void *argument) {
    static OSMesgQueue replies;
    static OSMesg replyMessages[QUEUE_LENGTH];
    OSMesg reply;
    char mark;

    (void)argument;
    osCreateMesgQueue(&f_Queue, f_Messages, QUEUE_LENGTH);
    osCreateMesgQueue(&replies, replyMessages, QUEUE_LENGTH);
    osCreateThread(&f_Threads[1], 2, RunEchoThread, &replies, NULL, HIGH_PRIORITY);
    osStartThread(&f_Threads[1]);
    for (mark = 'x'; mark <= 'z'; mark++) {
        osSendMesg(&f_Queue, (OSMesg)(uintptr_t)mark, OS_MESG_BLOCK);
        Trace('s');
        osRecvMesg(&replies, &reply, OS_MESG_BLOCK);
        EXPECT((char)(uintptr_t)reply == mark);
    }

    EXPECT(osRecvMesg(&replies, &reply, OS_MESG_NOBLOCK) == -1);
    SDL_SignalSemaphore(f_TestDone);
}

static void RunFillingThread(void *argument) {
    int i;

    (void)argument;
    for (i = 0; i < QUEUE_LENGTH + 2; i++) {
        osSendMesg(&f_Queue, (OSMesg)(uintptr_t)i, OS_MESG_BLOCK);
        Trace('0' + i);
    }
}

// A sender to a full queue waits until a receiver makes room
static void TestFullQueueBlocksSender(void *argument) {
    OSMesg message;
    int i;

    (void)argument;
    osCreateMesgQueue(&f_Queue, f_Messages, QUEUE_LENGTH);
    osCreateThread(&f_Threads[1], 2, RunFillingThread, NULL, NULL, HIGH_PRIORITY);
    osStartThread(&f_Threads[1]);
    Trace('r');
    for (i = 0; i < QUEUE_LENGTH + 2; i++) {
        osRecvMesg(&f_Queue, &message, OS_MESG_BLOCK);
        EXPECT((int)(uintptr_t)message == i);
    }

    Trace('e');
    SDL_SignalSemaphore(f_TestDone);
}

static void RunInterruptWaiter(void *argument) {
    OSMesg message;

    (void)argument;
    osRecvMesg(&f_Queue, &message, OS_MESG_BLOCK);
    Trace('I');
    SDL_SignalSemaphore(f_TestDone);
}

static void RunSpinningThread(void *argument) {
    (void)argument;
    Trace('s');
    while (f_TraceLength < 2) {
        osPcReachPreemptionPoint();
    }

    Trace('S');
}

// An interrupt wakes the thread waiting for its message, which takes the
// CPU from a lower-priority thread at that thread's next preemption point
static void TestInterruptPreempts(void *argument) {
    (void)argument;
    osCreateMesgQueue(&f_Queue, f_Messages, QUEUE_LENGTH);
    osSetEventMesg(OS_EVENT_SI, &f_Queue, (OSMesg)1);
    osCreateThread(&f_Threads[1], 2, RunInterruptWaiter, NULL, NULL, HIGH_PRIORITY);
    osStartThread(&f_Threads[1]);
    osCreateThread(&f_Threads[2], 3, RunSpinningThread, NULL, NULL, LOW_PRIORITY);
    osStartThread(&f_Threads[2]);
    // The boot thread ends here, leaving the spinner to run
}

static void RunWaitingForever(void *argument) {
    OSMesg message;

    (void)argument;
    Trace('w');
    osRecvMesg(&f_Queue, &message, OS_MESG_BLOCK);
    Trace('!');
}

// A thread destroyed while it waits never runs again, and its OSThread can
// start a new thread
static void TestDestroyWaitingThread(void *argument) {
    int round;

    (void)argument;
    osCreateMesgQueue(&f_Queue, f_Messages, QUEUE_LENGTH);
    for (round = 0; round < 40; round++) {
        osCreateThread(&f_Threads[1], 2, RunWaitingForever, NULL, NULL, HIGH_PRIORITY);
        osStartThread(&f_Threads[1]);
        osDestroyThread(&f_Threads[1]);
    }

    osSendMesg(&f_Queue, NULL, OS_MESG_NOBLOCK);
    Trace('d');
    SDL_SignalSemaphore(f_TestDone);
}

// Counts forever, calling libultra as game code does
static void TestHaltStopsThreads(void *argument) {
    (void)argument;
    SDL_SignalSemaphore(f_TestDone);
    for (;;) {
        SDL_AddAtomicInt(&f_HaltCounter, 1);
        osYieldThread();
    }
}

static void RunBootThread(void (*test)(void *)) {
    ResetTrace();
    PcStartBootThread(&f_Threads[0], test, NULL, LOW_PRIORITY);
    SDL_WaitSemaphore(f_TestDone);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    PcInitOs();
    f_TestDone = SDL_CreateSemaphore(0);

    RunBootThread(TestHigherPriorityRunsFirst);
    EXPECT_STRING(f_Trace, "aHb");

    RunBootThread(TestEqualPriorityWaits);
    EXPECT_STRING(f_Trace, "aEb");

    RunBootThread(TestMessagesWakeReceivers);
    EXPECT_STRING(f_Trace, "xsyszs");

    RunBootThread(TestFullQueueBlocksSender);
    EXPECT_STRING(f_Trace, "0123r45e");

    ResetTrace();
    PcStartBootThread(&f_Threads[0], TestInterruptPreempts, NULL, MIDDLE_PRIORITY);
    while (f_TraceLength < 1) {
        SDL_Delay(1);
    }

    PcRaiseEvent(OS_EVENT_SI);
    SDL_WaitSemaphore(f_TestDone);
    while (f_TraceLength < 3) {
        SDL_Delay(1);
    }

    EXPECT_STRING(f_Trace, "sIS");

    RunBootThread(TestDestroyWaitingThread);
    EXPECT(f_TraceLength == 41);
    EXPECT(f_Trace[f_TraceLength - 1] == 'd');

    // Once halted, no thread of the game's runs again
    RunBootThread(TestHaltStopsThreads);
    while (SDL_GetAtomicInt(&f_HaltCounter) < 1000) {
        SDL_Delay(1);
    }

    EXPECT(PcHaltGame(1000));
    {
        const int haltedCount = SDL_GetAtomicInt(&f_HaltCounter);

        SDL_Delay(20);
        PcRaiseEvent(OS_EVENT_SI);
        SDL_Delay(20);
        EXPECT(SDL_GetAtomicInt(&f_HaltCounter) == haltedCount);
    }

    return ReportTestResults("os_threads_test");
}
