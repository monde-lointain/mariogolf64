/*
 * RCP task scheduler: a single VI-retrace-driven thread that multiplexes the
 * signal processor (RSP) and display processor (RDP) across the graphics and
 * audio work the game submits each frame.
 *
 * Submitters hand OSScTasks to the command queue (osScGetCmdQ); the scheduler
 * thread drains that queue at each retrace, decides which task may own the RSP
 * and/or RDP next, launches them, and notifies registered clients (and the
 * submitter) by message when a retrace happens or a task completes. Audio takes
 * priority over graphics within a frame, and a long graphics task can be
 * yielded so audio is never starved.
 */
#include <ultralog.h>
#include <assert.h>
#include <sched.h>

// Interrupt-source identifiers posted to the scheduler's interruptQ; the value
// received tells __scMain which event fired.
#define VIDEO_MSG 666
#define RSP_DONE_MSG 667
#define RDP_DONE_MSG 668
#define PRE_NMI_MSG 669

// OSScTask->state bits: which RCP units a task still needs, and yield progress.
#define OS_SC_DP 0x0001       // task still needs the RDP
#define OS_SC_SP 0x0002       // task still needs the RSP
#define OS_SC_YIELD 0x0010    // a yield was requested of this task
#define OS_SC_YIELDED 0x0020  // the task has finished yielding

// OSScTask->flags task-type identifiers, expressed as the set of RCP units the
// type drives (XBUS feeds the RDP directly from the RSP; DRAM stages the
// display list through memory).
#define OS_SC_XBUS (OS_SC_SP | OS_SC_DP)
#define OS_SC_DRAM (OS_SC_SP | OS_SC_DP | OS_SC_DRAM_DLIST)
#define OS_SC_DP_XBUS (OS_SC_SP)
#define OS_SC_DP_DRAM (OS_SC_SP | OS_SC_DRAM_DLIST)
#define OS_SC_SP_XBUS (OS_SC_DP)
#define OS_SC_SP_DRAM (OS_SC_DP | OS_SC_DRAM_DLIST)

// Private scheduler-thread routines (declared up front so the API functions can
// reference the thread entry point).
static void __scMain(void* arg);
static void __scHandleRetrace(OSSched* s);
static void __scHandleRSP(OSSched* s);
static void __scHandleRDP(OSSched* s);
static void __scAppendList(OSSched* s, OSScTask* t);
OSScTask* __scTaskReady(OSScTask* t);
static s32 __scTaskComplete(OSSched* s, OSScTask* t);
static void __scExec(OSSched* sc, OSScTask* sp, OSScTask* dp);
static void __scYield(OSSched* s);
static s32 __scSchedule(OSSched* sc, OSScTask** sp, OSScTask** dp,
                        s32 availRCP);

// Optional event log, compiled in only for profiling builds.
#ifdef SC_LOGGING
#define SC_LOG_LEN 32 * 1024
static OSLog scLog;
static OSLog* l = &scLog;
static u32 logArray[SC_LOG_LEN / sizeof(u32)];
#endif

/*
 * Initialize a scheduler and start its thread.
 *
 * Clears the task/client bookkeeping, creates the interrupt and command queues,
 * and routes the VI, RSP, RDP, and pre-NMI events to the interrupt queue so the
 * thread is woken for each. mode selects the entry in osViModeTable; numFields
 * sets how many fields elapse between retrace messages.
 */
void osCreateScheduler(OSSched* sc, void* stack, OSPri priority, u8 mode,
                       u8 numFields) {
  sc->curRSPTask = 0;
  sc->curRDPTask = 0;
  sc->clientList = 0;
  sc->frameCount = 0;
  sc->audioListHead = 0;
  sc->gfxListHead = 0;
  sc->audioListTail = 0;
  sc->gfxListTail = 0;
  sc->retraceMsg.type = OS_SC_RETRACE_MSG;
  sc->prenmiMsg.type = OS_SC_PRE_NMI_MSG;

  osCreateMesgQueue(&sc->interruptQ, sc->intBuf, OS_SC_MAX_MESGS);
  osCreateMesgQueue(&sc->cmdQ, sc->cmdMsgBuf, OS_SC_MAX_MESGS);

  // Bring up the video manager and funnel the VI/RSP/RDP/pre-NMI events into
  // the single interrupt queue the scheduler thread waits on.
  osCreateViManager(OS_PRIORITY_VIMGR);
  osViSetMode(&osViModeTable[mode]);
  osViBlack(TRUE);
  osSetEventMesg(OS_EVENT_SP, &sc->interruptQ, (OSMesg)RSP_DONE_MSG);
  osSetEventMesg(OS_EVENT_DP, &sc->interruptQ, (OSMesg)RDP_DONE_MSG);
  osSetEventMesg(OS_EVENT_PRENMI, &sc->interruptQ, (OSMesg)PRE_NMI_MSG);
  osViSetEvent(&sc->interruptQ, (OSMesg)VIDEO_MSG, numFields);

#ifdef SC_LOGGING
  osCreateLog(l, logArray, sizeof(logArray));
#endif

  osCreateThread(&sc->thread, 4, __scMain, (void*)sc, stack, priority);
  osStartThread(&sc->thread);
}

/*
 * Register a client to receive retrace and pre-NMI messages. Runs with
 * interrupts masked because the scheduler thread also walks clientList.
 */
void osScAddClient(OSSched* sc, OSScClient* c, OSMesgQueue* msgQ) {
  OSIntMask mask;
  mask = osSetIntMask(OS_IM_NONE);
  c->msgQ = msgQ;
  c->next = sc->clientList;
  sc->clientList = c;
  osSetIntMask(mask);
}

/*
 * Unlink a client from the notification list. Interrupts are masked while the
 * shared list is edited.
 */
void osScRemoveClient(OSSched* sc, OSScClient* c) {
  OSScClient* client = sc->clientList;
  OSScClient* prev = 0;
  OSIntMask mask;
  mask = osSetIntMask(OS_IM_NONE);
  while (client != 0) {
    if (client == c) {
      if (prev)
        prev->next = c->next;
      else
        sc->clientList = c->next;
      break;
    }
    prev = client;
    client = client->next;
  }
  osSetIntMask(mask);
}

// Hand back the queue callers post OSScTasks to.
OSMesgQueue* osScGetCmdQ(OSSched* sc) { return &sc->cmdQ; }

/*
 * Scheduler thread body: block on the interrupt queue forever and dispatch each
 * event to its handler. A pre-NMI event is broadcast straight to every client
 * so they can shut down before the reset.
 */
static void __scMain(void* arg) {
  OSMesg msg;
  OSSched* sc = (OSSched*)arg;
  OSScClient* client;
  static int count = 0;
  while (1) {
    osRecvMesg(&sc->interruptQ, (OSMesg*)&msg, OS_MESG_BLOCK);
#ifdef SC_LOGGING
    if (++count % 1024 == 0) osFlushLog(l);
#endif
    switch ((int)msg) {
      case (VIDEO_MSG):
        __scHandleRetrace(sc);
        break;
      case (RSP_DONE_MSG):
        __scHandleRSP(sc);
        break;
      case (RDP_DONE_MSG):
        __scHandleRDP(sc);
        break;
      case (PRE_NMI_MSG):
        for (client = sc->clientList; client != 0; client = client->next) {
          osSendMesg(client->msgQ, (OSMesg)&sc->prenmiMsg, OS_MESG_NOBLOCK);
        }
        break;
    }
  }
}

static int dp_busy = 0;
static int dpCount = 0;

/*
 * Retrace handler: the per-frame entry point. Drain newly submitted tasks onto
 * the audio/gfx lists, then either yield the running RSP task for pending audio
 * or schedule the next ready task(s) onto the free RCP units, and finally
 * notify every client that a retrace occurred.
 */
void __scHandleRetrace(OSSched* sc) {
  OSScTask* rspTask;
  OSScClient* client;
  s32 i;
  s32 state;
  OSScTask* sp = 0;
  OSScTask* dp = 0;
  sc->frameCount++;
#ifdef SC_LOGGING
  osLogEvent(l, 500, 4, sc->frameCount, sc->curRSPTask, sc->curRDPTask);
#endif
  while (osRecvMesg(&sc->cmdQ, (OSMesg*)&rspTask, OS_MESG_NOBLOCK) != -1) {
    __scAppendList(sc, rspTask);
  }

  // If audio is waiting but the RSP is busy with graphics, preempt the graphics
  // task; otherwise schedule whatever is ready onto the idle units. state packs
  // RSP-idle in bit 1 and RDP-idle in bit 0 (the availRCP encoding).
  if (sc->doAudio && sc->curRSPTask) {
    __scYield(sc);
  } else {
    state = ((sc->curRSPTask == 0) << 1) | (sc->curRDPTask == 0);
    if (__scSchedule(sc, &sp, &dp, state) != state) __scExec(sc, sp, dp);
  }
  for (client = sc->clientList; client != 0; client = client->next) {
    osSendMesg(client->msgQ, (OSMesg)&sc->retraceMsg, OS_MESG_NOBLOCK);
  }
}

/*
 * RSP-completion handler. If the task merely yielded, re-queue it at the head
 * of the gfx list so it resumes later; otherwise mark its RSP requirement done
 * and complete it. Then schedule the next task(s) onto the freed units.
 */
void __scHandleRSP(OSSched* sc) {
  OSScTask *t, *sp = 0, *dp = 0;
  s32 state;
#ifdef _DEBUG
  assert(sc->curRSPTask);
#endif
  t = sc->curRSPTask;
  sc->curRSPTask = 0;
#ifdef SC_LOGGING
  osLogEvent(l, 510, 3, t, t->state, t->flags);
#endif

  // A genuine yield (vs. a normal finish): stash the task back on the gfx list
  // so __scSchedule can resume it; an XBUS task must re-acquire both units.
  if ((t->state & OS_SC_YIELD) && osSpTaskYielded(&t->list)) {
    t->state |= OS_SC_YIELDED;
#ifndef _FINALROM
    t->totalTime += osGetTime() - t->startTime;
#endif
    if ((t->flags & OS_SC_TYPE_MASK) == OS_SC_XBUS) {
      t->next = sc->gfxListHead;
      sc->gfxListHead = t;
      if (sc->gfxListTail == 0) sc->gfxListTail = t;
    }
#ifdef SC_LOGGING
    osLogEvent(l, 521, 1, t);
#endif
  } else {
    t->state &= ~OS_SC_NEEDS_RSP;
    __scTaskComplete(sc, t);
  }
  state = ((sc->curRSPTask == 0) << 1) | (sc->curRDPTask == 0);
  if ((__scSchedule(sc, &sp, &dp, state)) != state) __scExec(sc, sp, dp);
}

/*
 * RDP-completion handler. Clear the task's RDP requirement, complete it, and
 * schedule the next task(s) onto the freed units.
 */
void __scHandleRDP(OSSched* sc) {
  OSScTask *t, *sp = 0, *dp = 0;
  s32 state;
#ifdef _DEBUG
  assert(sc->curRDPTask);
  assert(sc->curRDPTask->list.t.type == M_GFXTASK);
#endif
  t = sc->curRDPTask;
  sc->curRDPTask = 0;
#ifdef SC_LOGGING
  osLogEvent(l, 515, 3, t, t->state, t->flags);
#endif
  t->state &= ~OS_SC_NEEDS_RDP;
  __scTaskComplete(sc, t);
  state = ((sc->curRSPTask == 0) << 1) | (sc->curRDPTask == 0);
  if ((__scSchedule(sc, &sp, &dp, state)) != state) __scExec(sc, sp, dp);
}

/*
 * Decide whether a graphics task may run yet. It must wait until the
 * framebuffer it would draw into is no longer being scanned out (current ==
 * next means the pending swap has not yet taken effect). Returns the task when
 * runnable, else 0.
 */
OSScTask* __scTaskReady(OSScTask* t) {
  int rv = 0;
  void* a;
  void* b;
  if (t) {
    if ((a = osViGetCurrentFramebuffer()) != (b = osViGetNextFramebuffer())) {
#ifdef SC_LOGGING
      osLogEvent(l, 513, 2, a, b);
#endif
      return 0;
    }
    return t;
  }
  return 0;
}

/*
 * Finish a task once it no longer needs any RCP unit: notify the submitter, and
 * for the last graphics task of a frame swap the displayed framebuffer (un-
 * blacking the screen on the very first swap). Returns 1 when completed.
 */
s32 __scTaskComplete(OSSched* sc, OSScTask* t) {
  int rv;
  static int firsttime = 1;
  if ((t->state & OS_SC_RCP_MASK) == 0) {
#ifdef _DEBUG
    assert(t->msgQ);
#endif
#ifndef _FINALROM
    t->totalTime += osGetTime() - t->startTime;
#endif
#ifdef SC_LOGGING
    osLogEvent(l, 504, 1, t);
#endif
    rv = osSendMesg(t->msgQ, t->msg, OS_MESG_BLOCK);
    if (t->list.t.type == M_GFXTASK) {
      if ((t->flags & OS_SC_SWAPBUFFER) && (t->flags & OS_SC_LAST_TASK)) {
        // Reveal the picture only once the first complete frame is ready.
        if (firsttime) {
          osViBlack(FALSE);
          firsttime = 0;
        }
        osViSwapBuffer(t->framebuffer);
#ifdef SC_LOGGING
        osLogEvent(l, 525, 1, t->framebuffer);
#endif
      }
    }
    return 1;
  }
  return 0;
}

/*
 * Append a newly submitted task to its queue: audio tasks to the audio list
 * (and flag that audio is pending), graphics tasks to the gfx list. Seeds the
 * task's state from its type flags so __scSchedule knows which units it still
 * needs.
 */
void __scAppendList(OSSched* sc, OSScTask* t) {
  long type = t->list.t.type;
#ifdef _DEBUG
  assert((type == M_AUDTASK) || (type == M_GFXTASK));
#endif
  if (type == M_AUDTASK) {
    if (sc->audioListTail)
      sc->audioListTail->next = t;
    else
      sc->audioListHead = t;
    sc->audioListTail = t;
    sc->doAudio = 1;
#ifdef SC_LOGGING
    osLogEvent(l, 506, 1, t);
#endif
  } else {
    if (sc->gfxListTail)
      sc->gfxListTail->next = t;
    else
      sc->gfxListHead = t;
    sc->gfxListTail = t;
#ifdef SC_LOGGING
    osLogEvent(l, 507, 1, t);
#endif
  }
  t->next = NULL;
  t->state = t->flags & OS_SC_RCP_MASK;
}

/*
 * Launch the chosen task(s) onto the RCP. The RSP task is loaded and started
 * (audio first writes back the data cache so the RSP sees fresh samples); a
 * distinct RDP task is pointed at its output framebuffer. When sp == dp the one
 * task drives both units.
 */
void __scExec(OSSched* sc, OSScTask* sp, OSScTask* dp) {
  int rv;
#ifdef SC_LOGGING
  osLogEvent(l, 511, 2, sp, dp);
#endif
#ifdef _DEBUG
  assert(sc->curRSPTask == 0);
#endif
  if (sp) {
    if (sp->list.t.type == M_AUDTASK) {
      osWritebackDCacheAll();
    }
    sp->state &= ~(OS_SC_YIELD | OS_SC_YIELDED);
#ifndef _FINALROM
    sp->startTime = osGetTime();
#endif
    osSpTaskLoad(&sp->list);
    osSpTaskStartGo(&sp->list);
    sc->curRSPTask = sp;
    if (sp == dp) sc->curRDPTask = dp;
  }
  if (dp && (dp != sp)) {
#ifdef _DEBUG
    assert(dp->list.t.output_buff);
#endif
#ifdef SC_LOGGING
    osLogEvent(l, 523, 3, dp, dp->list.t.output_buff,
               (u32)*dp->list.t.output_buff_size);
#endif
    rv =
        osDpSetNextBuffer(dp->list.t.output_buff, *dp->list.t.output_buff_size);
    dp_busy = 1;
    dpCount = 0;
#ifdef _DEBUG
    assert(rv == 0);
#endif
    sc->curRDPTask = dp;
  }
}

/*
 * Ask the running RSP graphics task to yield so pending audio can run. Only a
 * graphics task can be yielded; an audio task is left to finish.
 */
static void __scYield(OSSched* sc) {
#ifdef SC_LOGGING
  osLogEvent(l, 503, 1, sc->curRSPTask);
#endif
  if (sc->curRSPTask->list.t.type == M_GFXTASK) {
    sc->curRSPTask->state |= OS_SC_YIELD;
    osSpTaskYield();
  } else {
#ifdef SC_LOGGING
    osLogEvent(l, 508, 1, sc->curRSPTask);
#endif
  }
}

/*
 * Core scheduling decision. Given which RCP units are free (availRCP: bit 1 =
 * RSP, bit 0 = RDP), pick the next task(s) to run into *sp / *dp, consuming the
 * units and advancing the lists. Audio wins the RSP first; otherwise the head
 * graphics task is matched against its required units by type. Recurses while
 * units were just freed up, so several tasks can be packed onto one retrace.
 * Returns the units still unallocated.
 */
s32 __scSchedule(OSSched* sc, OSScTask** sp, OSScTask** dp, s32 availRCP) {
  s32 avail = availRCP;
  OSScTask* gfx = sc->gfxListHead;
  OSScTask* audio = sc->audioListHead;
#ifdef SC_LOGGING
  osLogEvent(l, 517, 3, *sp, *dp, availRCP);
#endif

  // Audio has priority for the RSP. A parallel-capable gfx task may share the
  // RSP this pass; otherwise the audio task takes it and is popped off its
  // list.
  if (sc->doAudio && (avail & OS_SC_SP)) {
    if (gfx && (gfx->flags & OS_SC_PARALLEL_TASK)) {
      *sp = gfx;
      avail &= ~OS_SC_SP;
    } else {
      *sp = audio;
      avail &= ~OS_SC_SP;
      sc->doAudio = 0;
      sc->audioListHead = sc->audioListHead->next;
      if (sc->audioListHead == NULL) sc->audioListTail = NULL;
    }
  } else {
#ifdef SC_LOGGING
    osLogEvent(l, 520, 1, gfx);
#endif
    // No audio pending: try to place the head graphics task, but only once its
    // target framebuffer is safe to draw into.
    if (__scTaskReady(gfx)) {
#ifdef SC_LOGGING
      osLogEvent(l, 522, 3, gfx, gfx->state, gfx->flags);
#endif
      switch (gfx->flags & OS_SC_TYPE_MASK) {
        // XBUS: RSP feeds the RDP directly, so the task needs both units (or
        // resumes a yielded task once the RSP is free).
        case (OS_SC_XBUS):
          if (gfx->state & OS_SC_YIELDED) {
#ifdef SC_LOGGING
            osLogEvent(l, 518, 0);
#endif
            if (avail & OS_SC_SP) {
#ifdef SC_LOGGING
              osLogEvent(l, 519, 0);
#endif
              *sp = gfx;
              avail &= ~OS_SC_SP;
              if (gfx->state & OS_SC_DP) {
                *dp = gfx;
                avail &= ~OS_SC_DP;
#ifdef _DEBUG
                if (avail & OS_SC_DP == 0) assert(sc->curRDPTask == gfx);
#endif
              }
              sc->gfxListHead = sc->gfxListHead->next;
              if (sc->gfxListHead == NULL) sc->gfxListTail = NULL;
            }
          } else {
            if (avail == (OS_SC_SP | OS_SC_DP)) {
              *sp = *dp = gfx;
              avail &= ~(OS_SC_SP | OS_SC_DP);
              sc->gfxListHead = sc->gfxListHead->next;
              if (sc->gfxListHead == NULL) sc->gfxListTail = NULL;
            }
          }
          break;
        // DRAM-staged display lists: the RSP and RDP halves run independently,
        // so place whichever unit the task currently needs and is free.
        case (OS_SC_DRAM):
        case (OS_SC_DP_DRAM):
        case (OS_SC_DP_XBUS):
          if (gfx->state & OS_SC_SP) {
            if (avail & OS_SC_SP) {
              *sp = gfx;
              avail &= ~OS_SC_SP;
            }
          } else if (gfx->state & OS_SC_DP) {
            if (avail & OS_SC_DP) {
              *dp = gfx;
              avail &= ~OS_SC_DP;
              sc->gfxListHead = sc->gfxListHead->next;
              if (sc->gfxListHead == NULL) sc->gfxListTail = NULL;
            }
          }
          break;
        case (OS_SC_SP_DRAM):
        case (OS_SC_SP_XBUS):
        default:
          break;
      }
    }
  }

  // If this pass allocated any unit, recurse to try to fill the rest too.
  if (avail != availRCP) avail = __scSchedule(sc, sp, dp, avail);
  return avail;
}
