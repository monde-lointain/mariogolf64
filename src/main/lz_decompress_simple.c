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

/*
 * Streaming LZ decompressor over a sliding pair of 4 KiB DMA windows
 * (called by lz_decompress_dma below, once per filled window).
 *
 * CONTROL-FLOW NOTE: the gotos here are load-bearing for the byte-exact KMC
 * GCC 2.7.2 -O2 match and cannot be replaced with structured loops. Proven
 * against the compiler source
 * (docs/hazards.md#pervasive-regalloc-classical-main):
 *   - The outer dispatch is a goto-loop on purpose. A structured for(;;)/while
 *     emits NOTE_INSN_LOOP_BEG (stmt.c expand_start_loop), which (a) raises
 * every outer value's register-allocation loop-depth weight by one (flow.c
 *     reg_n_refs += loop_depth -> global.c allocno priority), repermuting hard
 *     registers, and (b) makes loop.c run invariant-motion + induction-variable
 *     strength reduction, adding ~5 instructions. A goto-loop emits no note, so
 *     the optimizer leaves it alone.
 *   - The forward gotos (skip_flush, decode, done) fix the out-of-line block
 *     placement and branch polarity; GCC 2.7.2 has no basic-block reorder pass,
 *     so physical layout follows source order and only a goto+label can put a
 *     block where control does not structurally fall through.
 */
s32 lz_decompress_simple(LzDecompressState* state) {
  u16 flags_raw;
  u16 w0;
  u16 w1;
  u16* dst;
  u32 bits;
  u16* tail_dst;
  u16* copy_end;
  u16* src_ahead;
  u16* copy_dst;
  u16* src;
  u16* src_end;
  u16* ref_src;
  u16 token;
  u32 is_final;

  if (state->flags & 1) {
    /* Continuation window: skip the 2-halfword block header. */
    src = (u16*)state->src_cur;
    src += 2;
    dst = (u16*)state->dst_start;
  } else {
    src = (u16*)state->src_cur;
    dst = (u16*)state->dst_alt;
  }
  flags_raw = state->flags;
  src_end = (u16*)state->src_end;
  src_ahead = src + 0xf;
  is_final = ((flags_raw >> 1) ^ 1) & 1;

dispatch:
  if (src < (src_end + (-0x10))) {
    goto skip_flush;
  }
  if (is_final == 0) {
    goto skip_flush;
  }
  /* Near the window end and not the final block: flush the unconsumed tail
   * backwards into the carry buffer and ask the caller for more input. */
  tail_dst = (u16*)state->dst_cur;
  while (src < src_end) {
    src_end = src_end + (-1);
    tail_dst = tail_dst + (-1);
    *tail_dst = *src_end;
  }
  state->src_cur = (u8*)tail_dst;
  state->dst_alt = (u8*)dst;
  return -1;

skip_flush:
  bits = *src;
  src_ahead = src_ahead + 1;
  src = src + 1;
  if (bits != 0) {
    goto decode;
  }
  /* Control word 0: copy a raw run of 16 halfwords. The paired w0/w1
   * temporaries and the src_ahead[-n] addressing reproduce the compiler's
   * load/store pairing. */
  w0 = *src;
  w1 = src_ahead[-0xe];
  *dst = w0;
  w0 = src_ahead[-0xd];
  dst[1] = w1;
  w1 = src_ahead[-0xc];
  dst[2] = w0;
  w0 = src_ahead[-0xb];
  dst[3] = w1;
  w1 = src_ahead[-0xa];
  dst[4] = w0;
  w0 = src_ahead[-9];
  dst[5] = w1;
  w1 = src_ahead[-8];
  dst[6] = w0;
  w0 = src_ahead[-7];
  dst[7] = w1;
  w1 = src_ahead[-6];
  dst[8] = w0;
  w0 = src_ahead[-5];
  dst[9] = w1;
  w1 = src_ahead[-4];
  dst[10] = w0;
  w0 = src_ahead[-3];
  dst[0xb] = w1;
  w1 = src_ahead[-2];
  dst[0xc] = w0;
  w0 = src_ahead[-1];
  dst[0xd] = w1;
  w1 = *src_ahead;
  src = src + 0x10;
  src_ahead = src_ahead + 0x10;
  dst[0xe] = w0;
  dst[0xf] = w1;
  dst = dst + 0x10;
  goto dispatch;

done:
  return (s32)dst - (s32)state->dst_start;

decode:
  /* Compressed token stream: `bits` is a bit accumulator with a 0x8000 sentinel
   * marking when the low 16 bits are exhausted. */
  bits <<= 0x10;
  bits |= 0x8000;
  do {
    while (0 <= ((s32)bits)) {
      /* Top bit clear: emit one literal halfword. */
      src_ahead = src_ahead + 1;
      w0 = *src;
      src = src + 1;
      bits = bits << 1;
      *dst = w0;
      dst = dst + 1;
    }

    bits = bits << 1;
    if (bits == 0) {
      goto dispatch;
    }
    token = *src;
    src_ahead = src_ahead + 1;
    src = src + 1;
    if (token == 0) {
      goto done;
    }
    /* Back-reference: distance = token>>5 halfwords, length = (token&0x1f)+2.
     */
    ref_src = (u16*)(((s32)dst) - ((token >> 5) << 1));
    copy_end = dst + ((token & 0x1f) + 2);
    w0 = *ref_src;
    ref_src = ref_src + 1;
    copy_dst = dst + 1;
    *dst = w0;
    do {
      *copy_dst = *ref_src;
      copy_dst = copy_dst + 1;
      ref_src = ref_src + 1;
    } while (copy_dst != copy_end);
    dst = copy_dst;
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
