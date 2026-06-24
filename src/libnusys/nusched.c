/*
 * The scheduler: this game's customized version of the nusys RCP scheduler. It
 * owns the retrace/pre-NMI event handler thread plus the audio and graphics
 * task threads, and arbitrates the single RSP between them (audio can preempt a
 * running graphics task by yielding it). A broadcast mechanism relays retrace
 * and pre-NMI events to registered clients. Where this diverges from stock
 * nusys -- the TV-type handling, the frame-counted VI swap / display-off timing
 * in the event handler, and the game callbacks in the graphics thread -- the
 * comments describe what this code actually does, not the library contract.
 */

#define NU_DEBUG
#include <nusys.h>
#include "include_asm.h"

// RSP graphics-task yield states (see nuScExecuteAudio).
#define TASK_YIELD 1
#define TASK_YIELDED 2

// Tags identifying each event source on the event-handler queue.
#define VIDEO_MSG 666
#define RSP_DONE_MSG 667
#define RDP_DONE_MSG 668
#define PRE_NMI_MSG 669

extern NUDebTaskPerf* debTaskPerfPtr;

// Game-relocated scheduler globals (one declaration per line, comments
// aligned).
// clang-format off: keep each extern on its own line with an aligned comment
extern vu32 D_801B68E0;   // free-running retrace tick (debug/timing)
extern vs32 D_800B678C;   // perf: a swap occurred, latch retrace time
extern vu32 D_8012F4D4;   // retrace count when the current gfx task started
extern void* D_800B6784;  // the in-flight gfx task (for the swap timer), or NULL
extern vu32 D_800B6788;   // perf double-buffer index
extern s8 D_800B67C0;     // gate for the frame-0x36 display-off step
extern u32 D_800B6790;    // game TV-type code (1=MPAL 2=PAL 3=NTSC 4=other)
// clang-format on
extern u64 nuScStack[NU_SC_STACK_SIZE / sizeof(u64)];
extern u64 nuScAudioStack[NU_SC_STACK_SIZE / sizeof(u64)];
extern u64 nuScGraphicsStack[NU_SC_STACK_SIZE / sizeof(u64)];
extern volatile u32 nuScRetraceCounter;

void nuScEventHandler(void);
void nuScExecuteAudio(void);
void nuScExecuteGraphics(void);

// Game callbacks run after a buffer-swapping graphics task completes.
extern void func_8008D1DC(void);
extern void func_80092324(void* framebuffer);

/*
 * Initialize the scheduler and bring up its three threads. videoMode selects
 * the VI mode table entry; numFields is the retrace notification cycle (1 =
 * every field/60Hz, 2 = every other, ...). Called once at startup.
 */
void nuScCreateScheduler(u8 videoMode, u8 numFields) {
  // Seed the scheduler state: the two broadcast message values, the retrace
  // divisor, no tasks running, an empty client list, double buffering.
  nusched.retraceMsg = NU_SC_RETRACE_MSG;
  nusched.prenmiMsg = NU_SC_PRENMI_MSG;
  nusched.retraceCount = numFields;
  nusched.curGraphicsTask = NULL;
  nusched.curAudioTask = NULL;
  nusched.graphicsTaskSuspended = NULL;
  nusched.clientList = NULL;
  nusched.frameBufferNum = 2;

  // This game ships only for NTSC and MPAL; any other TV type hangs here
  // rather than running with an unsupported video timing.
  if (osTvType != OS_TV_NTSC && osTvType != OS_TV_MPAL) {
    while (1) {
      ;
    }
  }
  if (osTvType == OS_TV_MPAL) {
    videoMode = OS_VI_MPAL_LAN1;
  }

  // Record a game-internal TV-type code used elsewhere in the title.
  switch (osTvType) {
    case OS_TV_MPAL:
      D_800B6790 = 1;
      break;
    case OS_TV_PAL:
      D_800B6790 = 2;
      break;
    case OS_TV_NTSC:
      D_800B6790 = 3;
      break;
    default:
      D_800B6790 = 4;
      break;
  }

  // PAL refreshes at 50 Hz, the rest at 60 Hz.
  if (osTvType == OS_TV_PAL) {
    nusched.frameRate = 50;
  } else {
    nusched.frameRate = 60;
  }
  nuScPreNMIFlag = NU_SC_PRENMI_YET;

  // Debug builds keep a rolling per-frame performance log; prime the first
  // buffer and clear the RDP hardware counters.
#ifdef NU_DEBUG
  debTaskPerfPtr = &nuDebTaskPerf[0];
  debTaskPerfPtr->retraceTime = 0;
  debTaskPerfPtr->auTaskCnt = 0;
  debTaskPerfPtr->gfxTaskCnt = 0;
  nuDebTaskPerfPtr = debTaskPerfPtr;
  osDpSetStatus(DPC_CLR_TMEM_CTR | DPC_CLR_PIPE_CTR | DPC_CLR_CMD_CTR |
                DPC_CLR_CLOCK_CTR);
#endif

  // Message queues: retrace/pre-NMI events, RSP and RDP completion, the audio
  // and graphics task request inboxes, and the gfx-thread retrace wait queue.
  osCreateMesgQueue(&nusched.retraceMQ, nusched.retraceMsgBuf, NU_SC_MAX_MESGS);
  osCreateMesgQueue(&nusched.rspMQ, nusched.rspMsgBuf, NU_SC_MAX_MESGS);
  osCreateMesgQueue(&nusched.rdpMQ, nusched.rdpMsgBuf, NU_SC_MAX_MESGS);
  osCreateMesgQueue(&nusched.graphicsRequestMQ, nusched.graphicsRequestBuf,
                    NU_SC_MAX_MESGS);
  osCreateMesgQueue(&nusched.audioRequestMQ, nusched.audioRequestBuf,
                    NU_SC_MAX_MESGS);
  osCreateMesgQueue(&nusched.waitMQ, nusched.waitMsgBuf, NU_SC_MAX_MESGS);

  // Bring up VI in the selected mode (blanked) and wire the four hardware
  // events to the scheduler's queues with their identifying tags.
  osCreateViManager(OS_PRIORITY_VIMGR);
  osViSetMode(&osViModeTable[videoMode]);
  osViBlack(TRUE);
  osViSetEvent(&nusched.retraceMQ, (OSMesg)VIDEO_MSG, numFields);
  osSetEventMesg(OS_EVENT_SP, &nusched.rspMQ, (OSMesg)RSP_DONE_MSG);
  osSetEventMesg(OS_EVENT_DP, &nusched.rdpMQ, (OSMesg)RDP_DONE_MSG);
  osSetEventMesg(OS_EVENT_PRENMI, &nusched.retraceMQ, (OSMesg)PRE_NMI_MSG);

  // Three threads, by descending priority: event handler, audio, graphics.
  osCreateThread(&nusched.schedulerThread, 19,
                 (void (*)(void*))nuScEventHandler, (void*)&nusched,
                 nuScStack + (NU_SC_STACK_SIZE / sizeof(u64)),
                 NU_SC_HANDLER_PRI);
  osStartThread(&nusched.schedulerThread);
  osCreateThread(&nusched.audioThread, 18, (void (*)(void*))nuScExecuteAudio,
                 (void*)&nusched,
                 nuScAudioStack + (NU_SC_STACK_SIZE / sizeof(u64)),
                 NU_SC_AUDIO_PRI);
  osStartThread(&nusched.audioThread);
  osCreateThread(&nusched.graphicsThread, 17,
                 (void (*)(void*))nuScExecuteGraphics, (void*)&nusched,
                 nuScGraphicsStack + (NU_SC_STACK_SIZE / sizeof(u64)),
                 NU_SC_GRAPHICS_PRI);
  osStartThread(&nusched.graphicsThread);
}

/* Return the audio / graphics task request queues so callers can submit tasks.
 */
OSMesgQueue* nuScGetAudioMQ(void) { return (&nusched.audioRequestMQ); }
OSMesgQueue* nuScGetGfxMQ(void) { return (&nusched.graphicsRequestMQ); }

/*
 * Highest-priority thread: receives every hardware event and turns it into the
 * retrace tick, the client broadcasts, the game's frame-timed VI swap/blank,
 * and the pre-NMI reset countdown.
 */
void nuScEventHandler(void) {
  OSMesg msg;
  s32 beforeResetFrame;

  nuScRetraceCounter = 0;
  D_801B68E0 = 0;
  while (1) {
    osRecvMesg(&nusched.retraceMQ, &msg, OS_MESG_BLOCK);
    switch ((int)msg) {
      case VIDEO_MSG:
        // Advance the retrace counters on every vertical retrace.
        nuScRetraceCounter++;
        D_801B68E0++;

        // On the frame after a buffer swap, latch the retrace timestamp into
        // the perf log and reset that buffer's audio-task count.
        if (D_800B678C != 0) {
          debTaskPerfPtr->retraceTime = OS_CYCLES_TO_USEC(osGetTime());
          nuDebTaskPerf[D_800B6788].auTaskCnt = 0;
          D_800B678C = 0;
        }

        // Game-specific deferred VI handling, measured in retraces since the
        // current graphics task started: at frame 0x20 swap in that task's
        // framebuffer (the pointer stored 0xc into the task); at frame 0x36
        // (unless gated) blank the display and forget the task. The >= 0x1F
        // guard skips the whole check until enough frames have elapsed.
        if ((nuScRetraceCounter - D_8012F4D4) >= 0x1F) {
          s32 frame = nuScRetraceCounter - D_8012F4D4;
          if (D_800B6784 != NULL) {
            if (frame == 0x20) {
              osViSwapBuffer(*(void**)((u8*)D_800B6784 + 0xc));
            } else if (frame == 0x36 && D_800B67C0 == 0) {
              nuGfxDisplayOff();
              D_800B6784 = NULL;
            }
          }
        }

        // Relay the retrace to all retrace clients.
        nuScEventBroadcast(&nusched.retraceMsg);

        // If a pre-NMI is pending, count down the grace frames, then enter the
        // before-reset state: stop tasks, restore VI scale, and blank.
        if (nuScPreNMIFlag) {
          if (beforeResetFrame) {
            beforeResetFrame--;
          } else {
            nuScPreNMIFlag |= NU_SC_BEFORE_RESET;
            osAfterPreNMI();
            osViSetYScale(1.0);
            osViBlack(TRUE);
          }
        }
        break;

      case PRE_NMI_MSG:
        // Reset requested: record it, notify clients and the user hook, and
        // arm the grace period (half a second, in retrace-cycle units, less 3).
        nuScPreNMIFlag = NU_SC_PRENMI_GET;
        nuScEventBroadcast(&nusched.prenmiMsg);
        if (nuScPreNMIFunc != NULL) {
          (*nuScPreNMIFunc)();
        }
        beforeResetFrame = ((nusched.frameRate / 2) / nusched.retraceCount) - 3;
        break;

      default:
        break;
    }
  }
}

/*
 * Register a client to receive retrace and/or pre-NMI broadcasts on msgQ.
 * Pushed onto the head of the client list. If pre-NMI was requested and one has
 * already happened, the client is told immediately so it can't miss the reset.
 */
void nuScAddClient(NUScClient* client, OSMesgQueue* msgQ, NUScMsg msgType) {
  OSIntMask mask;

  mask = osSetIntMask(OS_IM_NONE);
  client->msgQ = msgQ;
  client->next = nusched.clientList;
  client->msgType = msgType;
  nusched.clientList = client;
  if ((msgType & NU_SC_PRENMI_MSG) && nuScPreNMIFlag) {
    osSendMesg(msgQ, (OSMesg*)&nusched.prenmiMsg, OS_MESG_NOBLOCK);
  }
  osSetIntMask(mask);
}

/* Change which event types a registered client subscribes to. */
void nuScResetClientMesgType(NUScClient* client, NUScMsg msgType) {
  OSIntMask mask;

  mask = osSetIntMask(OS_IM_NONE);
  client->msgType = msgType;
  osSetIntMask(mask);
}

/* Unlink client c from the broadcast list, handling the head-of-list case. */
void nuScRemoveClient(NUScClient* c) {
  NUScClient* client;
  NUScClient* prev;
  OSIntMask mask;

  mask = osSetIntMask(OS_IM_NONE);
  client = nusched.clientList;
  prev = 0;
  while (client != 0) {
    if (client == c) {
      if (prev) {
        prev->next = c->next;
      } else {
        nusched.clientList = c->next;
      }
      break;
    }
    prev = client;
    client = client->next;
  }
  osSetIntMask(mask);
}

/*
 * Send *msg to every client subscribed to that message type. Non-blocking so a
 * client with a full queue is skipped rather than stalling the event handler.
 */
void nuScEventBroadcast(NUScMsg* msg) {
  NUScClient* client;

  for (client = nusched.clientList; client != 0; client = client->next) {
    if (client->msgType & *msg) {
      osSendMesg(client->msgQ, (OSMesg*)msg, OS_MESG_NOBLOCK);
    }
  }
}

/*
 * Audio task thread. Audio has priority on the RSP: if a graphics task is
 * running it is yielded first, the audio task runs to completion, and the
 * graphics task is then resumed. Each request is acknowledged on its own queue.
 */
void nuScExecuteAudio(void) {
  NUScTask* gfxTask;
  NUScTask* audioTask;
  OSMesg msg;
  u32 yieldFlag;
#ifdef NU_DEBUG
  OSIntMask mask;
#endif
  while (1) {
    osRecvMesg(&nusched.audioRequestMQ, (OSMesg*)&audioTask, OS_MESG_BLOCK);

    // During the pre-reset window, don't touch the RSP; just acknowledge.
    if (nuScPreNMIFlag & NU_SC_BEFORE_RESET) {
      osSendMesg(audioTask->msgQ, audioTask->msg, OS_MESG_BLOCK);
      continue;
    }
    osWritebackDCacheAll();
    yieldFlag = 0;

    // If graphics is on the RSP, yield it and wait for the RSP to report back.
    // Whether it actually yielded (saved state to resume) or had already
    // finished decides how it gets resumed after the audio task.
    gfxTask = nusched.curGraphicsTask;
    if (gfxTask) {
      osSpTaskYield();
      osRecvMesg(&nusched.rspMQ, &msg, OS_MESG_BLOCK);
      if (osSpTaskYielded(&gfxTask->list)) {
        yieldFlag = TASK_YIELD;
      } else {
        yieldFlag = TASK_YIELDED;
      }
    }
#ifdef NU_DEBUG
    mask = osSetIntMask(OS_IM_NONE);
    if (debTaskPerfPtr->auTaskCnt < NU_DEB_PERF_AUTASK_CNT) {
      debTaskPerfPtr->auTaskTime[debTaskPerfPtr->auTaskCnt].rspStart =
          OS_CYCLES_TO_USEC(osGetTime());
    }
    osSetIntMask(mask);
#endif

    // Run the audio task to completion.
    nusched.curAudioTask = audioTask;
    osSpTaskStart(&audioTask->list);
    osRecvMesg(&nusched.rspMQ, &msg, OS_MESG_BLOCK);
    nusched.curAudioTask = (NUScTask*)NULL;
#ifdef NU_DEBUG
    mask = osSetIntMask(OS_IM_NONE);
    if (debTaskPerfPtr->auTaskCnt < NU_DEB_PERF_AUTASK_CNT) {
      debTaskPerfPtr->auTaskTime[debTaskPerfPtr->auTaskCnt].rspEnd =
          OS_CYCLES_TO_USEC(osGetTime());
      debTaskPerfPtr->auTaskCnt++;
    }
    osSetIntMask(mask);
#endif

    // If the graphics thread parked itself waiting on us, release it.
    if (nusched.graphicsTaskSuspended) {
      osSendMesg(&nusched.waitMQ, &msg, OS_MESG_BLOCK);
    }

    // Resume the preempted graphics task: restart it if it genuinely yielded,
    // or just re-post its completion if it had already finished.
    if (yieldFlag == TASK_YIELD) {
      osSpTaskStart(&gfxTask->list);
    } else if (yieldFlag == TASK_YIELDED) {
      osSendMesg(&nusched.rspMQ, &msg, OS_MESG_BLOCK);
    }
    osSendMesg(audioTask->msgQ, audioTask->msg, OS_MESG_BLOCK);
  }
}

/* Game leaf that always reports ready/true. */
s32 func_80028D28(void) { return 1; }

/*
 * Graphics task thread. Waits until the target framebuffer is free, defers to
 * any running audio task, runs the graphics task on the RSP (and waits for the
 * RDP unless the task opts out), records perf data, and on a buffer-swapping
 * task runs the game's post-swap callbacks before acknowledging.
 */
void nuScExecuteGraphics(void) {
  OSMesg msg;
  NUScTask* gfxTask;
  OSIntMask mask;

  while (1) {
    osRecvMesg(&nusched.graphicsRequestMQ, (OSMesg*)&gfxTask, OS_MESG_BLOCK);

    // During the pre-reset window, don't touch the RSP; just acknowledge.
    if (nuScPreNMIFlag & NU_SC_BEFORE_RESET) {
      osSendMesg(gfxTask->msgQ, (OSMesg)gfxTask, OS_MESG_BLOCK);
      continue;
    }

    // Wait until the framebuffer this task draws to is no longer on screen.
    nuScWaitTaskReady(gfxTask);

    // Audio owns the RSP: if an audio task is running, mark ourselves suspended
    // and sleep on the wait queue until the audio thread releases us.
    mask = osSetIntMask(OS_IM_NONE);
    while (nusched.curAudioTask) {
      nusched.graphicsTaskSuspended = gfxTask;
      osSetIntMask(mask);
      osRecvMesg(&nusched.waitMQ, &msg, OS_MESG_BLOCK);
      mask = osSetIntMask(OS_IM_NONE);
      nusched.graphicsTaskSuspended = (NUScTask*)NULL;
    }
#ifdef NU_DEBUG
    if (debTaskPerfPtr->gfxTaskCnt < NU_DEB_PERF_GFXTASK_CNT) {
      debTaskPerfPtr->gfxTaskTime[debTaskPerfPtr->gfxTaskCnt].rspStart =
          OS_CYCLES_TO_USEC(osGetTime());
    }
#endif

    // Start the task and record the start retrace and task pointer that the
    // event handler's frame-timed swap/blank logic keys off of.
    nusched.curGraphicsTask = gfxTask;
    D_8012F4D4 = nuScRetraceCounter;
    D_800B6784 = gfxTask;
    osSpTaskLoad(&gfxTask->list);
    osSpTaskStartGo(&gfxTask->list);
    osSetIntMask(mask);

    // Wait for the RSP to finish the task.
    mask = osSetIntMask(OS_IM_NONE);
    osRecvMesg(&nusched.rspMQ, &msg, OS_MESG_BLOCK);
    nusched.curGraphicsTask = (NUScTask*)NULL;
    osSetIntMask(mask);
    mask = osSetIntMask(OS_IM_NONE);
#ifdef NU_DEBUG
    if (debTaskPerfPtr->gfxTaskCnt < NU_DEB_PERF_GFXTASK_CNT) {
      debTaskPerfPtr->gfxTaskTime[debTaskPerfPtr->gfxTaskCnt].rspEnd =
          OS_CYCLES_TO_USEC(osGetTime());
    }
#endif
    osSetIntMask(mask);

    // Unless the task suppresses RDP use, wait for the RDP to drain too.
    if (!(gfxTask->flags & NU_SC_NORDP)) {
      osRecvMesg(&nusched.rdpMQ, &msg, OS_MESG_BLOCK);
    }
    D_800B6784 = NULL;
    mask = osSetIntMask(OS_IM_NONE);
#ifdef NU_DEBUG
    // Log RDP end time and counters (or zeros for a NORDP task).
    if (debTaskPerfPtr->gfxTaskCnt < NU_DEB_PERF_GFXTASK_CNT) {
      if (gfxTask->flags & NU_SC_NORDP) {
        debTaskPerfPtr->gfxTaskTime[debTaskPerfPtr->gfxTaskCnt].rdpEnd =
            debTaskPerfPtr->gfxTaskTime[debTaskPerfPtr->gfxTaskCnt].rspStart;
        debTaskPerfPtr->gfxTaskTime[debTaskPerfPtr->gfxTaskCnt].dpCnt[0] = 0;
      } else {
        debTaskPerfPtr->gfxTaskTime[debTaskPerfPtr->gfxTaskCnt].rdpEnd =
            OS_CYCLES_TO_USEC(osGetTime());
        osDpGetCounters(
            debTaskPerfPtr->gfxTaskTime[debTaskPerfPtr->gfxTaskCnt].dpCnt);
        osDpSetStatus(DPC_CLR_TMEM_CTR | DPC_CLR_PIPE_CTR | DPC_CLR_CMD_CTR |
                      DPC_CLR_CLOCK_CTR);
      }
      debTaskPerfPtr->gfxTaskCnt++;
    }

    // A swap closes the frame: rotate to the next perf log buffer and flag that
    // the event handler should latch the swap timestamp next retrace.
    if (gfxTask->flags & NU_SC_SWAPBUFFER) {
      D_800B6788++;
      D_800B6788 %= NU_DEB_PERF_BUF_NUM;
      nuDebTaskPerfPtr = debTaskPerfPtr;
      nuDebTaskPerf[D_800B6788].gfxTaskCnt = 0;
      nuDebTaskPerf[D_800B6788].retraceTime = 0;
      debTaskPerfPtr = &nuDebTaskPerf[D_800B6788];
      D_800B678C++;
    }
#endif
    osSetIntMask(mask);

    // On a swapping task, run the game's post-frame hooks before acknowledging.
    if (gfxTask->flags & NU_SC_SWAPBUFFER) {
      func_8008D1DC();
      func_80092324(gfxTask->framebuffer);
    }
    osSendMesg(gfxTask->msgQ, (OSMesg)gfxTask, OS_MESG_BLOCK);
  }
}

/*
 * Block until the task's target framebuffer is safe to draw into, i.e. it is
 * neither the framebuffer currently displayed nor the one queued next. Skipped
 * in single-buffer mode. Uses a temporary retrace client to wake per frame.
 */
void nuScWaitTaskReady(NUScTask* task) {
  NUScClient client;
  void* fb = task->framebuffer;

  if (nusched.frameBufferNum == 1) {
    return;
  }
  nuScAddClient(&client, &nusched.waitMQ, NU_SC_RETRACE_MSG);
  while (osViGetCurrentFramebuffer() == fb || osViGetNextFramebuffer() == fb) {
    osRecvMesg(&nusched.waitMQ, NULL, OS_MESG_BLOCK);
  }
  nuScRemoveClient(&client);
}

/* Select single- or double-buffered operation (changes nuScWaitTaskReady). */
void nuScSetFrameBufferNum(u8 frameBufferNum) {
  nusched.frameBufferNum = frameBufferNum;
}

/* Return the display refresh rate (50 for PAL, else 60). */
s32 nuScGetFrameRate(void) { return nusched.frameRate; }
