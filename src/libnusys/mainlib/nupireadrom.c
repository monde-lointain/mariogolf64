#include "common.h"

extern OSPiHandle *nuPiCartHandle;

void nuPiReadRom(u32 rom_addr, void *buf_ptr, u32 size) {
    OSIoMesg dmaIoMesgBuf;
    OSMesgQueue dmaMesgQ;
    OSMesg dmaMesgBuf;
    u32 readSize;

    osCreateMesgQueue(&dmaMesgQ, &dmaMesgBuf, 1);
    while (size) {
        if (size > 0x2000) {
            readSize = 0x2000;
        } else {
            readSize = size;
        }
        dmaIoMesgBuf.hdr.pri      = OS_MESG_PRI_NORMAL;
        dmaIoMesgBuf.hdr.retQueue = &dmaMesgQ;
        dmaIoMesgBuf.dramAddr     = buf_ptr;
        dmaIoMesgBuf.devAddr      = rom_addr;
        dmaIoMesgBuf.size         = readSize;
        osInvalDCache(buf_ptr, (s32)readSize);
        osEPiStartDma(nuPiCartHandle, &dmaIoMesgBuf, OS_READ);
        rom_addr += readSize;
        size -= readSize;
        (void)osRecvMesg(&dmaMesgQ, &dmaMesgBuf, OS_MESG_BLOCK);
        osInvalDCache(buf_ptr, (s32)readSize);
        buf_ptr = (void *)((u8 *)buf_ptr + readSize);
    }
}
