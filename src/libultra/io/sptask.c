/*
 * RSP task launch: stage an OSTask into the signal processor and start it.
 *
 * The task structure the caller builds holds RDRAM (virtual) pointers, but the
 * RSP boot microcode reads them as physical addresses. So osSpTaskLoad copies
 * the task to a scratch OSTask, rewrites every pointer field to physical, DMAs
 * that header plus the boot microcode into IMEM, and osSpTaskStartGo releases
 * the processor.
 */
#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "PR/sptask.h"
#include "PR/rcp.h"
#include "osint.h"

#if BUILD_VERSION < VERSION_J
#ident "$Revision: 1.4 $"
#endif

// Convert one task pointer field from virtual to physical in place, leaving a
// NULL pointer alone.
#define _osVirtualToPhysical(ptr)          \
  if (ptr != NULL) {                       \
    ptr = (void*)osVirtualToPhysical(ptr); \
  }                                        \
  (void)0

extern OSTask tmp_task;

/*
 * Copy the caller's task into the scratch tmp_task and convert all of its RDRAM
 * pointer fields to physical addresses, so the result can be DMAed straight to
 * the RSP. Returns the scratch copy; the caller's task is left untouched.
 */
static OSTask* _VirtualToPhysicalTask(OSTask* intp) {
  OSTask* tp;
  tp = &tmp_task;
  bcopy(intp, tp, sizeof(OSTask));
  _osVirtualToPhysical(tp->t.ucode);
  _osVirtualToPhysical(tp->t.ucode_data);
  _osVirtualToPhysical(tp->t.dram_stack);
  _osVirtualToPhysical(tp->t.output_buff);
  _osVirtualToPhysical(tp->t.output_buff_size);
  _osVirtualToPhysical(tp->t.data_ptr);
  _osVirtualToPhysical(tp->t.yield_data_ptr);
  return tp;
}

/*
 * Stage a task into the RSP. After validating pointer alignment (debug builds),
 * it physical-izes the task, fixes up the ucode for a resumed (yielded) task,
 * writes the task back to RDRAM so the DMA sees fresh data, and DMAs the task
 * header and boot microcode into IMEM. The processor is left halted; call
 * osSpTaskStartGo to run it.
 */
void osSpTaskLoad(OSTask* intp) {
  OSTask* tp;
#ifdef _DEBUG
  if ((intp->t.dram_stack != 0x0) && ((u32)intp->t.dram_stack & 0xf)) {
    __osError(ERR_OSSPTASKLOAD_DRAM, 1, intp->t.dram_stack);
    return;
  }
  if ((intp->t.output_buff != 0x0) && ((u32)intp->t.output_buff & 0xf)) {
    __osError(ERR_OSSPTASKLOAD_OUT, 1, intp->t.output_buff);
    return;
  }
  if ((intp->t.output_buff_size != 0x0) &&
      ((u32)intp->t.output_buff_size & 0xf)) {
    __osError(ERR_OSSPTASKLOAD_OUTSIZE, 1, intp->t.output_buff_size);
    return;
  }
  if ((intp->t.yield_data_ptr != 0x0) && ((u32)intp->t.yield_data_ptr & 0xf)) {
    __osError(ERR_OSSPTASKLOAD_YIELD, 1, intp->t.yield_data_ptr);
    return;
  }
#endif
  tp = _VirtualToPhysicalTask(intp);

  // Resuming a yielded task: feed it the saved yield state instead of the
  // original ucode data, and (for a loadable ucode) recover the saved ucode
  // pointer stashed at the end of the yield buffer.
  if (tp->t.flags & OS_TASK_YIELDED) {
    tp->t.ucode_data = tp->t.yield_data_ptr;
    tp->t.ucode_data_size = tp->t.yield_data_size;
    intp->t.flags &= ~OS_TASK_YIELDED;
    if (tp->t.flags & OS_TASK_LOADABLE) {
      tp->t.ucode =
          (u64*)IO_READ((u32)intp->t.yield_data_ptr + OS_YIELD_DATA_SIZE - 4);
    }
  }
  osWritebackDCache(tp, sizeof(OSTask));
  __osSpSetStatus(SP_CLR_YIELD | SP_CLR_YIELDED | SP_CLR_TASKDONE |
                  SP_SET_INTR_BREAK);

  // Point the SP at the start of IMEM, DMA the task header to just below it,
  // wait for that to land, then DMA the boot microcode into IMEM. Each DMA is
  // retried until the SP is free to accept it.
  while (__osSpSetPc(SP_IMEM_START) == -1) {
  }
  while (__osSpRawStartDma(1, (SP_IMEM_START - sizeof(*tp)), tp,
                           sizeof(OSTask)) == -1) {
  }
  while (__osSpDeviceBusy()) {
  }
  while (__osSpRawStartDma(1, SP_IMEM_START, tp->t.ucode_boot,
                           tp->t.ucode_boot_size) == -1) {
  }
}

/*
 * Release a loaded task to run: wait for any pending SP DMA to finish, then
 * clear halt/break so the processor executes the boot microcode.
 */
void osSpTaskStartGo(OSTask* tp) {
  while (__osSpDeviceBusy()) {
  }
  __osSpSetStatus(SP_SET_INTR_BREAK | SP_CLR_SSTEP | SP_CLR_BROKE |
                  SP_CLR_HALT);
}
