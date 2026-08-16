/* S319 restore body for lz_decompress_extended: 282/282 at the ROM's exact
 * -0x18 frame, instruction order identical, **10 cmpfn rows (cmpfn as of
 * S319)** of register naming left, down from S318's 20. SELF-CONTAINED: paste
 * this whole file over the INCLUDE_ASM stub in
 * src/main/lz_decompress_simple.c. It carries the LzHistoryState typedef and
 * the static inline lz_expand helper, both of which the S318 copy omitted --
 * without them the paste builds a 189-instruction function at -0x50.
 * The host also needs its S318 LzDecompressState field names.
 * See lz_decompress_extended.near-match.md. */

/* The extended decoder's frame-local window record (the ROM keeps it in the
 * 0x0..0xF span of its -0x18 frame: halfword run, word hist_base, halfword
 * hist_idx, pointer out). */
typedef struct {
  /* 0x0 */ u16 run;
  /* 0x4 */ s32 hist_base;
  /* 0x8 */ u16 hist_idx;
  /* 0xC */ u16* out;
} LzHistoryState;

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

/* lz_decompress_extended: the ring-buffered decoder, the file's last stub.
 * S318 carry at 282/282 instructions, the ROM's exact -0x18 frame, and an
 * instruction order identical to the ROM's once register names are normalised;
 * 28 cmpfn rows of register naming remain, all in local-alloc quantities.
 * The body is docs/wip/lz_decompress_extended.base.c and the measured state,
 * the six structural edits and the do{}while(0) allocation levers behind it are
 * in docs/wip/lz_decompress_extended.near-match.md. Do not re-derive from
 * scratch; do not inherit the S166 "raw-185 coloring floor" verdict, which was
 * measured on a body with six structural defects. */
/* Emits `run` halfwords from the history ring, either back-referenced by a
 * negative byte offset or forward from the output cursor itself. Each emitted
 * halfword also pushes the value it overwrites into the ring. */
static inline void lz_expand(LzHistoryState* q, s16 off, u32 run) {
  u16 v;
  u32 hi;
  u16* o;
  u16 next_run;

  if (off < 0) {
    if (run != 0) {
      do {
        v = *((u16*)((((((u32)q->hist_idx) + off) & 0xff) * 2) + q->hist_base));
        hi = q->hist_idx;
        q->hist_idx = q->hist_idx + 1;
        *((u16*)((hi * 2) + q->hist_base)) = *q->out;
        q->hist_idx = q->hist_idx & 0xff;
        o = q->out;
        q->out = o + 1;
        *o = v;
        q->run = q->run - 1;
      } while (q->run != 0);
    }
  } else if (run != 0) {
    do {
      hi = q->hist_idx;
      q->hist_idx = q->hist_idx + 1;
      *((u16*)((hi * 2) + q->hist_base)) = *q->out;
      q->hist_idx = q->hist_idx & 0xff;
      *q->out = q->out[off];
      q->out = q->out + 1;
      next_run = q->run - 1;
      q->run = next_run;
    } while (next_run != 0);
  }
}

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
  u32 word;
  u32 zx;
  u16 token;
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
  u16 run2;
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
  word = *src;
  src = src + 1;
  do {
    zx = (u16)word;
    do {
      if (zx == 0) {
        goto done;
      }
    } while (0);
  } while (0);
  /* Back-reference into the control ring: distance = word>>5 entries,
   * length = (word&0x1f)+1. */
  do {
  count = word & 0x1f;
  do {
  do {
  do {
  word = zx >> 5;
  } while (0);
  } while (0);
  } while (0);
  count = count + 1;
  do {
    ro = ridx & 0xffff;
    next_ridx = ridx + 1;
    ridx = next_ridx & 0x7ff;
    ctrl = *((u16*)((((ro - word) & 0x7ff) * 2) + ring));
    run2 = st.run;
    ro = ro * 2;
    ro = ro + ring;
    *((u16*)ro) = ctrl;
    if (run2 == 0) {
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
