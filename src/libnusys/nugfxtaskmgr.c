/*
 * Graphics task manager (game-customized nusys variant): owns the pool of RCP
 * task records, runs the manager thread that retires completed tasks and
 * throttles buffer swaps, and exposes nuGfxTaskStart for the app to submit a
 * display list. The externs are the game's relocated copies of the stock nusys
 * statics/globals (task ring, head pointer, manager thread + its stack, queue +
 * its buffer, end/swap message constants). Two have no stock equivalent and
 * stay generic: D_800D8980 (a swap-pacing retrace counter) and D_800B67A4 (a
 * CFB-index advance table replacing stock's modulo).
 */

#include <nusys.h>

extern NUScTask nuGfxTask[NU_GFX_TASK_NUM];  // ring of task records
extern NUScTask* nuGfxTask_ptr;              // next task record to fill
extern NUScMsg taskDoneMsg;                  // GTASKEND message constant
extern NUScMsg swapBufMsg;                   // SWAPBUFFER message constant
extern OSThread GfxTaskMgrThread;            // gfx task manager thread
extern u64
    GfxTaskMgrStack[NU_GFX_TASKMGR_STACK_SIZE / sizeof(u64)];  // its stack
extern OSMesg
    nuGfxTaskMgrMesgBuf[NU_GFX_TASKMGR_MESGS];  // manager queue buffer
extern OSMesgQueue nuGfxTaskMgrMesgQ;           // manager completion queue
extern u16 beforeFlag;  // previous task's flags (XBUS carry)
// Game-custom (no stock nusys equivalent), kept generic:
extern u32 D_800D8980;   // retrace count of the last swap
extern u8 D_800B67A4[];  // CFB index advance table

/*
 * Manager thread body: drains the completion queue, retiring finished tasks and
 * handling the paced buffer swap / display-on for swapping tasks.
 */
void nuGfxTaskMgr(void* arg) {
  NUScMsg* mesg_type;
  NUScTask* gfxTask;
  OSIntMask mask;

  osCreateMesgQueue(&nuGfxTaskMgrMesgQ, nuGfxTaskMgrMesgBuf, NU_GFX_MESGS);
  while (1) {
    // Block for the next completed task and read what kind of completion it is.
    (void)osRecvMesg(&nuGfxTaskMgrMesgQ, (OSMesg*)&gfxTask, OS_MESG_BLOCK);
    mesg_type = gfxTask->msg;
    switch (*mesg_type) {
      case NU_SC_SWAPBUFFER_MSG:
        // Pace buffer swaps: spin until at least two retraces have elapsed
        // since the previous swap so a frame is always shown long enough.
        // nuGfxRetraceWait drops the mask while sleeping, hence re-masking each
        // iteration.
        while (1) {
          mask = osSetIntMask(OS_IM_NONE);
          if ((nuScRetraceCounter - D_800D8980) >= 2) {
            break;
          }
          osSetIntMask(mask);
          nuGfxRetraceWait(1);
        }
        D_800D8980 = nuScRetraceCounter;
        osSetIntMask(mask);

        // Hand the finished frame to the swap callback (queues the VI swap).
        if (nuGfxSwapCfbFunc != NULL) {
          (*nuGfxSwapCfbFunc)((void*)gfxTask);
        }

        // Retire the task from the in-flight count.
        mask = osSetIntMask(OS_IM_NONE);
        nuGfxTaskSpool--;
        osSetIntMask(mask);

        // Honor a deferred display-on request now that a real frame is up,
        // un-blanking the screen on the first swap after nuGfxDisplayOn.
        if (nuGfxDisplay & NU_GFX_DISPLAY_ON_TRIGGER) {
          osViBlack(FALSE);
          nuGfxDisplay = NU_GFX_DISPLAY_ON;
        }
        break;

      case NU_SC_GTASKEND_MSG:
        // A non-swapping task finished: just retire it and notify the app.
        mask = osSetIntMask(OS_IM_NONE);
        nuGfxTaskSpool--;
        osSetIntMask(mask);
        if (nuGfxTaskEndFunc != NULL) {
          (*nuGfxTaskEndFunc)((void*)gfxTask);
        }
        break;
    }
  }
}

/*
 * One-time setup: start the manager thread, then pre-fill every task record in
 * the ring with the fixed RSP parameters (microcode boot blob, stacks, RDP
 * output buffer, yield buffer) so nuGfxTaskStart only has to plug in the
 * display list. The records are chained into a circular list.
 */
void nuGfxTaskMgrInit(void) {
  u32 cnt;

  // Cache the two completion-message constants and start blanked.
  taskDoneMsg = NU_SC_GTASKEND_MSG;
  swapBufMsg = NU_SC_SWAPBUFFER_MSG;
  nuGfxTaskSpool = 0;
  nuGfxDisplayOff();

  osCreateThread(&GfxTaskMgrThread, NU_GFX_TASKMGR_THREAD_ID, nuGfxTaskMgr,
                 (void*)NULL,
                 (GfxTaskMgrStack + NU_GFX_TASKMGR_STACK_SIZE / sizeof(u64)),
                 NU_GFX_TASKMGR_THREAD_PRI);
  osStartThread(&GfxTaskMgrThread);

  // Initialize each task record with the invariant fields.
  for (cnt = 0; cnt < NU_GFX_TASK_NUM; cnt++) {
    nuGfxTask[cnt].next = &nuGfxTask[cnt + 1];
    nuGfxTask[cnt].msgQ = &nuGfxTaskMgrMesgQ;
    nuGfxTask[cnt].list.t.type = M_GFXTASK;
    nuGfxTask[cnt].list.t.flags = 0x00;

    // The RSP boot microcode is the blob spanning [rspbootTextStart,
    // rspbootTextEnd).
    nuGfxTask[cnt].list.t.ucode_boot = (u64*)rspbootTextStart;
    nuGfxTask[cnt].list.t.ucode_boot_size =
        ((s32)rspbootTextEnd - (s32)rspbootTextStart);
    nuGfxTask[cnt].list.t.ucode_size = SP_UCODE_SIZE;
    nuGfxTask[cnt].list.t.ucode_data_size = SP_UCODE_DATA_SIZE;

    // Shared RSP DRAM stack, RDP output buffer (128 KB), and yield buffer.
    nuGfxTask[cnt].list.t.dram_stack = (u64*)nuDramStack;
    nuGfxTask[cnt].list.t.dram_stack_size = SP_DRAM_STACK_SIZE8;
    nuGfxTask[cnt].list.t.output_buff = (u64*)&nuRDPOutputBuf[0];
    nuGfxTask[cnt].list.t.output_buff_size = (u64*)&nuRDPOutputBuf[0x20000];
    nuGfxTask[cnt].list.t.yield_data_ptr = (u64*)nuYieldBuf;
    nuGfxTask[cnt].list.t.yield_data_size = OS_YIELD_DATA_SIZE;
    nuGfxTask[cnt].msgQ = &nuGfxTaskMgrMesgQ;
  }

  // Close the ring and point the fill cursor at the first record.
  nuGfxTask[NU_GFX_TASK_NUM - 1].next = &nuGfxTask[0];
  nuGfxTask_ptr = &nuGfxTask[0];
}

/*
 * Submit a display list as the next graphics task: fill the current ring record
 * with the list, the chosen microcode, and the flags, pick the swap-vs-end
 * completion message, then enqueue it to the scheduler and advance the cursor.
 */
void nuGfxTaskStart(Gfx* gfxList_ptr, u32 gfxListSize, u32 ucode, u32 flag) {
  OSIntMask mask;

  nuGfxTask_ptr->list.t.data_ptr = (u64*)gfxList_ptr;
  nuGfxTask_ptr->list.t.data_size = gfxListSize;
  nuGfxTask_ptr->list.t.flags = flag >> 16;
  nuGfxTask_ptr->list.t.ucode = nuGfxUcode[ucode].ucode;
  nuGfxTask_ptr->list.t.ucode_data = nuGfxUcode[ucode].ucode_data;
  nuGfxTask_ptr->flags = flag & 0x0000ffff;
  nuGfxTask_ptr->framebuffer = nuGfxCfb_ptr;

  // If the previous task ran XBUS microcode, make this task wait for the RDP to
  // drain before starting, then clear the carried XBUS bit.
  if (beforeFlag & NU_SC_UCODE_XBUS) {
    nuGfxTask_ptr->list.t.flags |= OS_TASK_DP_WAIT;
    beforeFlag ^= NU_SC_UCODE_XBUS;
  }
  beforeFlag = flag;

  // Choose the completion message. A swapping task also rotates to the next
  // color framebuffer so drawing moves off the buffer about to be displayed.
  if (flag & NU_SC_SWAPBUFFER) {
    nuGfxTask_ptr->msg = (OSMesg)&swapBufMsg;
    nuGfxCfbCounter = D_800B67A4[nuGfxCfbCounter];
    nuGfxCfb_ptr = nuGfxCfb[nuGfxCfbCounter];
  } else {
    nuGfxTask_ptr->msg = (OSMesg)&taskDoneMsg;
  }

  // Count the task in flight, flush the display list to RDRAM, and queue it.
  mask = osSetIntMask(OS_IM_NONE);
  nuGfxTaskSpool++;
  osSetIntMask(mask);
  osWritebackDCacheAll();
  osSendMesg(&nusched.graphicsRequestMQ, (OSMesg*)nuGfxTask_ptr, OS_MESG_BLOCK);
  nuGfxTask_ptr = nuGfxTask_ptr->next;
}
