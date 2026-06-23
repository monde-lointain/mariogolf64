/*
 * pimgr.c -- Parallel Interface device manager startup.
 *
 * osCreatePiManager spins up the worker thread that drains the PI command queue
 * and serializes all cartridge/peripheral DMA. On non-final ROMs it also starts
 * a helper thread that services host-side RAM-ROM requests over the debug link.
 */
#include "PRinternal/macros.h"
#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "piint.h"
#include "PR/rdb.h"

extern OSThread piThread;
extern u64 piThreadStack[ALIGN8(OS_PIM_STACKSIZE) / sizeof(u64)];

/* Debug-only RAM-ROM relay thread and its private queues, present only when the
 * ROM is built with the debugger link (not _FINALROM). */
#ifndef _FINALROM
static OSThread ramromThread ALIGNED(0x8);
static STACK(ramromThreadStack, 0x400) ALIGNED(0x10);
static OSMesgQueue getRamromQ ALIGNED(0x8);
static OSMesg getRamromBuf[1];
static OSMesgQueue freeRamromQ ALIGNED(0x8);
static OSMesg freeRamromBuf[1];
static void ramromMain(void*);
#endif

extern OSMesgQueue piEventQueue;
extern OSMesg piEventBuf[1];

/* Start the PI manager: create its command and event queues, register for PI
 * interrupts, populate the device-manager record, and launch the worker thread
 * at priority `pri`. Idempotent: a second call returns once the manager is up.
 */
void osCreatePiManager(OSPri pri, OSMesgQueue* cmdQ, OSMesg* cmdBuf,
                       s32 cmdMsgCnt) {
  u32 savedMask;
  OSPri oldPri;
  OSPri myPri;

#ifdef _DEBUG
  if ((pri < OS_PRIORITY_IDLE) || (pri > OS_PRIORITY_MAX)) {
    __osError(ERR_OSCREATEPIMANAGER, 1, pri);
    return;
  }
#endif

  if (__osPiDevMgr.active) {
    return;
  }

  osCreateMesgQueue(cmdQ, cmdBuf, cmdMsgCnt);
  osCreateMesgQueue(&piEventQueue, (OSMesg*)piEventBuf, 1);
  if (!__osPiAccessQueueEnabled) {
    __osPiCreateAccessQueue();
  }
  osSetEventMesg(OS_EVENT_PI, &piEventQueue, (OSMesg)0x22222222);

  /* Temporarily raise this thread to `pri` so the manager's threads inherit a
   * stable priority while we register them; restore the old priority at the end
   * if we changed it. oldPri == -1 means it was left alone. */
  oldPri = -1;
  myPri = osGetThreadPri(NULL);
  if (myPri < pri) {
    oldPri = myPri;
    osSetThreadPri(NULL, pri);
  }

  /* Publish the device-manager record and start the worker under a disabled
   * interrupt window so no PI event arrives at a half-built manager. */
  savedMask = __osDisableInt();
  __osPiDevMgr.active = 1;
  __osPiDevMgr.thread = &piThread;
  __osPiDevMgr.cmdQueue = cmdQ;
  __osPiDevMgr.evtQueue = &piEventQueue;
  __osPiDevMgr.acsQueue = &__osPiAccessQueue;
  __osPiDevMgr.dma = __osPiRawStartDma;
  __osPiDevMgr.edma = __osEPiRawStartDma;
  osCreateThread(&piThread, 0, __osDevMgrMain, &__osPiDevMgr,
                 STACK_START(piThreadStack), pri);
  osStartThread(&piThread);

  /* The debug RAM-ROM helper runs one priority below the manager. */
#ifndef _FINALROM
  osCreateThread(&ramromThread, 0, ramromMain, NULL,
                 STACK_START(ramromThreadStack), (OSPri)pri - 1);
  osStartThread(&ramromThread);
#endif

  __osRestoreInt(savedMask);
  if (oldPri != -1) {
    osSetThreadPri(NULL, oldPri);
  }
}

/* Debug RAM-ROM relay: on each host request, take the PI lock, push one RAM-ROM
 * block up the debug link, then wait for the host to free it before releasing
 * the lock. Loops forever. */
#ifndef _FINALROM
static void ramromMain(void* arg) {
  u32 sent;
  u8 tmp[3];

  osCreateMesgQueue(&getRamromQ, getRamromBuf, 1);
  osCreateMesgQueue(&freeRamromQ, freeRamromBuf, 1);
  osSetEventMesg(OS_EVENT_RDB_REQ_RAMROM, &getRamromQ, NULL);
  osSetEventMesg(OS_EVENT_RDB_FREE_RAMROM, &freeRamromQ, NULL);
  while (TRUE) {
    osRecvMesg(&getRamromQ, NULL, OS_MESG_BLOCK);
    __osPiGetAccess();
    sent = 0;
    while (sent < 1) {
      sent += __osRdbSend(tmp, 1, RDB_TYPE_GtoH_RAMROM);
    }
    osRecvMesg(&freeRamromQ, NULL, OS_MESG_BLOCK);
    __osPiRelAccess();
  }
}
#endif
