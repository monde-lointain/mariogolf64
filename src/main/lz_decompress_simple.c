#include "common.h"

typedef struct {
  /* 0x00 */ u8* src_cur;
  /* 0x04 */ u8* dst_start;
  /* 0x08 */ u8* dst_cur;
  /* 0x0C */ u8* dst_alt;
  /* 0x10 */ u8* src_end;
  /* 0x14 */ u16 flags;
  /* 0x16 */ u16 unk16;
  /* 0x18 */ s32 unk18;
  /* 0x1C */ s32 unk1C;
  /* 0x20 */ s32 unk20;
  /* 0x24 */ s32 unk24;
} LzDecompressState;

extern OSPiHandle* nuPiCartHandle;

s32 lz_decompress_simple(LzDecompressState* param_1) {
  u16 uVar1;
  u16 uVar2;
  u16 uVar3;
  u16* puVar4;
  u32 acc;
  u16* puVar6;
  u16* puVar7;
  u16* puVar8;
  u16* puVar9;
  u16* puVar10;
  u16* puVar11;
  u16* puVar12;
  u16 desc;
  u32 t4;
  if (param_1->flags & 1) {
    puVar10 = (u16*)param_1->src_cur;
    puVar10 += 2;
    puVar4 = (u16*)param_1->dst_start;
  } else {
    puVar10 = (u16*)param_1->src_cur;
    puVar4 = (u16*)param_1->dst_alt;
  }
  uVar1 = param_1->flags;
  puVar11 = (u16*)param_1->src_end;
  puVar8 = puVar10 + 0xf;
  t4 = ((uVar1 >> 1) ^ 1) & 1;
loop_top:
  if (puVar10 < (puVar11 + (-0x10))) {
    goto body;
  }

  if (t4 == 0) {
    goto body;
  }
  puVar6 = (u16*)param_1->dst_cur;
  while (puVar10 < puVar11) {
    puVar11 = puVar11 + (-1);
    puVar6 = puVar6 + (-1);
    *puVar6 = *puVar11;
  }

  param_1->src_cur = (u8*)puVar6;
  param_1->dst_alt = (u8*)puVar4;
  return -1;
body:
  acc = *puVar10;

  puVar8 = puVar8 + 1;
  puVar10 = puVar10 + 1;
  if (acc != 0) {
    goto decode;
  }
  uVar2 = *puVar10;
  uVar3 = puVar8[-0xe];
  *puVar4 = uVar2;
  uVar2 = puVar8[-0xd];
  puVar4[1] = uVar3;
  uVar3 = puVar8[-0xc];
  puVar4[2] = uVar2;
  uVar2 = puVar8[-0xb];
  puVar4[3] = uVar3;
  uVar3 = puVar8[-0xa];
  puVar4[4] = uVar2;
  uVar2 = puVar8[-9];
  puVar4[5] = uVar3;
  uVar3 = puVar8[-8];
  puVar4[6] = uVar2;
  uVar2 = puVar8[-7];
  puVar4[7] = uVar3;
  uVar3 = puVar8[-6];
  puVar4[8] = uVar2;
  uVar2 = puVar8[-5];
  puVar4[9] = uVar3;
  uVar3 = puVar8[-4];
  puVar4[10] = uVar2;
  uVar2 = puVar8[-3];
  puVar4[0xb] = uVar3;
  uVar3 = puVar8[-2];
  puVar4[0xc] = uVar2;
  uVar2 = puVar8[-1];
  puVar4[0xd] = uVar3;
  uVar3 = *puVar8;
  puVar10 = puVar10 + 0x10;
  puVar8 = puVar8 + 0x10;
  puVar4[0xe] = uVar2;
  puVar4[0xf] = uVar3;
  puVar4 = puVar4 + 0x10;
  goto loop_top;
do_return:
  return (s32)puVar4 - (s32)param_1->dst_start;
decode:
  acc <<= 0x10;

  acc |= 0x8000;
  do {
    while (0 <= ((s32)acc)) {
      puVar8 = puVar8 + 1;
      uVar2 = *puVar10;
      puVar10 = puVar10 + 1;
      acc = acc << 1;
      *puVar4 = uVar2;
      puVar4 = puVar4 + 1;
    }

    acc = acc << 1;
    if (acc == 0) {
      goto loop_top;
    }
    desc = *puVar10;
    puVar8 = puVar8 + 1;
    puVar10 = puVar10 + 1;
    if (desc == 0) {
      goto do_return;
    }
    puVar12 = (u16*)(((s32)puVar4) - ((desc >> 5) << 1));
    puVar7 = puVar4 + ((desc & 0x1f) + 2);
    uVar2 = *puVar12;
    puVar12 = puVar12 + 1;
    puVar9 = puVar4 + 1;
    *puVar4 = uVar2;
    do {
      ;
      *puVar9 = *puVar12;
      puVar9 = puVar9 + 1;
      puVar12 = puVar12 + 1;
    } while (puVar9 != puVar7);
    puVar4 = puVar9;
  } while (1);
}

INCLUDE_ASM("asm/nonmatchings/main/lz_decompress_simple",
            lz_decompress_extended);

s32 lz_decompress_dma(u32 romAddr, void* dest, u32 length) {
  OSIoMesg mesg[2];
  OSMesgQueue queue;
  OSMesg mesgBuf[2];
  LzDecompressState state;
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
  state.src_cur = buf0;
  state.dst_cur = buf1;
  state.flags = 1;
  state.dst_start = (u8*)dest;
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
    state.dst_cur = bufs[recvIdx];
    state.src_end = (u8*)bufs[otherIdx] + 0x1000;
    if (inFlight == 0 && length == 0) {
      state.flags |= 2;
    }
    {
      s32 result = lz_decompress_simple(&state);
      recvCount--;
      if (result >= 0) {
        osWritebackDCache(dest, result);
        return result;
      }
      state.flags &= 0xFFFE;
    }
  }
}
