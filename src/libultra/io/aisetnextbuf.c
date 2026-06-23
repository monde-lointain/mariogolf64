/*
 * aisetnextbuf.c -- queue the next RDRAM->AI audio DMA transfer.
 *
 * The hardware can hold one pending buffer beyond the one playing; osAiSetNext-
 * Buffer loads it. It also patches a known DMA hardware bug that corrupts
 * transfers whose buffer ends on a 0x2000 boundary (see hdwrBugFlag below).
 */
#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "PR/rcp.h"
#include "osint.h"

// Sticky flag: set when the PREVIOUS buffer ended on the troublesome boundary,
// so this call knows to start the new transfer 0x2000 bytes early.
extern u8 hdwrBugFlag;

s32 osAiSetNextBuffer(void* bufPtr, u32 size) {
  char* bptr;

  // Only one buffer may be pending; bail if the DMA registers are already full.
  if (__osAiDeviceBusy()) {
    return -1;
  }

  // The DMA engine requires an 8-byte-aligned address and an 8-byte-multiple
  // length; reject anything else early.
#ifdef _DEBUG
  if ((u32)bufPtr & (8 - 1)) {
    __osError(ERR_OSAISETNEXTBUFFER_ADDR, 1, bufPtr);
    return -1;
  }
  if ((u32)size & (8 - 1)) {
    __osError(ERR_OSAISETNEXTBUFFER_SIZE, 1, size);
    return -1;
  }
#endif

  // Apply the bug workaround left over from the previous call: if that buffer
  // ended on the bad boundary, rewind this transfer's start by 0x2000.
  bptr = bufPtr;
  if (hdwrBugFlag) {
    bptr = (u8*)bufPtr - 0x2000;
  }

  // Detect whether THIS buffer ends on the boundary that triggers the bug, and
  // remember it so the next call compensates.
  if ((((u32)bufPtr + size) & 0x1fff) == 0) {
    hdwrBugFlag = TRUE;
  } else {
    hdwrBugFlag = FALSE;
  }

  // Hand the AI a physical address; it cannot follow the TLB-mapped pointer.
  IO_WRITE(AI_DRAM_ADDR_REG, osVirtualToPhysical(bptr));
  IO_WRITE(AI_LEN_REG, size);
  return 0;
}
