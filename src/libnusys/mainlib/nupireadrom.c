/*
 * Read a span of cartridge ROM into RDRAM via PI DMA, transferring it in
 * bounded chunks and waiting for each chunk to finish before issuing the next.
 */

#include "common.h"

extern OSPiHandle* nuPiCartHandle;

/*
 * Copy `size` bytes from ROM offset rom_addr into buf_ptr. The transfer is
 * split into 8 KB pieces and run synchronously: each DMA is started, then the
 * completion message is awaited before advancing.
 */
void nuPiReadRom(u32 rom_addr, void* buf_ptr, u32 size) {
  OSIoMesg dmaIoMesgBuf;
  OSMesgQueue dmaMesgQ;
  OSMesg dmaMesgBuf;
  u32 readSize;

  osCreateMesgQueue(&dmaMesgQ, &dmaMesgBuf, 1);
  while (size) {
    // Cap each DMA at 8 KB so a single transfer never monopolizes the PI.
    if (size > 0x2000) {
      readSize = 0x2000;
    } else {
      readSize = size;
    }

    dmaIoMesgBuf.hdr.pri = OS_MESG_PRI_NORMAL;
    dmaIoMesgBuf.hdr.retQueue = &dmaMesgQ;
    dmaIoMesgBuf.dramAddr = buf_ptr;
    dmaIoMesgBuf.devAddr = rom_addr;
    dmaIoMesgBuf.size = readSize;

    // Invalidate before the DMA so no stale cached lines for this region get
    // written back over the data the DMA is about to deposit.
    osInvalDCache(buf_ptr, (s32)readSize);
    osEPiStartDma(nuPiCartHandle, &dmaIoMesgBuf, OS_READ);
    rom_addr += readSize;
    size -= readSize;

    // Block until the DMA reports done, then invalidate again so subsequent
    // reads see RDRAM rather than any line speculatively refilled meanwhile.
    (void)osRecvMesg(&dmaMesgQ, &dmaMesgBuf, OS_MESG_BLOCK);
    osInvalDCache(buf_ptr, (s32)readSize);
    buf_ptr = (void*)((u8*)buf_ptr + readSize);
  }
}
