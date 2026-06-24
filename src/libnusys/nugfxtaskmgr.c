/*
 * Graphics task manager (game-customized nusys variant): owns the pool of RCP
 * task records, runs the manager thread that retires completed tasks and
 * throttles buffer swaps, and exposes nuGfxTaskStart for the app to submit a
 * display list. The D_-prefixed externs are the game's relocated copies of the
 * stock nusys task ring, head pointer, manager thread, queue, and the
 * end/swap message constants.
 */

#include <nusys.h>

extern NUScTask D_801B5D08[NU_GFX_TASK_NUM];  // ring of task records
extern NUScTask* D_800D8984;                  // next task record to fill
extern NUScMsg D_800D8988;                    // GTASKEND message constant
extern NUScMsg D_800D898A;                    // SWAPBUFFER message constant
extern OSThread D_800D8990;                   // manager thread
extern OSMesg D_800DAB40[];                   // manager queue message buffer
extern OSMesgQueue D_801B93F8;                // manager completion queue
extern u32 D_800D8980;                        // retrace count of the last swap
extern u8 D_800B67A4[];                       // CFB index advance table
extern u16 D_800B67BC;                        // pending task flags carry

/*
 * Manager thread body: drains the completion queue, retiring finished tasks and
 * handling the paced buffer swap / display-on for swapping tasks.
 */
void nuGfxTaskMgr(void* arg) {
  NUScMsg* mesg_type;
  NUScTask* gfxTask;
  OSIntMask mask;

  osCreateMesgQueue(&D_801B93F8, D_800DAB40, NU_GFX_MESGS);
  while (1) {
    // Block for the next completed task and read what kind of completion it is.
    (void)osRecvMesg(&D_801B93F8, (OSMesg*)&gfxTask, OS_MESG_BLOCK);
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
  D_800D8988 = NU_SC_GTASKEND_MSG;
  D_800D898A = NU_SC_SWAPBUFFER_MSG;
  nuGfxTaskSpool = 0;
  nuGfxDisplayOff();

  osCreateThread(&D_800D8990, NU_GFX_TASKMGR_THREAD_ID, nuGfxTaskMgr,
                 (void*)NULL, (D_800DAB40), NU_GFX_TASKMGR_THREAD_PRI);
  osStartThread(&D_800D8990);

  // Initialize each task record with the invariant fields.
  for (cnt = 0; cnt < NU_GFX_TASK_NUM; cnt++) {
    D_801B5D08[cnt].next = &D_801B5D08[cnt + 1];
    D_801B5D08[cnt].msgQ = &D_801B93F8;
    D_801B5D08[cnt].list.t.type = M_GFXTASK;
    D_801B5D08[cnt].list.t.flags = 0x00;

    // The RSP boot microcode is the blob spanning [rspbootTextStart,
    // rspbootTextEnd).
    D_801B5D08[cnt].list.t.ucode_boot = (u64*)rspbootTextStart;
    D_801B5D08[cnt].list.t.ucode_boot_size =
        ((s32)rspbootTextEnd - (s32)rspbootTextStart);
    D_801B5D08[cnt].list.t.ucode_size = SP_UCODE_SIZE;
    D_801B5D08[cnt].list.t.ucode_data_size = SP_UCODE_DATA_SIZE;

    // Shared RSP DRAM stack, RDP output buffer (128 KB), and yield buffer.
    D_801B5D08[cnt].list.t.dram_stack = (u64*)nuDramStack;
    D_801B5D08[cnt].list.t.dram_stack_size = SP_DRAM_STACK_SIZE8;
    D_801B5D08[cnt].list.t.output_buff = (u64*)&nuRDPOutputBuf[0];
    D_801B5D08[cnt].list.t.output_buff_size = (u64*)&nuRDPOutputBuf[0x20000];
    D_801B5D08[cnt].list.t.yield_data_ptr = (u64*)nuYieldBuf;
    D_801B5D08[cnt].list.t.yield_data_size = OS_YIELD_DATA_SIZE;
    D_801B5D08[cnt].msgQ = &D_801B93F8;
  }

  // Close the ring and point the fill cursor at the first record.
  D_801B5D08[NU_GFX_TASK_NUM - 1].next = &D_801B5D08[0];
  D_800D8984 = &D_801B5D08[0];
}

/*
 * Submit a display list as the next graphics task: fill the current ring record
 * with the list, the chosen microcode, and the flags, pick the swap-vs-end
 * completion message, then enqueue it to the scheduler and advance the cursor.
 */
void nuGfxTaskStart(Gfx* gfxList_ptr, u32 gfxListSize, u32 ucode, u32 flag) {
  OSIntMask mask;

  D_800D8984->list.t.data_ptr = (u64*)gfxList_ptr;
  D_800D8984->list.t.data_size = gfxListSize;
  D_800D8984->list.t.flags = flag >> 16;
  D_800D8984->list.t.ucode = nuGfxUcode[ucode].ucode;
  D_800D8984->list.t.ucode_data = nuGfxUcode[ucode].ucode_data;
  D_800D8984->flags = flag & 0x0000ffff;
  D_800D8984->framebuffer = nuGfxCfb_ptr;

  // If the previous task ran XBUS microcode, make this task wait for the RDP to
  // drain before starting, then clear the carried XBUS bit.
  if (D_800B67BC & NU_SC_UCODE_XBUS) {
    D_800D8984->list.t.flags |= OS_TASK_DP_WAIT;
    D_800B67BC ^= NU_SC_UCODE_XBUS;
  }
  D_800B67BC = flag;

  // Choose the completion message. A swapping task also rotates to the next
  // color framebuffer so drawing moves off the buffer about to be displayed.
  if (flag & NU_SC_SWAPBUFFER) {
    D_800D8984->msg = (OSMesg)&D_800D898A;
    nuGfxCfbCounter = D_800B67A4[nuGfxCfbCounter];
    nuGfxCfb_ptr = nuGfxCfb[nuGfxCfbCounter];
  } else {
    D_800D8984->msg = (OSMesg)&D_800D8988;
  }

  // Count the task in flight, flush the display list to RDRAM, and queue it.
  mask = osSetIntMask(OS_IM_NONE);
  nuGfxTaskSpool++;
  osSetIntMask(mask);
  osWritebackDCacheAll();
  osSendMesg(&nusched.graphicsRequestMQ, (OSMesg*)D_800D8984, OS_MESG_BLOCK);
  D_800D8984 = D_800D8984->next;
}
