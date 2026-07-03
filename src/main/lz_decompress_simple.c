#include "common.h"

typedef struct {
  /* 0x00 */ u8* unk00;
  /* 0x04 */ u32 unk04;
  /* 0x08 */ u8* unk08;
  /* 0x0C */ s32 unk0C;
  /* 0x10 */ s32 unk10;
  /* 0x14 */ u16 unk14;
  /* 0x16 */ u16 unk16;
  /* 0x18 */ s32 unk18;
  /* 0x1C */ s32 unk1C;
  /* 0x20 */ s32 unk20;
  /* 0x24 */ s32 unk24;
} DecompressState;

extern OSPiHandle* nuPiCartHandle;

INCLUDE_ASM("asm/nonmatchings/main/lz_decompress_simple", lz_decompress_simple);

INCLUDE_ASM("asm/nonmatchings/main/lz_decompress_simple",
            lz_decompress_extended);

extern s32 lz_decompress_simple(DecompressState* state);

s32 lz_decompress_dma(u32 romAddr, void* dest, u32 length) {
  OSIoMesg mesg[2];
  OSMesgQueue queue;
  OSMesg mesgBuf[2];
  DecompressState state;
  u8 buf0store[4160];
  u8 buf1store[4160];
  void* bufs[2];
  OSMesg* msgArr;

  s32 recvCount = 0;
  u32 bufIdx = 0;
  s32 inFlight = 0;
  u32 recvIdx = 0;
  u8* buf0 = (u8*)((((u32)buf0store + 0xF) & ~0xF) + 0x20);
  u8* buf1 = (u8*)((((u32)buf1store + 0xF) & ~0xF) + 0x20);

  bufs[0] = buf0;
  bufs[1] = buf1;
  state.unk00 = buf0;
  state.unk08 = buf1;
  state.unk14 = 1;
  state.unk04 = (u32)dest;
  osCreateMesgQueue(&queue, mesgBuf, 2);
  msgArr = mesgBuf;

  for (;;) {
    u32 otherIdx;

    if (length != 0) {
      u32 nbytes = length;
      OSIoMesg* iomsg;
      void* dram;

      if (nbytes > 0x1000) {
        nbytes = 0x1000;
      }
      iomsg = &mesg[bufIdx];
      iomsg->hdr.pri = 0;
      iomsg->hdr.retQueue = &queue;
      dram = bufs[bufIdx];
      length -= nbytes;
      iomsg->devAddr = romAddr;
      romAddr += nbytes;
      iomsg->size = nbytes;
      iomsg->dramAddr = dram;
      inFlight++;
      osInvalDCache(bufs[bufIdx], nbytes);
      bufIdx ^= 1;
      osEPiStartDma(nuPiCartHandle, iomsg, OS_READ);
    }

    if (osRecvMesg(&queue, msgArr + recvIdx, OS_MESG_NOBLOCK) != -1) {
      recvIdx ^= 1;
      recvCount++;
      inFlight--;
    }

    if (recvCount == 0) {
      if (inFlight == 2 || length == 0) {
        osRecvMesg(&queue, msgArr + recvIdx, OS_MESG_BLOCK);
        recvIdx ^= 1;
        recvCount = 1;
        inFlight--;
      }
      if (recvCount == 0) {
        continue;
      }
    }

    otherIdx = recvIdx ^ 1;
    osInvalDCache(bufs[otherIdx], mesg[otherIdx].size);
    state.unk08 = bufs[recvIdx];
    state.unk10 = (s32)bufs[otherIdx] + 0x1000;
    if (inFlight == 0 && length == 0) {
      state.unk14 |= 2;
    }
    {
      s32 result = lz_decompress_simple(&state);
      recvCount--;
      if (result >= 0) {
        osWritebackDCache(dest, result);
        return result;
      }
      state.unk14 &= 0xFFFE;
    }
  }
}
