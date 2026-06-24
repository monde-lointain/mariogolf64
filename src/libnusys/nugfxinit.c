/*
 * Graphics subsystem initialization.
 *
 * Brings up everything needed before nuGfxTaskStart can be used: the graphics
 * thread (retrace- and PRE_NMI-synchronized), the frame/Z/FIFO buffers and
 * microcode, the default frame-buffer swap callback, and the graphics task
 * manager. It finishes by running one throwaway display list so the RDP, whose
 * registers come up undefined, is left in a known state.
 */
#include <nusys.h>
#undef nuGfxInit

// Appears to configure the graphics buffer/microcode setup; called with the
// block below and the constant 3.
extern void func_80029250(void*, s32);

// Used as the configuration block passed to func_80029250 (appears to hold the
// frame buffers and related setup).
extern u32 D_800B6688;

// Used as the default microcode descriptor wired into the task manager below.
extern u32 D_800B6680;

// Used as the display list run once at the end to settle the RDP.
extern u32 D_B6698;

/*
 * Bring the graphics subsystem up and prime the RDP. Runs once at startup,
 * before any nuGfxTaskStart call.
 */
void nuGfxInit(void) {
  Gfx gfxList[0x100];
  Gfx* gfxList_ptr;

  // Start the retrace/PRE_NMI graphics thread and configure its buffers.
  nuGfxThreadStart();
  func_80029250(&D_800B6688, 3);

  // Z-buffer is placed at a fixed RDRAM address; install the default swap
  // callback.
  nuGfxZBuffer = (u16*)0x80000400;
  nuGfxSwapCfbFuncSet(nuGfxSwapCfb);
  nuGfxUcode = (NUUcode*)&D_800B6680;
  nuGfxTaskMgrInit();

  // Build a minimal display list (a sub-list call, a full pipeline sync, and an
  // end marker) and run it once. Its only job is to settle the RDP, so the
  // function blocks on task completion before returning.
  gfxList_ptr = gfxList;
  gSPDisplayList(gfxList_ptr++, (u32)&D_B6698);
  gDPFullSync(gfxList_ptr++);
  gSPEndDisplayList(gfxList_ptr++);
  nuGfxTaskStart(gfxList, (s32)(gfxList_ptr - gfxList) * sizeof(Gfx), 0, 0);
  nuGfxTaskAllEndWait();
}
