/*
 * VI manager: the system thread that services vertical-retrace and CPU-counter
 * interrupts.
 *
 * osCreateViManager builds the manager thread and its event plumbing; viMgrMain
 * is the thread body that swaps the VI context each retrace, posts the
 * application's retrace message, dispatches timer interrupts, and advances the
 * OS time base.
 */

#include "PRinternal/macros.h"
#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "PR/rcp.h"
#include "viint.h"
#include "osint.h"

OSDevMgr __osViDevMgr = {0};

#if BUILD_VERSION >= VERSION_J
// Extra scanlines to shift the active display down (FPAL support).
u32 __additional_scanline = 0;
#endif

extern OSThread viThread;
extern u64 viThreadStack[ALIGN8(OS_VIM_STACKSIZE) / sizeof(u64)];
extern OSMesgQueue viEventQueue;
extern OSMesg viEventBuf[5];
extern OSIoMesg viRetraceMsg;
extern OSIoMesg viCounterMsg;
extern u16 retrace;

static void viMgrMain(void* arg);

/* Create and launch the VI manager thread at priority pri. Must be called once,
 * early in startup, so retrace interrupts begin being serviced. */
void osCreateViManager(OSPri pri) {
  u32 savedMask;
  OSPri oldPri;
  OSPri myPri;

#ifdef _DEBUG
  if ((pri < OS_PRIORITY_IDLE) || (pri > OS_PRIORITY_MAX)) {
    __osError(ERR_OSCREATEVIMANAGER, 1, pri);
    return;
  }
#endif

  // Idempotent: a second call is a no-op.
  if (__osViDevMgr.active) {
    return;
  }

  __osTimerServicesInit();
#if BUILD_VERSION >= VERSION_J
  __additional_scanline = 0;
#endif

  // One queue carries both VI-retrace and CPU-counter events to the thread.
  osCreateMesgQueue(&viEventQueue, viEventBuf, ARRLEN(viEventBuf));
  viRetraceMsg.hdr.type = OS_MESG_TYPE_VRETRACE;
  viRetraceMsg.hdr.pri = OS_MESG_PRI_NORMAL;
  viRetraceMsg.hdr.retQueue = NULL;
  viCounterMsg.hdr.type = OS_MESG_TYPE_COUNTER;
  viCounterMsg.hdr.pri = OS_MESG_PRI_NORMAL;
  viCounterMsg.hdr.retQueue = NULL;
  osSetEventMesg(OS_EVENT_VI, &viEventQueue, &viRetraceMsg);
  osSetEventMesg(OS_EVENT_COUNTER, &viEventQueue, &viCounterMsg);

  // Temporarily raise the caller to pri so the setup below (and the manager's
  // first run) is not preempted; oldPri remembers it for restore.
  oldPri = -1;
  myPri = osGetThreadPri(NULL);
  if (myPri < pri) {
    oldPri = myPri;
    osSetThreadPri(NULL, pri);
  }

  // Publish the device-manager record and start the thread atomically.
  savedMask = __osDisableInt();
  __osViDevMgr.active = TRUE;
  __osViDevMgr.thread = &viThread;
  __osViDevMgr.cmdQueue = &viEventQueue;
  __osViDevMgr.evtQueue = &viEventQueue;
  __osViDevMgr.acsQueue = NULL;
  __osViDevMgr.dma = NULL;
  __osViDevMgr.edma = NULL;
  osCreateThread(&viThread, 0, viMgrMain, &__osViDevMgr,
                 STACK_START(viThreadStack), pri);
  __osViInit();
  osStartThread(&viThread);
  __osRestoreInt(savedMask);

  if (oldPri != -1) {
    osSetThreadPri(NULL, oldPri);
  }
}

/* Manager thread body: block on the event queue, then handle each event. */
static void viMgrMain(void* arg) {
  __OSViContext* vc;
  OSDevMgr* dm;
  OSIoMesg* mb;
  s32 first;
  u32 count;

  mb = NULL;
  first = 0;

  // retrace counts down the requested retrace divisor; never let it reach 0
  // before the loop starts.
  vc = __osViGetCurrentContext();
  retrace = vc->retraceCount;
  if (retrace == 0) {
    retrace = 1;
  }
  dm = (OSDevMgr*)arg;

  while (TRUE) {
    osRecvMesg(dm->evtQueue, (OSMesg)&mb, OS_MESG_BLOCK);
    switch (mb->hdr.type) {
      case OS_MESG_TYPE_VRETRACE:
        // Present the pending context, then count down to the next application
        // notification.
        __osViSwapContext();
        retrace--;
        if (retrace == 0) {
          vc = __osViGetCurrentContext();
          if (vc->msgq != NULL) {
            osSendMesg(vc->msgq, vc->msg, OS_MESG_NOBLOCK);
          }
          retrace = vc->retraceCount;
        }
        __osViIntrCount++;

        // The retrace interrupt also drives the OS time base: accumulate the
        // elapsed CPU counts since the previous retrace into __osCurrentTime.
        // first seeds the base on the very first pass.
        if (first) {
          count = osGetCount();
          __osCurrentTime = count;
          first = 0;
        }
        count = __osBaseCounter;
        __osBaseCounter = osGetCount();
        count = __osBaseCounter - count;
        __osCurrentTime = __osCurrentTime + count;
        break;
      case OS_MESG_TYPE_COUNTER:
        __osTimerInterrupt();
        break;
      default:
        break;
    }
  }
}
