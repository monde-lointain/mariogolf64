/* S318 carry body for lz_decompress_extended: 282/282 at the ROM's exact -0x18
 * frame, instruction order identical, 28 cmpfn rows of register naming left.
 * Paste over the INCLUDE_ASM stub in src/main/lz_decompress_simple.c; it also
 * needs that file's S318 LzDecompressState field names and the LzHistoryState
 * typedef. See lz_decompress_extended.near-match.md. */

/*
 * The extended (ring-buffered) decoder: same bit-accumulator stream as
 * lz_decompress_simple above, but every emitted halfword also passes through a
 * 2 KiB control ring and a 256-entry history window, so a token can back-
 * reference either.
 *
 * CONTROL-FLOW NOTE: the gotos are load-bearing here for the same reason they
 * are in lz_decompress_simple (see the note above that function).
 *
 * REGISTER-ALLOCATION NOTE: the bare `do { ... } while (0)` wrappers below emit
 * no instruction. GCC 2.7.2 weights a pseudo's reference count by loop depth
 * (flow.c), and both allocation passes rank by
 * floor_log2(n_refs) * n_refs / live_length (global.c allocno_compare,
 * local-alloc.c qty_compare), so a wrapper multiplies the references of exactly
 * what it encloses and nothing else. Each one below is placed to move one
 * allocno past another and reproduce the ROM's register order; see
 * docs/wip/lz_decompress_extended.near-match.md for which allocno each moves.
 */
s32 lz_decompress_extended(LzDecompressState* state) {
  u16* src;
  u16* end;
  s32 ring;
  s32 is_final;
  u32 bits;
  u32 ridx;
  u16 flags;
  u16 ctrl;
  u16 token;
  u32 word;
  u32 count;
  s32 off;
  u32 ro;
  u32 ho;
  u32 run;
  u16 tmp;
  u16* out0;
  u16* init_out;
  u16* dst;
  u16 hist_idx;
  u16 run_left;
  u16* save_out;
  u16 save_run;
  u16 save_hist_idx;
  u32 next_ridx;
  s32 hist_base;
  LzHistoryState st;
  LzHistoryState* q;
  u32 marker;

  flags = state->flags;
  end = (u16*)state->src_end;
  is_final = ((flags >> 1) ^ 1) & 1;
  if ((flags & 1) != 0) {
    /* Continuation window: skip the 2-halfword block header and restart the
     * ring and history from empty. */
    src = (u16*)state->src_cur;
    src = src + 2;
    ring = state->ring;
    init_out = (u16*)state->dst_start;
    hist_base = state->hist_base;
    ridx = 0;
    bits = 0x80000000;
    st.hist_idx = 0;
    st.run = 0;
    st.out = init_out;
    st.hist_base = hist_base;
    goto decode_entry;
  }
  src = (u16*)state->src_cur;
  ring = state->ring;
  ridx = state->ring_idx;
  hist_base = state->hist_base;
  hist_idx = state->hist_idx;
  run_left = state->run;
  st.out = (u16*)state->dst_alt;
  st.hist_idx = hist_idx;
  st.run = run_left;
  st.hist_base = hist_base;

dispatch:
  if (src < (end + (-0x10))) {
    goto read_word;
  }
  if (is_final == 0) {
    goto read_word;
  }
  /* Near the window end and not the final block: flush the unconsumed tail
   * backwards into the carry buffer, save the ring/history cursors and ask the
   * caller for more input. */
  dst = (u16*)state->dst_cur;
  while (src < end) {
    do {
      end = end + (-1);
      dst = dst + (-1);
      *dst = *end;
    } while (0);
  }
  save_out = st.out;
  save_run = st.run;
  save_hist_idx = st.hist_idx;
  state->src_cur = (u8*)dst;
  state->ring_idx = ridx;
  state->dst_alt = (u8*)save_out;
  state->run = save_run;
  state->hist_idx = save_hist_idx;
  return -1;

done:
  return ((s32)st.out) - (s32)state->dst_start;

read_word:
  bits = *src;
  bits = bits << 0x10;
  bits = bits | 0x8000;
  src = src + 1;

decode_entry:
  q = &st;
  marker = 0x8000;

decode_top:
  do {
  if ((-1) < ((s32)bits)) {
    do {
      /* Top bit clear: one literal halfword, pushed through the control
       * ring. */
      ctrl = *src;
      src = src + 1;
      next_ridx = ridx + 1;
      ro = ridx * 2;
      run_left = st.run;
      ridx = next_ridx & 0x7ff;
      ro = ro + ring;
      *((u16*)ro) = ctrl;
      if (run_left == 0) {
        run = (ctrl & 0xff) + 1;
        st.run = run;
        tmp = ctrl & 0xff00;
        if (tmp != marker) {
          do {
            off = ((s32)(((u32)(ctrl & 0xff00)) << 0x10)) >> 0x18;
            lz_expand(q, off, run);
          } while (0);
        }
      } else {
        ho = st.hist_idx;
        st.hist_idx = st.hist_idx + 1;
        tmp = *st.out;
        st.hist_idx = st.hist_idx & 0xff;
        out0 = st.out;
        st.out = out0 + 1;
        *((u16*)((ho * 2) + st.hist_base)) = tmp;
        *out0 = ctrl;
        st.run = st.run - 1;
      }
      bits = bits << 1;
    } while ((-1) < ((s32)bits));
  }
  } while (0);

  do {
    bits = bits << 1;
  } while (0);
  if (bits == 0) {
    goto dispatch;
  }
  token = *src;
  src = src + 1;
  do {
    word = token;
    do {
      if (word == 0) {
        goto done;
      }
    } while (0);
  } while (0);
  /* Back-reference into the control ring: distance = word>>5 entries,
   * length = (token&0x1f)+1. */
  do {
  count = token & 0x1f;
  word = word >> 5;
  count = count + 1;
  do {
    ro = ridx & 0xffff;
    next_ridx = ridx + 1;
    ridx = next_ridx & 0x7ff;
    ctrl = *((u16*)((((ro - word) & 0x7ff) * 2) + ring));
    run_left = st.run;
    ro = (ro * 2) + ring;
    *((u16*)ro) = ctrl;
    if (run_left == 0) {
      run = (ctrl & 0xff) + 1;
      st.run = run;
      if ((ctrl & 0xff00) != marker) {
        do {
          off = ((s32)(((u32)(ctrl & 0xff00)) << 0x10)) >> 0x18;
          lz_expand(q, off, run);
        } while (0);
      }
    } else {
      ho = st.hist_idx;
      st.hist_idx = st.hist_idx + 1;
      tmp = *st.out;
      st.hist_idx = st.hist_idx & 0xff;
      out0 = st.out;
      st.out = out0 + 1;
      *((u16*)((ho * 2) + st.hist_base)) = tmp;
      *out0 = ctrl;
      st.run = st.run - 1;
    }
    if (count == 0) {
      break;
    }
    count = count - 1;
  } while (1);
  } while (0);
  goto decode_top;
}

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
