/*
 * Transfer data between RDRAM and cartridge SRAM in either direction over PI
 * DMA. The whole body is compiled only when EPI SRAM support is enabled.
 */

#include <nusys.h>

/*
 * Synchronously DMA `size` bytes between buf_ptr and SRAM offset addr; flag is
 * OS_READ (SRAM -> RDRAM) or OS_WRITE (RDRAM -> SRAM).
 */
void nuPiReadWriteSram(u32 addr, void* buf_ptr, u32 size, s32 flag) {
#ifdef USE_EPI
  OSIoMesg dmaIoMesgBuf;
  OSMesgQueue dmaMesgQ;
  OSMesg dmaMesgBuf;

  // Debug builds warn about the alignment SRAM DMA requires: even SRAM address,
  // 8-byte-aligned RDRAM buffer, and even byte count.
#ifdef NU_DEBUG
  if (addr & 0x00000001) {
    osSyncPrintf("nuPiReadWriteSram: SRAM address is odd!!\n");
  }
  if ((u32)buf_ptr & 0x07) {
    osSyncPrintf("nuPiReadWriteSram: RDRAM address must align 8!!\n");
  }
  if (size & 0x00000001) {
    osSyncPrintf("nuPiReadWriteSram: DMA size is odd!!\n");
  }
#endif

  osCreateMesgQueue(&dmaMesgQ, &dmaMesgBuf, 1);
  dmaIoMesgBuf.hdr.pri = OS_MESG_PRI_NORMAL;
  dmaIoMesgBuf.hdr.retQueue = &dmaMesgQ;
  dmaIoMesgBuf.dramAddr = buf_ptr;
  dmaIoMesgBuf.devAddr = addr;
  dmaIoMesgBuf.size = size;

  // Prepare the cache for the chosen direction: invalidate before a read so the
  // incoming data is seen, write back before a write so the buffer's latest
  // contents reach SRAM.
  if (flag == OS_READ) {
    osInvalDCache(buf_ptr, (s32)size);
  } else {
    osWritebackDCache(buf_ptr, (s32)size);
  }

  // Kick off the transfer and block until it completes.
  osEPiStartDma(nuPiSramHandle, &dmaIoMesgBuf, flag);
  (void)osRecvMesg(&dmaMesgQ, &dmaMesgBuf, OS_MESG_BLOCK);
#endif
}
