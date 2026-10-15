#include "common.h"

/* Preserve the observed target offsets and the two distinct destination roles.
 */
typedef struct {
  /* 0x00 */ u8* source_cursor;
  /* 0x04 */ u8* output_start;
  /* 0x08 */ u8* carry_buffer_start;
  /* 0x0C */ u8* output_cursor;
  /* 0x10 */ u8* source_end;
  /* 0x14 */ u16 window_flags;
  /* 0x16 */ u16 unknown_16;
  /* 0x18 */ s32 ring_buffer_base;
  /* 0x1C */ u16 ring_index;
  /* 0x1E */ u16 run_remaining;
  /* 0x20 */ u16 history_index;
  /* 0x24 */ s32 history_buffer_base;
} LzDecompressState;

/* Frame-local history state; its ROM-sized layout is retained. */
typedef struct {
  /* 0x0 */ u16 run_remaining;
  /* 0x4 */ s32 history_buffer_base;
  /* 0x8 */ u16 history_index;
  /* 0xC */ u16* output;
} LzHistoryState;

extern OSPiHandle* nuPiCartHandle;

/*
 * Streaming LZ decompressor over a sliding pair of 4 KiB DMA windows.
 * Separate dispatch edges preserve retail branch placement; paired raw copies
 * preserve load/store scheduling and overlapping-copy behavior.
 */
s32 lz_decompress_simple(LzDecompressState* state) {
  u16 window_flags;
  u16 current_word;
  u16 next_word;
  u16* output_cursor;
  u32 bit_accumulator;
  u16* carry_cursor;
  u16* copy_end;
  u16* lookahead_cursor;
  u16* copy_cursor;
  u16* source_cursor;
  u16* source_end;
  u16* reference_cursor;
  u16 token;
  u32 is_nonfinal_window;

  if (state->window_flags & 1) {
    /* The first window skips the two-halfword stream header. */
    source_cursor = (u16*)state->source_cursor;
    source_cursor += 2;
    output_cursor = (u16*)state->output_start;
  } else {
    source_cursor = (u16*)state->source_cursor;
    output_cursor = (u16*)state->output_cursor;
  }
  window_flags = state->window_flags;
  source_end = (u16*)state->source_end;
  lookahead_cursor = source_cursor + 0xF;
  is_nonfinal_window = ((window_flags >> 1) ^ 1) & 1;

dispatch:
  if (source_cursor >= (source_end + (-0x10))) {
    if (is_nonfinal_window != 0) {
      /* Carry unread input backward into the current DMA buffer. */
      carry_cursor = (u16*)state->carry_buffer_start;
      while (source_cursor < source_end) {
        source_end = source_end + (-1);
        carry_cursor = carry_cursor + (-1);
        *carry_cursor = *source_end;
      }
      state->source_cursor = (u8*)carry_cursor;
      state->output_cursor = (u8*)output_cursor;
      return -1;
    }
  }

  bit_accumulator = *source_cursor;
  lookahead_cursor = lookahead_cursor + 1;
  source_cursor = source_cursor + 1;
  if (bit_accumulator != 0) {
    goto decode;
  }
  /* Preserve the paired load/store sequence for a raw 16-halfword block. */
  current_word = *source_cursor;
  next_word = lookahead_cursor[-0xe];
  *output_cursor = current_word;
  current_word = lookahead_cursor[-0xd];
  output_cursor[1] = next_word;
  next_word = lookahead_cursor[-0xc];
  output_cursor[2] = current_word;
  current_word = lookahead_cursor[-0xb];
  output_cursor[3] = next_word;
  next_word = lookahead_cursor[-0xa];
  output_cursor[4] = current_word;
  current_word = lookahead_cursor[-9];
  output_cursor[5] = next_word;
  next_word = lookahead_cursor[-8];
  output_cursor[6] = current_word;
  current_word = lookahead_cursor[-7];
  output_cursor[7] = next_word;
  next_word = lookahead_cursor[-6];
  output_cursor[8] = current_word;
  current_word = lookahead_cursor[-5];
  output_cursor[9] = next_word;
  next_word = lookahead_cursor[-4];
  output_cursor[10] = current_word;
  current_word = lookahead_cursor[-3];
  output_cursor[0xb] = next_word;
  next_word = lookahead_cursor[-2];
  output_cursor[0xc] = current_word;
  current_word = lookahead_cursor[-1];
  output_cursor[0xd] = next_word;
  next_word = *lookahead_cursor;
  source_cursor = source_cursor + 0x10;
  lookahead_cursor = lookahead_cursor + 0x10;
  output_cursor[0xe] = current_word;
  output_cursor[0xf] = next_word;
  output_cursor = output_cursor + 0x10;
  goto dispatch;

done:
  return (s32)output_cursor - (s32)state->output_start;

decode:
  /* The 0x8000 sentinel marks exhaustion of this control word. */
  bit_accumulator <<= 0x10;
  bit_accumulator |= 0x8000;
  while (1) {
    while (0 <= ((s32)bit_accumulator)) {
      /* A clear bit emits one literal halfword. */
      lookahead_cursor = lookahead_cursor + 1;
      current_word = *source_cursor;
      source_cursor = source_cursor + 1;
      bit_accumulator = bit_accumulator << 1;
      *output_cursor = current_word;
      output_cursor = output_cursor + 1;
    }

    bit_accumulator = bit_accumulator << 1;
    if (bit_accumulator == 0) {
      goto dispatch;
    }
    token = *source_cursor;
    lookahead_cursor = lookahead_cursor + 1;
    source_cursor = source_cursor + 1;
    if (token == 0) {
      goto done;
    }
    /* Back-reference distance is in halfwords; forward copy permits overlap. */
    reference_cursor = output_cursor - (token >> 5);
    copy_end = output_cursor + ((token & 0x1f) + 2);
    current_word = *reference_cursor;
    reference_cursor = reference_cursor + 1;
    copy_cursor = output_cursor + 1;
    *output_cursor = current_word;
    do {
      *copy_cursor = *reference_cursor;
      copy_cursor = copy_cursor + 1;
      reference_cursor = reference_cursor + 1;
    } while (copy_cursor != copy_end);
    output_cursor = copy_cursor;
  }
}

/* Emit a run through the circular history window or forward from output. */
static inline void lz_expand(LzHistoryState* history, s16 history_offset,
                             u32 run_length) {
  u16 history_value;
  u32 history_slot;
  u16* output_cursor;
  u16 next_run;

  if (history_offset < 0) {
    if (run_length != 0) {
      do {
        history_value = *(
            (u16*)((((((u32)history->history_index) + history_offset) & 0xff) *
                    2) +
                   history->history_buffer_base));
        history_slot = history->history_index;
        history->history_index = history->history_index + 1;
        *((u16*)((history_slot * 2) + history->history_buffer_base)) =
            *history->output;
        history->history_index = history->history_index & 0xff;
        output_cursor = history->output;
        history->output = output_cursor + 1;
        *output_cursor = history_value;
        history->run_remaining = history->run_remaining - 1;
      } while (history->run_remaining != 0);
    }
  } else if (run_length != 0) {
    do {
      history_slot = history->history_index;
      history->history_index = history->history_index + 1;
      *((u16*)((history_slot * 2) + history->history_buffer_base)) =
          *history->output;
      history->history_index = history->history_index & 0xff;
      *history->output = history->output[history_offset];
      history->output = history->output + 1;
      next_run = history->run_remaining - 1;
      history->run_remaining = next_run;
    } while (next_run != 0);
  }
}

/*
 * The extended decoder routes each control halfword through a 2 KiB ring and
 * a 256-halfword history window.
 *
 * Keep per-block scratch values distinct, along with the entry scheduling
 * barrier and no-op loop wrappers: they are present in the measured ROM form.
 */
s32 lz_decompress_extended(LzDecompressState* state) {
  u16* source_cursor;
  u16* source_end;
  s32 ring_buffer_base;
  s32 is_nonfinal_window;
  u32 bit_accumulator;
  u32 ring_index;
  u16 window_flags;
  u16 control_word;
  u32 reference_word;
  u32 zero_extended_word;
  u32 reference_count;
  s32 history_offset;
  u32 ring_slot_index;
  u32 history_slot_index;
  u32 run_length;
  /* This shared scratch keeps the retail allocation across token paths. */
  u16 word_scratch;
  u16* output_slot;
  u16* initial_output;
  u16* carry_cursor;
  u16 history_index;
  u16 literal_run;
  u16 reference_run;
  u32 literal_slot_offset;
  u32 reference_slot_offset;
  u32 reference_slot;
  u32 literal_slot;
  u32 literal_next_ring_index;
  u16 initial_run_remaining;
  u16* initial_output_cursor;
  u16* saved_output_cursor;
  u16 saved_run_remaining;
  u16 saved_history_index;
  u32 next_ring_index;
  s32 history_buffer_base;
  s32 entry_history_buffer_base;
  LzHistoryState history;
  LzHistoryState* history_state;
  u32 run_marker;

  window_flags = state->window_flags;
  source_end = (u16*)state->source_end;
  is_nonfinal_window = ((window_flags >> 1) ^ 1) & 1;
  if ((window_flags & 1) != 0) {
    /* The first window skips its header and resets both histories. */
    source_cursor = (u16*)state->source_cursor;
    source_cursor = source_cursor + 2;
    ring_buffer_base = state->ring_buffer_base;
    initial_output = (u16*)state->output_start;
    history_buffer_base = state->history_buffer_base;
    ring_index = 0;
    bit_accumulator = 0x80000000;
    history.history_index = 0;
    history.run_remaining = 0;
    history.output = initial_output;
    history.history_buffer_base = history_buffer_base;
    goto decode_entry;
  }
  initial_output_cursor = (u16*)state->output_cursor;
  __asm__ __volatile__("");
  source_cursor = (u16*)state->source_cursor;
  ring_buffer_base = state->ring_buffer_base;
  ring_index = state->ring_index;
  history_index = state->history_index;
  initial_run_remaining = state->run_remaining;
  entry_history_buffer_base = state->history_buffer_base;
  do {
    do {
      history.output = initial_output_cursor;
    } while (0);
  } while (0);
  do {
    history.history_index = history_index;
  } while (0);
  history.run_remaining = initial_run_remaining;
  history.history_buffer_base = entry_history_buffer_base;

dispatch:
  if (source_cursor < (source_end + (-0x10))) {
    goto read_word;
  }
  if (is_nonfinal_window == 0) {
    goto read_word;
  }
  /* Carry unread input backward and save the ring/history cursors. */
  carry_cursor = (u16*)state->carry_buffer_start;
  while (source_cursor < source_end) {
    do {
      source_end = source_end + (-1);
      carry_cursor = carry_cursor + (-1);
      *carry_cursor = *source_end;
    } while (0);
  }
  saved_output_cursor = history.output;
  saved_run_remaining = history.run_remaining;
  saved_history_index = history.history_index;
  state->source_cursor = (u8*)carry_cursor;
  state->ring_index = ring_index;
  state->output_cursor = (u8*)saved_output_cursor;
  state->run_remaining = saved_run_remaining;
  state->history_index = saved_history_index;
  return -1;

done:
  return ((s32)history.output) - (s32)state->output_start;

read_word:
  bit_accumulator = *source_cursor;
  bit_accumulator = bit_accumulator << 0x10;
  bit_accumulator = bit_accumulator | 0x8000;
  source_cursor = source_cursor + 1;

decode_entry:
  history_state = &history;
  run_marker = 0x8000;

decode_top:
  do {
    if ((-1) < ((s32)bit_accumulator)) {
      do {
        /* Literal control halfword: append to the ring before interpretation.
         */
        control_word = *source_cursor;
        source_cursor = source_cursor + 1;
        literal_next_ring_index = ring_index + 1;
        literal_slot_offset = ring_index * 2;
        literal_run = history.run_remaining;
        ring_index = literal_next_ring_index & 0x7ff;
        literal_slot = literal_slot_offset + ring_buffer_base;
        *((u16*)literal_slot) = control_word;
        if (literal_run == 0) {
          run_length = (control_word & 0xff) + 1;
          history.run_remaining = run_length;
          word_scratch = control_word & 0xff00;
          if (word_scratch != run_marker) {
            do {
              history_offset =
                  ((s32)(((u32)(control_word & 0xff00)) << 0x10)) >> 0x18;
              lz_expand(history_state, history_offset, run_length);
            } while (0);
          }
        } else {
          history_slot_index = history.history_index;
          history.history_index = history.history_index + 1;
          word_scratch = *history.output;
          history.history_index = history.history_index & 0xff;
          output_slot = history.output;
          history.output = output_slot + 1;
          *((u16*)((history_slot_index * 2) + history.history_buffer_base)) =
              word_scratch;
          *output_slot = control_word;
          history.run_remaining = history.run_remaining - 1;
        }
        bit_accumulator = bit_accumulator << 1;
      } while ((-1) < ((s32)bit_accumulator));
    }
  } while (0);

  do {
    bit_accumulator = bit_accumulator << 1;
  } while (0);
  if (bit_accumulator == 0) {
    goto dispatch;
  }
  reference_word = *source_cursor;
  source_cursor = source_cursor + 1;
  do {
    zero_extended_word = (u16)reference_word;
    do {
      if (zero_extended_word == 0) {
        goto done;
      }
    } while (0);
  } while (0);
  /* Ring back-reference distance is in control-ring entries. */
  do {
    reference_count = reference_word & 0x1f;
    do {
      do {
        do {
          reference_word = zero_extended_word >> 5;
        } while (0);
      } while (0);
    } while (0);
    reference_count = reference_count + 1;
    while (1) {
      ring_slot_index = ring_index & 0xffff;
      next_ring_index = ring_index + 1;
      ring_index = next_ring_index & 0x7ff;
      control_word =
          *((u16*)((((ring_slot_index - reference_word) & 0x7ff) * 2) +
                   ring_buffer_base));
      reference_run = history.run_remaining;
      reference_slot_offset = ring_slot_index * 2;
      reference_slot = reference_slot_offset + ring_buffer_base;
      *((u16*)reference_slot) = control_word;
      if (reference_run == 0) {
        run_length = (control_word & 0xff) + 1;
        history.run_remaining = run_length;
        if ((control_word & 0xff00) != run_marker) {
          do {
            history_offset =
                ((s32)(((u32)(control_word & 0xff00)) << 0x10)) >> 0x18;
            lz_expand(history_state, history_offset, run_length);
          } while (0);
        }
      } else {
        history_slot_index = history.history_index;
        history.history_index = history.history_index + 1;
        word_scratch = *history.output;
        history.history_index = history.history_index & 0xff;
        output_slot = history.output;
        history.output = output_slot + 1;
        *((u16*)((history_slot_index * 2) + history.history_buffer_base)) =
            word_scratch;
        *output_slot = control_word;
        history.run_remaining = history.run_remaining - 1;
      }
      if (reference_count == 0) {
        break;
      }
      reference_count = reference_count - 1;
    }
  } while (0);
  goto decode_top;
}

/* Stream compressed ROM bytes through the simple decoder's double DMA window.
 */
s32 lz_decompress_dma(u32 rom_address, void* destination, u32 remaining_bytes) {
  OSIoMesg dma_messages[2];
  OSMesgQueue dma_queue;
  OSMesg message_buffer[2];
  LzDecompressState state;
  u8 first_buffer_storage[4160];
  u8 second_buffer_storage[4160];
  void* buffers[2];
  OSMesg* message_array;

  s32 received_count = 0;
  u32 next_dma_buffer = 0;
  s32 in_flight_count = 0;
  u32 receive_index = 0;
  u8* first_buffer = (u8*)((((u32)first_buffer_storage + 0xF) & ~0xF) + 0x20);
  u8* second_buffer = (u8*)((((u32)second_buffer_storage + 0xF) & ~0xF) + 0x20);

  buffers[0] = first_buffer;
  buffers[1] = second_buffer;
  state.source_cursor = first_buffer;
  state.carry_buffer_start = second_buffer;
  state.window_flags = 1;
  state.output_start = (u8*)destination;
  osCreateMesgQueue(&dma_queue, message_buffer, 2);
  message_array = message_buffer;

  for (;;) {
    u32 other_buffer_index;

    if (remaining_bytes != 0) {
      u32 dma_size = remaining_bytes;
      OSIoMesg* dma_message;
      void* dram_address;

      if (dma_size > 0x1000) {
        dma_size = 0x1000;
      }
      dma_message = &dma_messages[next_dma_buffer];
      dma_message->hdr.pri = 0;
      dma_message->hdr.retQueue = &dma_queue;
      dram_address = buffers[next_dma_buffer];
      remaining_bytes -= dma_size;
      dma_message->devAddr = rom_address;
      rom_address += dma_size;
      dma_message->size = dma_size;
      dma_message->dramAddr = dram_address;
      in_flight_count++;
      osInvalDCache(buffers[next_dma_buffer], dma_size);
      next_dma_buffer ^= 1;
      osEPiStartDma(nuPiCartHandle, dma_message, OS_READ);
    }

    if (osRecvMesg(&dma_queue, message_array + receive_index,
                   OS_MESG_NOBLOCK) != -1) {
      receive_index ^= 1;
      received_count++;
      in_flight_count--;
    }

    if (received_count == 0) {
      if (in_flight_count == 2 || remaining_bytes == 0) {
        osRecvMesg(&dma_queue, message_array + receive_index, OS_MESG_BLOCK);
        receive_index ^= 1;
        received_count = 1;
        in_flight_count--;
      }
      if (received_count == 0) {
        continue;
      }
    }

    other_buffer_index = receive_index ^ 1;
    osInvalDCache(buffers[other_buffer_index],
                  dma_messages[other_buffer_index].size);
    state.carry_buffer_start = buffers[receive_index];
    state.source_end = (u8*)buffers[other_buffer_index] + 0x1000;
    if (in_flight_count == 0 && remaining_bytes == 0) {
      state.window_flags |= 2;
    }
    {
      s32 result = lz_decompress_simple(&state);

      received_count--;
      if (result >= 0) {
        osWritebackDCache(destination, result);
        return result;
      }
      state.window_flags &= 0xFFFE;
    }
  }
}
