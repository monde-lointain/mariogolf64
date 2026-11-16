/*
 * libultra's threads on the host's threads, one of them holding the CPU at a
 * time (see scheduler.h).
 */

#include "scheduler.h"

#include <SDL3/SDL.h>
#include <setjmp.h>

#include "port/os_host.h"

// More than the game ever has at once: the main, idle, graphics, task
// manager, scheduler, loader, planner and fault threads
#define THREAD_SLOT_COUNT 32
// The game's threads' stacks hold large buffers, and the host's frames are
// larger than the N64's
#define THREAD_STACK_SIZE (2 * 1024 * 1024)

typedef enum ThreadState {
    THREAD_UNUSED,
    THREAD_STOPPED, // Created but not started, or stopped
    THREAD_RUNNABLE,
    THREAD_WAITING,
    THREAD_EXITING // Destroyed, its host thread not yet gone
} ThreadState;

// The host's side of an OSThread
typedef struct ThreadSlot {
    OSThread *thread;
    ThreadState state;
    void (*entry)(void *);
    void *argument;
    SDL_Condition *turn; // Signaled when the slot gets the CPU or is destroyed
    // What a waiting thread waits for
    OSMesgQueue *waitQueue;
    WaitKind waitKind;
    // When the thread last became runnable: of threads of one priority, the
    // one that has waited longest runs first
    u64 readySequence;
    jmp_buf exit; // Where a destroyed thread unwinds to
} ThreadSlot;

static SDL_Mutex *f_Lock;
static ThreadSlot f_Slots[THREAD_SLOT_COUNT];
static ThreadSlot *f_RunningSlot; // NULL while every thread waits
static u64 f_ReadySequence;
static SDL_TLSID f_CallingSlot;
static bool f_IsHalted; // No thread gets the CPU again
static SDL_Condition *f_CpuReleased; // Signaled when a halt leaves it idle

volatile int osPcPreemptPending;

static ThreadSlot *FindSlot(const OSThread *thread) {
    int i;

    for (i = 0; i < THREAD_SLOT_COUNT; i++) {
        ThreadState state = f_Slots[i].state;

        if (state != THREAD_UNUSED && state != THREAD_EXITING && f_Slots[i].thread == thread) {
            return &f_Slots[i];
        }
    }

    return NULL;
}

static ThreadSlot *GetCallingSlot(void) {
    return SDL_GetTLS(&f_CallingSlot);
}

static ThreadSlot *FindSubjectSlot(OSThread *thread) {
    return (thread != NULL) ? FindSlot(thread) : GetCallingSlot();
}

static void MarkRunnable(ThreadSlot *slot) {
    slot->state = THREAD_RUNNABLE;
    slot->thread->state = OS_STATE_RUNNABLE;
    slot->waitQueue = NULL;
    slot->readySequence = f_ReadySequence++;
}

// Whether a runnable slot should run before best: a higher priority, or an
// equal one with the CPU already, or one runnable for longer
static int RunsBefore(const ThreadSlot *slot, const ThreadSlot *best) {
    if (slot->thread->priority != best->thread->priority) {
        return slot->thread->priority > best->thread->priority;
    }

    if (best == f_RunningSlot) {
        return FALSE;
    }

    return slot == f_RunningSlot || slot->readySequence < best->readySequence;
}

static ThreadSlot *ChooseSlot(void) {
    ThreadSlot *best = NULL;
    int i;

    if (f_IsHalted) {
        return NULL;
    }

    for (i = 0; i < THREAD_SLOT_COUNT; i++) {
        ThreadSlot *slot = &f_Slots[i];

        if (slot->state == THREAD_RUNNABLE && (best == NULL || RunsBefore(slot, best))) {
            best = slot;
        }
    }

    return best;
}

// Gives the CPU to the thread that should hold it now
static void Dispatch(void) {
    ThreadSlot *chosen = ChooseSlot();

    osPcPreemptPending = FALSE;
    if (chosen != f_RunningSlot) {
        if (f_RunningSlot != NULL && f_RunningSlot->state == THREAD_RUNNABLE) {
            f_RunningSlot->thread->state = OS_STATE_RUNNABLE;
        }

        f_RunningSlot = chosen;
        if (chosen != NULL) {
            chosen->thread->state = OS_STATE_RUNNING;
            SDL_BroadcastCondition(chosen->turn);
        }
        else {
            SDL_BroadcastCondition(f_CpuReleased);
        }
    }
}

// Unwinds a destroyed thread to the bottom of its host thread
static PC_NORETURN void ExitDestroyedThread(ThreadSlot *slot) {
    UnlockScheduler();
    longjmp(slot->exit, 1);
}

// Blocks until the slot holds the CPU, unwinding if it is destroyed
static void ParkUntilRunning(ThreadSlot *slot) {
    while (f_RunningSlot != slot || slot->state != THREAD_RUNNABLE) {
        if (slot->state == THREAD_EXITING) {
            ExitDestroyedThread(slot);
        }

        SDL_WaitCondition(slot->turn, f_Lock);
    }
}

void LockScheduler(void) {
    SDL_LockMutex(f_Lock);
}

void UnlockScheduler(void) {
    SDL_UnlockMutex(f_Lock);
}

OSThread *GetCallingThread(void) {
    ThreadSlot *slot = GetCallingSlot();

    return (slot != NULL) ? slot->thread : NULL;
}

void ReachPreemptionPoint(void) {
    ThreadSlot *slot = GetCallingSlot();

    if (slot == NULL) {
        RequestPreemption();
        return;
    }

    Dispatch();
    ParkUntilRunning(slot);
}

void RequestPreemption(void) {
    ThreadSlot *chosen;

    if (f_RunningSlot == NULL) {
        Dispatch();
        return;
    }

    chosen = ChooseSlot();
    if (chosen != f_RunningSlot) {
        osPcPreemptPending = TRUE;
    }
}

void WaitForWakeUp(OSMesgQueue *queue, WaitKind kind) {
    ThreadSlot *slot = GetCallingSlot();

    SDL_assert_always(slot != NULL);
    slot->state = THREAD_WAITING;
    slot->thread->state = OS_STATE_WAITING;
    slot->waitQueue = queue;
    slot->waitKind = kind;
    Dispatch();
    ParkUntilRunning(slot);
}

int WakeWaitingThread(OSMesgQueue *queue, WaitKind kind) {
    ThreadSlot *best = NULL;
    int i;

    for (i = 0; i < THREAD_SLOT_COUNT; i++) {
        ThreadSlot *slot = &f_Slots[i];

        if (slot->state == THREAD_WAITING &&
            slot->waitQueue == queue &&
            slot->waitKind == kind &&
            (best == NULL || slot->thread->priority > best->thread->priority)) {
            best = slot;
        }
    }

    if (best == NULL) {
        return FALSE;
    }

    MarkRunnable(best);
    return TRUE;
}

void WakeInterruptWaiters(void) {
    int i;

    for (i = 0; i < THREAD_SLOT_COUNT; i++) {
        if (f_Slots[i].state == THREAD_WAITING && f_Slots[i].waitKind == WAIT_FOR_INTERRUPT) {
            MarkRunnable(&f_Slots[i]);
        }
    }
}

// Ends the slot's thread: the slot stays EXITING until its host thread has
// unwound, so that no new thread takes it before then
static void EndSlot(ThreadSlot *slot) {
    slot->state = THREAD_EXITING;
    slot->thread->state = OS_STATE_STOPPED;
    SDL_BroadcastCondition(slot->turn);
}

static int RunHostThread(void *data) {
    ThreadSlot *slot = data;

    SDL_SetTLS(&f_CallingSlot, slot, NULL);
    if (setjmp(slot->exit) == 0) {
        LockScheduler();
        ParkUntilRunning(slot);
        UnlockScheduler();

        slot->entry(slot->argument);

        // Returning from the entry destroys the thread, as on the N64
        LockScheduler();
        EndSlot(slot);
        Dispatch();
        UnlockScheduler();
    }

    LockScheduler();
    slot->state = THREAD_UNUSED;
    slot->thread = NULL;
    UnlockScheduler();
    return 0;
}

static void StartHostThread(ThreadSlot *slot) {
    SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_Thread *hostThread;
    char name[32];

    SDL_snprintf(name, sizeof(name), "ultra%d", (int)slot->thread->id);
    SDL_SetPointerProperty(
        properties,
        SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER,
        (void *)RunHostThread
    );
    SDL_SetPointerProperty(properties, SDL_PROP_THREAD_CREATE_USERDATA_POINTER, slot);
    SDL_SetStringProperty(properties, SDL_PROP_THREAD_CREATE_NAME_STRING, name);
    SDL_SetNumberProperty(properties, SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER, THREAD_STACK_SIZE);
    hostThread = SDL_CreateThreadWithProperties(properties);
    SDL_DestroyProperties(properties);
    if (hostThread == NULL) {
        PcFatalError("Can't start thread %s: %s", name, SDL_GetError());
    }

    SDL_DetachThread(hostThread);
}

void PcInitScheduler(void) {
    int i;

    f_Lock = SDL_CreateMutex();
    f_CpuReleased = SDL_CreateCondition();
    if (f_Lock == NULL || f_CpuReleased == NULL) {
        PcFatalError("Can't create the scheduler's lock: %s", SDL_GetError());
    }

    for (i = 0; i < THREAD_SLOT_COUNT; i++) {
        f_Slots[i].turn = SDL_CreateCondition();
        if (f_Slots[i].turn == NULL) {
            PcFatalError("Can't create a thread's condition: %s", SDL_GetError());
        }
    }
}

void osCreateThread(
    OSThread *thread,
    OSId id,
    void (*entry)(void *),
    void *argument,
    void *stack,
    OSPri priority
) {
    ThreadSlot *slot = NULL;
    int i;

    (void)stack;
    SDL_assert_always(thread != NULL && entry != NULL);
    LockScheduler();
    SDL_assert_always(FindSlot(thread) == NULL);
    for (i = 0; slot == NULL && i < THREAD_SLOT_COUNT; i++) {
        if (f_Slots[i].state == THREAD_UNUSED) {
            slot = &f_Slots[i];
        }
    }

    if (slot == NULL) {
        PcFatalError("More than %d threads", THREAD_SLOT_COUNT);
    }

    SDL_memset(thread, 0, sizeof(*thread));
    thread->id = id;
    thread->priority = priority;
    thread->state = OS_STATE_STOPPED;
    slot->thread = thread;
    slot->state = THREAD_STOPPED;
    slot->entry = entry;
    slot->argument = argument;
    StartHostThread(slot);
    UnlockScheduler();
}

void osStartThread(OSThread *thread) {
    ThreadSlot *slot;

    LockScheduler();
    slot = FindSlot(thread);
    SDL_assert_always(slot != NULL);
    if (slot->state == THREAD_STOPPED) {
        MarkRunnable(slot);
        ReachPreemptionPoint();
    }

    UnlockScheduler();
}

void osStopThread(OSThread *thread) {
    ThreadSlot *slot;

    LockScheduler();
    slot = FindSubjectSlot(thread);
    if (slot != NULL) {
        slot->state = THREAD_STOPPED;
        slot->thread->state = OS_STATE_STOPPED;
        Dispatch();
        if (slot == GetCallingSlot()) {
            ParkUntilRunning(slot);
        }
    }

    UnlockScheduler();
}

void osDestroyThread(OSThread *thread) {
    ThreadSlot *slot;

    LockScheduler();
    slot = FindSubjectSlot(thread);
    if (slot != NULL) {
        EndSlot(slot);
        Dispatch();
        if (slot == GetCallingSlot()) {
            ExitDestroyedThread(slot);
        }
    }

    UnlockScheduler();
}

OSPri osGetThreadPri(OSThread *thread) {
    OSThread *subject = (thread != NULL) ? thread : GetCallingThread();

    return (subject != NULL) ? subject->priority : OS_PRIORITY_IDLE;
}

void osSetThreadPri(OSThread *thread, OSPri priority) {
    OSThread *subject;

    LockScheduler();
    subject = (thread != NULL) ? thread : GetCallingThread();
    if (subject != NULL && subject->priority != priority) {
        subject->priority = priority;
        ReachPreemptionPoint();
    }

    UnlockScheduler();
}

OSId osGetThreadId(OSThread *thread) {
    OSThread *subject = (thread != NULL) ? thread : GetCallingThread();

    return (subject != NULL) ? subject->id : 0;
}

void osYieldThread(void) {
    ThreadSlot *slot;

    LockScheduler();
    slot = GetCallingSlot();
    if (slot != NULL) {
        // Behind every other runnable thread of its priority
        slot->readySequence = f_ReadySequence++;
        f_RunningSlot = NULL;
        ReachPreemptionPoint();
    }

    UnlockScheduler();
}

void osPcPreempt(void) {
    LockScheduler();
    ReachPreemptionPoint();
    UnlockScheduler();
}

void osPcWaitForInterrupt(void) {
    LockScheduler();
    if (GetCallingSlot() != NULL) {
        WaitForWakeUp(NULL, WAIT_FOR_INTERRUPT);
    }

    UnlockScheduler();
}

bool PcHaltGame(int timeoutMs) {
    const Uint64 deadline = SDL_GetTicks() + (Uint64)SDL_max(timeoutMs, 0);
    bool isIdle;

    LockScheduler();
    f_IsHalted = true;
    if (f_RunningSlot != NULL) {
        osPcPreemptPending = TRUE;
    }

    while (f_RunningSlot != NULL && SDL_GetTicks() < deadline) {
        (void)SDL_WaitConditionTimeout(f_CpuReleased, f_Lock, (Sint32)(deadline - SDL_GetTicks()));
    }

    isIdle = f_RunningSlot == NULL;
    UnlockScheduler();
    return isIdle;
}

OSThread *__osGetCurrFaultedThread(void) {
    // A host's exception ends the program rather than fault a thread
    return NULL;
}
