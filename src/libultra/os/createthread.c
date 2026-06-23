/*
 * Thread creation.
 *
 * Initializes a caller-allocated OSThread and registers it on the global active
 * list. The thread is built in the STOPPED state with a fully-formed initial
 * CPU context, so a later osStartThread can simply restore the context to begin
 * executing `entry(arg)` on the supplied stack.
 */

#include "PR/os_internal.h"
#include "PR/R4300.h"
#include "PR/ultraerror.h"
#include "osint.h"

extern __OSThreadprofile_s thprof[];

void osCreateThread(OSThread* t, OSId id, void (*entry)(void*), void* arg,
                    void* sp, OSPri p) {
  register u32 saveMask;
  OSIntMask mask;

#ifdef _DEBUG
  if ((u32)sp & 0x7) {
    __osError(ERR_OSCREATETHREAD_SP, 1, sp);
    return 0;
  }
  if ((p < OS_PRIORITY_IDLE) || (p > OS_PRIORITY_MAX)) {
    __osError(ERR_OSCREATETHREAD_PRI, 1, p);
    return 0;
  }
#endif

  t->id = id;
  t->priority = p;
  t->next = NULL;
  t->queue = NULL;

  // Seed the saved CPU context so the first dispatch enters entry(arg). a0
  // carries the argument; the stack pointer is biased down 16 bytes to leave
  // the mandatory argument-save area; ra points at the cleanup trampoline so a
  // thread that simply returns is torn down rather than running off into
  // nothing.
  t->context.pc = (u32)entry;
  t->context.a0 = (s64)(s32)arg;
  t->context.sp = (s64)(s32)sp - 16;
  t->context.ra = (s64)(s32)__osCleanupThread;

  // Start with all interrupts enabled but masked behind EXL: the thread begins
  // in exception level so the very first context restore (an ERET) atomically
  // drops to user level with the intended interrupt mask in place.
  mask = OS_IM_ALL;
  t->context.sr = (mask & (SR_IMASK | SR_IE)) | SR_EXL;
  t->context.rcp = (mask & RCP_IMASK) >> RCP_IMASKSHIFT;

  // Flush-to-zero plus enable invalid-operation exceptions for the FPU.
  t->context.fpcsr = (u32)(FPCSR_FS | FPCSR_EV);
  t->fp = 0;

  t->state = OS_STATE_STOPPED;
  t->flags = 0;

#ifndef _FINALROM
  // Attach a per-thread profiling slot for non-final builds, clamping ids past
  // the table to its last entry rather than indexing out of bounds.
  if (id < THPROF_IDMAX) {
    t->thprof = &thprof[id];
  } else {
    t->thprof = &thprof[THPROF_IDMAX - 1];
  }
#endif

  // Link onto the global active list under interrupt lock so a concurrent
  // dispatch cannot observe a half-inserted node.
  saveMask = __osDisableInt();
  t->tlnext = __osActiveQueue;
  __osActiveQueue = t;
  __osRestoreInt(saveMask);
}
