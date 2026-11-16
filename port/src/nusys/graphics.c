/*
 * NuSystem's graphics for the port: the thread that hands each retrace to
 * the game, the graphics tasks, and the frame buffers that they draw into.
 *
 * A task's display list runs as soon as the game starts the task. The task
 * manager thread then retires it, showing a finished frame no sooner than
 * SWAP_RETRACE_MIN retraces after the last, as the game's NuSystem does, so
 * that every frame stays on the screen as long.
 *
 * The game draws into two or three frame buffers. With two, the third's
 * memory holds the depth buffer, which the saved-frame effect draws into;
 * with three, the depth buffer lies at a fixed address. The buffers in use
 * form a ring through f_NextFramebuffer, and the one left out is the spare.
 */

#include <framebuffer_count.h>

#include "nusys_internal.h"
#include "port/nusys_host.h"

#define GFX_STACK_SIZE 0x10000
#define GFX_QUEUE_LENGTH 8
#define TASK_QUEUE_LENGTH NU_GFX_TASK_NUM
#define SWAP_RETRACE_MIN 2
#define NO_SPARE_FRAMEBUFFER 0xFF
#define FRAMEBUFFER_SLOT_COUNT 3

// The depth buffers' fixed addresses
#define DOUBLE_BUFFERING_INITIAL_DEPTH_BUFFER 0x803DA800u
#define TRIPLE_BUFFERING_DEPTH_BUFFER 0x802EA000u

u16 **nuGfxCfb;
u16 *nuGfxCfb_ptr;
u32 nuGfxCfbNum;
u32 nuGfxCfbCounter;
u16 *nuGfxZBuffer;
volatile u32 nuGfxTaskSpool;
u32 nuGfxDisplay;
u16 *g_DepthBuffer = (u16 *)PcRdramAddress(DOUBLE_BUFFERING_INITIAL_DEPTH_BUFFER);

// The task timings that the game's performance meter reads, which the port
// doesn't measure
static NUDebTaskPerf f_UnmeasuredPerformance;
NUDebTaskPerf *nuDebTaskPerfPtr = &f_UnmeasuredPerformance;

static u8 f_NextFramebuffer[FRAMEBUFFER_SLOT_COUNT];
static u8 f_SpareFramebuffer = NO_SPARE_FRAMEBUFFER;

static NUGfxFunc f_RetraceFunction;
static NUGfxPreNMIFunc f_PreNmiFunction;

static OSThread f_GfxThread;
static OSMesgQueue f_GfxQueue;
static OSMesg f_GfxMessages[GFX_QUEUE_LENGTH];
static NUScClient f_GfxClient;

static OSThread f_TaskManagerThread;
static OSMesgQueue f_FinishedTaskQueue;
static OSMesg f_FinishedTaskMessages[TASK_QUEUE_LENGTH];
static OSMesgQueue f_TaskManagerRetraceQueue;
static OSMesg f_TaskManagerRetraceMessages[GFX_QUEUE_LENGTH];
static NUScClient f_TaskManagerClient;
static u32 f_LastSwapRetrace;

static NUScTask f_Tasks[NU_GFX_TASK_NUM];
static int f_NextTaskIndex;

// NuSystem's own frame buffers, until the game sets its own
static u16 *f_InitialFramebuffers[FRAMEBUFFER_SLOT_COUNT];

static void RunGfxThread(void *argument) {
    NUScMsg *message;

    (void)argument;
    while (TRUE) {
        osRecvMesg(&f_GfxQueue, (OSMesg *)&message, OS_MESG_BLOCK);
        if (*message == NU_SC_RETRACE_MSG) {
            if (f_RetraceFunction != NULL) {
                f_RetraceFunction(nuGfxTaskSpool);
            }
        }
        else if (*message == NU_SC_PRENMI_MSG) {
            if (f_PreNmiFunction != NULL) {
                f_PreNmiFunction();
            }
        }
    }
}

// Blocks until SWAP_RETRACE_MIN retraces have passed since the last swap.
// Retraces that came while the task manager was busy are still in its
// queue, but the counter, not the messages, says when it may go on.
static void WaitToSwap(void) {
    OSMesg message;

    while (nuScRetraceCounter - f_LastSwapRetrace < SWAP_RETRACE_MIN) {
        osRecvMesg(&f_TaskManagerRetraceQueue, &message, OS_MESG_BLOCK);
    }

    f_LastSwapRetrace = nuScRetraceCounter;
}

static void RetireTask(NUScTask *task) {
    if (task->flags & NU_SC_SWAPBUFFER) {
        WaitToSwap();
        osViSwapBuffer(task->framebuffer);
        nuGfxTaskSpool--;
        if (nuGfxDisplay & NU_GFX_DISPLAY_ON_TRIGGER) {
            osViBlack(FALSE);
            nuGfxDisplay = NU_GFX_DISPLAY_ON;
        }
    }
    else {
        nuGfxTaskSpool--;
    }
}

static void RunTaskManagerThread(void *argument) {
    NUScTask *task;

    (void)argument;
    while (TRUE) {
        osRecvMesg(&f_FinishedTaskQueue, (OSMesg *)&task, OS_MESG_BLOCK);
        RetireTask(task);
    }
}

void nuGfxInitEX2(void) {
    int i;

    for (i = 0; i < FRAMEBUFFER_SLOT_COUNT; i++) {
        f_InitialFramebuffers[i] = (u16 *)PcRdramAddress(
            NU_GFX_FRAMEBUFFER_ADDR +
            i * NU_GFX_INIT_SCREEN_WD * NU_GFX_INIT_SCREEN_HT * sizeof(u16)
        );
    }

    nuGfxZBuffer = (u16 *)PcRdramAddress(NU_GFX_ZBUFFER_ADDR);
    nuGfxSetCfb(f_InitialFramebuffers, NU_GFX_FRAMEBUFFER_NUM);
    nuGfxDisplayOff();

    osCreateMesgQueue(&f_FinishedTaskQueue, f_FinishedTaskMessages, TASK_QUEUE_LENGTH);
    osCreateMesgQueue(&f_TaskManagerRetraceQueue, f_TaskManagerRetraceMessages, GFX_QUEUE_LENGTH);
    nuScAddClient(&f_TaskManagerClient, &f_TaskManagerRetraceQueue, NU_SC_RETRACE_MSG);
    osCreateThread(
        &f_TaskManagerThread,
        NU_GFX_TASKMGR_THREAD_ID,
        RunTaskManagerThread,
        NULL,
        NULL,
        NU_GFX_TASKMGR_THREAD_PRI
    );
    osStartThread(&f_TaskManagerThread);

    osCreateMesgQueue(&f_GfxQueue, f_GfxMessages, GFX_QUEUE_LENGTH);
    nuScAddClient(&f_GfxClient, &f_GfxQueue, NU_SC_RETRACE_MSG | NU_SC_PRENMI_MSG);
    osCreateThread(&f_GfxThread, NU_GFX_THREAD_ID, RunGfxThread, NULL, NULL, NU_GFX_THREAD_PRI);
    osStartThread(&f_GfxThread);
}

void nuGfxFuncSet(NUGfxFunc function) {
    f_RetraceFunction = function;
}

void nuGfxPreNMIFuncSet(NUGfxPreNMIFunc function) {
    f_PreNmiFunction = function;
}

// Moves the frame buffer to draw into along the ring
static void AdvanceFramebuffer(void) {
    nuGfxCfbCounter = f_NextFramebuffer[nuGfxCfbCounter];
    nuGfxCfb_ptr = nuGfxCfb[nuGfxCfbCounter];
}

void nuGfxTaskStart(Gfx *displayList, u32 size, u32 ucode, u32 flags) {
    NUScTask *task = &f_Tasks[f_NextTaskIndex];

    (void)ucode;
    f_NextTaskIndex = (f_NextTaskIndex + 1) % NU_GFX_TASK_NUM;
    task->flags = flags & 0xFFFF;
    task->framebuffer = nuGfxCfb_ptr;
    task->list.t.data_ptr = (u64 *)displayList;
    task->list.t.data_size = size;
    if (flags & NU_SC_SWAPBUFFER) {
        AdvanceFramebuffer();
    }

    nuGfxTaskSpool++;
    PcRunGfxTask(displayList, size);
    osSendMesg(&f_FinishedTaskQueue, task, OS_MESG_BLOCK);
}

void nuGfxTaskAllEndWait(void) {
    while (nuGfxTaskSpool != 0) {
        osPcWaitForInterrupt();
    }
}

void nuGfxRetraceWait(u32 retraceCount) {
    u32 start = nuScRetraceCounter;

    while (nuScRetraceCounter - start < retraceCount) {
        osPcWaitForInterrupt();
    }
}

void nuGfxDisplayOff(void) {
    nuGfxDisplay = NU_GFX_DISPLAY_OFF;
    osViBlack(TRUE);
}

// The screen comes on with the next frame shown
void nuGfxDisplayOn(void) {
    nuGfxDisplay = NU_GFX_DISPLAY_ON_TRIGGER;
}

void nuGfxSetCfb(u16 **framebuffers, u32 count) {
    nuGfxCfb = framebuffers;
    nuGfxCfbNum = count;
    nuGfxCfbCounter = 0;
    nuGfxCfb_ptr = framebuffers[0];
    if (count == TRIPLE_BUFFERING) {
        g_DepthBuffer = (u16 *)PcRdramAddress(TRIPLE_BUFFERING_DEPTH_BUFFER);
        f_NextFramebuffer[0] = 1;
        f_NextFramebuffer[1] = 2;
        f_NextFramebuffer[2] = 0;
        f_SpareFramebuffer = NO_SPARE_FRAMEBUFFER;
    }
    else {
        g_DepthBuffer = framebuffers[2];
        f_NextFramebuffer[0] = 1;
        f_NextFramebuffer[1] = 0;
        f_NextFramebuffer[2] = NO_SPARE_FRAMEBUFFER;
        f_SpareFramebuffer = 2;
    }
}

// Puts the spare frame buffer back into the ring, to be drawn into next,
// and takes the depth buffer to its fixed address. The ring's other two
// keep their order, so the frames shown don't change.
static void JoinSpareFramebuffer(void) {
    u8 drawing = (u8)nuGfxCfbCounter;
    u8 other = f_NextFramebuffer[drawing];
    u8 spare = f_SpareFramebuffer;

    f_NextFramebuffer[other] = spare;
    f_NextFramebuffer[spare] = drawing;
    f_SpareFramebuffer = NO_SPARE_FRAMEBUFFER;
    nuGfxCfbCounter = spare;
    nuGfxCfb_ptr = nuGfxCfb[spare];
    g_DepthBuffer = (u16 *)PcRdramAddress(TRIPLE_BUFFERING_DEPTH_BUFFER);
}

// Takes the frame buffer about to be drawn into out of the ring to hold
// the depth, and draws into the next one instead
static void SpareFramebufferForDepth(void) {
    u8 spare = (u8)nuGfxCfbCounter;
    u8 drawing = f_NextFramebuffer[spare];
    u8 other = f_NextFramebuffer[drawing];

    f_NextFramebuffer[other] = drawing;
    f_SpareFramebuffer = spare;
    nuGfxCfbCounter = drawing;
    nuGfxCfb_ptr = nuGfxCfb[drawing];
    g_DepthBuffer = nuGfxCfb[spare];
}

void SetFramebufferCount(u32 count) {
    if (nuGfxCfbNum == count) {
        return;
    }

    nuGfxCfbNum = count;
    if (count == TRIPLE_BUFFERING) {
        JoinSpareFramebuffer();
    }
    else {
        SpareFramebufferForDepth();
    }
}
