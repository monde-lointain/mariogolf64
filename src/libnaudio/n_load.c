/*
 * n_load.c
 *
 * n_audio (microcode-variant) ADPCM sample load/decode filter. Pulls
 * compressed ADPCM sample data out of DRAM through the voice's DMA callback
 * and emits the RSP command sequence that decompresses it into the
 * synthesizer's DMEM decode buffer. It tracks loop points and the 16-sample
 * ADPCM frame boundary so a sample can span several command-list frames, and
 * zero-fills any tail past the end of the sample. n_alLoadParam binds a
 * wavetable to a voice and resets its decode state.
 */

#include "n_synthInternals.h"
#include <os.h>
#include <R4300.h>

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

/* Profiling counters; compiled in only for instrumented AUD_PROFILE builds. */
#ifdef AUD_PROFILE
extern u32 cnt_index, adpcm_num, adpcm_cnt, adpcm_max, adpcm_min, lastCnt[];
#endif

#define ADPCMFBYTES 9  // compressed byte size of one 16-sample ADPCM frame
#define LFSAMPLES 4    // log2(16): shift converting ADPCM frames <-> samples

static Acmd* _decodeChunk(Acmd* ptr, N_PVoice* f, s32 tsam, s32 nbytes,
                          s16 outp, s16 inp, u32 flags);

/*
 * Builds the RSP command list that decodes outCount samples of voice `filter`
 * into the DMEM decode buffer at *outp, advancing the voice's decode and loop
 * state. Walks the sample's loop region as many times as needed, decoding one
 * loop-bounded chunk per pass and stitching the pieces together in DMEM. On
 * return *outp has been bumped past the leading partial frame so the next
 * filter reads from the first real output sample. Returns the advanced command
 * pointer.
 */
Acmd* n_alAdpcmPull(N_PVoice* filter, s16* outp, s32 outCount, Acmd* p) {
  Acmd* ptr = p;
  s16 inp;
  s32 tsam;
  s32 nframes;
  s32 nbytes;
  s32 overFlow;
  s32 startZero;
  s32 nOver;
  s32 nSam;
  s32 op;
  s32 nLeft;
  s32 bEnd;
  s32 decoded = 0;
  s32 looped = 0;
  N_PVoice* f = filter;
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif
  // Nothing requested: emit no commands.
  if (outCount == 0) return ptr;

  // The decoder reads its compressed input from the decoder-input DMEM buffer.
#ifndef N_MICRO
  inp = AL_DECODER_IN;
#else
  inp = N_AL_DECODER_IN;
#endif

  // Load this voice's ADPCM codebook (predictor coefficients) into DMEM.
  aLoadADPCM(ptr++, f->dc_bookSize,
             K0_TO_PHYS(f->dc_table->waveInfo.adpcmWave.book->book));
  // A loop is active when this request runs past the loop end and the loop
  // still has iterations remaining.
  looped =
      (outCount + f->dc_sample > f->dc_loop.end) && (f->dc_loop.count != 0);

  // nSam = samples to decode before the next loop boundary (the whole request
  // when not looping).
  if (looped)
    nSam = f->dc_loop.end - f->dc_sample;
  else
    nSam = outCount;

  // nLeft = samples already buffered in the partial frame carried from the
  // previous call (0 when the last call ended on a frame boundary).
  if (f->dc_lastsam)
    nLeft = ADPCMFSIZE - f->dc_lastsam;
  else
    nLeft = 0;

  // tsam = new samples to decode this pass; round up to whole ADPCM frames to
  // size the compressed read (nframes frames, nbytes bytes).
  tsam = nSam - nLeft;
  if (tsam < 0) tsam = 0;
  nframes = (tsam + ADPCMFSIZE - 1) >> LFSAMPLES;
  nbytes = nframes * ADPCMFBYTES;
  // Looping case: decode the run up to the loop end, then restart at the loop
  // start and keep filling until the whole request has been produced.
  if (looped) {
    ptr = _decodeChunk(ptr, f, tsam, nbytes, *outp, inp, f->dc_first);

    // Advance *outp past the leading partial frame so the following filter's
    // input pointer lands on the first decoded sample.
    if (f->dc_lastsam)
      *outp += (f->dc_lastsam << 1);
    else
      *outp += (ADPCMFSIZE << 1);

    // Rewind decode state to the loop-start point for the upcoming passes.
    f->dc_lastsam = f->dc_loop.start & 0xf;
    f->dc_memin = (s32)f->dc_table->base +
                  ADPCMFBYTES * ((s32)(f->dc_loop.start >> LFSAMPLES) + 1);
    f->dc_sample = f->dc_loop.start;
    bEnd = *outp;
    // Each pass decodes one loop-length chunk and stitches it onto the end of
    // the data already in DMEM.
    while (outCount > nSam) {
      outCount -= nSam;

      // Decode target for this pass: (nframes+1) frames past bEnd, ahead of the
      // data assembled so far (the decoded chunk is DMEM-moved back to bEnd
      // below). <<(LFSAMPLES+1) converts frames to bytes; the +16 before &
      // ~0x1f rounds the base to the nearest 32-byte frame boundary instead of
      // down (the disabled #if 0 line is the stock round-down form).
#if 0
      op = (bEnd + ((nframes+1)<<(LFSAMPLES+1))) & ~0x1f;
#else
      op = (bEnd + ((nframes + 1) << (LFSAMPLES + 1)) + 16) & ~0x1f;
#endif

      // bEnd marks the true end of the data written so far.
      bEnd += (nSam << 1);

      // Consume one loop iteration; a count of -1 means loop forever.
      if ((f->dc_loop.count != -1) && (f->dc_loop.count != 0))
        f->dc_loop.count--;

      // Size the next chunk: the smaller of what remains and one loop length.
      nSam = MIN(outCount, f->dc_loop.end - f->dc_loop.start);
      tsam = nSam - ADPCMFSIZE + f->dc_lastsam;
      if (tsam < 0) tsam = 0;
      nframes = (tsam + ADPCMFSIZE - 1) >> LFSAMPLES;
      nbytes = nframes * ADPCMFBYTES;
      ptr = _decodeChunk(ptr, f, tsam, nbytes, op, inp, f->dc_first | A_LOOP);

      // Merge the freshly decoded chunk onto the end of the previous data.
      aDMEMMove(ptr++, op + (f->dc_lastsam << 1), bEnd, nSam << 1);
    }
    // Record the final partial-frame offset and advance the play cursor and
    // DRAM read pointer for the next call.
    f->dc_lastsam = (outCount + f->dc_lastsam) & 0xf;
    f->dc_sample += outCount;
    f->dc_memin += ADPCMFBYTES * nframes;
#ifdef AUD_PROFILE
    PROFILE_AUD(adpcm_num, adpcm_cnt, adpcm_max, adpcm_min);
#endif
    return ptr;
  }
  // Non-looping case (the common path): decode straight through, clamping the
  // read at the end of the sample and zero-filling any tail past the end.
  nSam = nframes << LFSAMPLES;

  // overFlow = bytes the read would run past the end of the sample data.
  overFlow = f->dc_memin + nbytes - ((s32)f->dc_table->base + f->dc_table->len);
  if (overFlow < 0) overFlow = 0;

  // nOver = the samples that overflow maps to, clamped to this request.
  nOver = (overFlow / ADPCMFBYTES) << LFSAMPLES;
  if (nOver > nSam + nLeft) nOver = nSam + nLeft;

  // Trim the compressed read back to the real end of the data.
  nbytes -= overFlow;
  // Decode only when at least a whole frame of real samples remains; otherwise
  // the request is entirely tail and we just advance the read pointer.
  if ((nOver - (nOver & 0xf)) < outCount) {
    decoded = 1;
    ptr = _decodeChunk(ptr, f, nSam - nOver, nbytes, *outp, inp, f->dc_first);

    // Advance *outp past the leading partial frame, as in the looping case.
    if (f->dc_lastsam)
      *outp += (f->dc_lastsam << 1);
    else
      *outp += (ADPCMFSIZE << 1);

    // Advance decode/play state by the samples produced.
    f->dc_lastsam = (outCount + f->dc_lastsam) & 0xf;
    f->dc_sample += outCount;
    f->dc_memin += ADPCMFBYTES * nframes;
  } else {
    f->dc_lastsam = 0;
    f->dc_memin += ADPCMFBYTES * nframes;
  }
  // Zero-fill the tail that ran past the end of the sample. startZero offsets
  // the fill to just after whatever was decoded.
  if (nOver) {
    f->dc_lastsam = 0;
    if (decoded)
      startZero = (nLeft + nSam - nOver) << 1;
    else
      startZero = 0;
    aClearBuffer(ptr++, startZero + *outp, nOver << 1);
  }
#ifdef AUD_PROFILE
  PROFILE_AUD(adpcm_num, adpcm_cnt, adpcm_max, adpcm_min);
#endif
  return ptr;
}

/*
 * Load-filter parameter handler. AL_FILTER_SET_WAVETABLE binds a wavetable to
 * the voice and seeds its decode and loop state; AL_FILTER_RESET rewinds decode
 * state to the sample start while keeping the bound wavetable. The s32 return
 * value is unused for this filter and is left unset, matching upstream.
 */
s32 n_alLoadParam(N_PVoice* filter, s32 paramID, void* param) {
  N_PVoice* a = filter;
#if 0
  ALFilter *f = (ALFilter *) filter;
#endif
  switch (paramID) {
    // Bind a new wavetable and point the read cursor at its start.
    case (AL_FILTER_SET_WAVETABLE):
      a->dc_table = (ALWaveTable*)param;
      a->dc_memin = (s32)a->dc_table->base;
      a->dc_sample = 0;
      switch (a->dc_table->type) {
        // Compressed ADPCM: round the table to whole frames, size the codebook,
        // and copy any loop region.
        case (AL_ADPCM_WAVE):
          a->dc_table->len =
              ADPCMFBYTES * ((s32)(a->dc_table->len / ADPCMFBYTES));

          // Codebook bytes = 2 (s16) * order * npredictors * vector size.
          a->dc_bookSize = 2 * a->dc_table->waveInfo.adpcmWave.book->order *
                           a->dc_table->waveInfo.adpcmWave.book->npredictors *
                           ADPCMVSIZE;
          if (a->dc_table->waveInfo.adpcmWave.loop) {
            a->dc_loop.start = a->dc_table->waveInfo.adpcmWave.loop->start;
            a->dc_loop.end = a->dc_table->waveInfo.adpcmWave.loop->end;
            a->dc_loop.count = a->dc_table->waveInfo.adpcmWave.loop->count;
            alCopy(a->dc_table->waveInfo.adpcmWave.loop->state, a->dc_lstate,
                   sizeof(ADPCM_STATE));
          } else {
            a->dc_loop.start = a->dc_loop.end = a->dc_loop.count = 0;
          }
          break;
        // Uncompressed 16-bit PCM: copy loop bounds only (no codebook).
        case (AL_RAW16_WAVE):
          if (a->dc_table->waveInfo.rawWave.loop) {
            a->dc_loop.start = a->dc_table->waveInfo.rawWave.loop->start;
            a->dc_loop.end = a->dc_table->waveInfo.rawWave.loop->end;
            a->dc_loop.count = a->dc_table->waveInfo.rawWave.loop->count;
          } else {
            a->dc_loop.start = a->dc_loop.end = a->dc_loop.count = 0;
          }
          break;
        default:
          break;
      }
      break;
    // Rewind decode state to the sample start; refresh the loop count from the
    // bound table (which may still be null, so it is checked first).
    case (AL_FILTER_RESET):
      a->dc_lastsam = 0;
      a->dc_first = 1;
      a->dc_sample = 0;
      if (a->dc_table) {
        a->dc_memin = (s32)a->dc_table->base;
        if (a->dc_table->type == AL_ADPCM_WAVE) {
          if (a->dc_table->waveInfo.adpcmWave.loop)
            a->dc_loop.count = a->dc_table->waveInfo.adpcmWave.loop->count;
        } else if (a->dc_table->type == AL_RAW16_WAVE) {
          if (a->dc_table->waveInfo.rawWave.loop)
            a->dc_loop.count = a->dc_table->waveInfo.rawWave.loop->count;
        }
      }
      break;
    default:
      break;
  }
}

/*
 * Emits the RSP commands that DMA one compressed chunk into DMEM and decode it:
 * DMAs nbytes from the voice's DRAM read cursor (correcting for the DMA's
 * 8-byte alignment), optionally restores the saved loop predictor state, then
 * decodes tsam samples to DMEM offset outp. nbytes and tsam may be zero (the
 * loop/tail bookkeeping in the caller relies on that). Clears dc_first after
 * the first decode so subsequent decodes reuse predictor state.
 */
Acmd* _decodeChunk(Acmd* ptr, N_PVoice* f, s32 tsam, s32 nbytes, s16 outp,
                   s16 inp, u32 flags) {
  s32 dramAlign, dramLoc;

  // DMA the compressed data in; skip entirely when there is nothing to load.
  if (nbytes > 0) {
    dramLoc = (f->dc_dma)(f->dc_memin, nbytes, f->dc_dmaState);

    // The DMA can return any byte address; pad to the 8-byte boundary the RSP
    // load needs and round the byte count up to a multiple of 8.
    dramAlign = dramLoc & 0x7;
    nbytes += dramAlign;
#ifndef N_MICRO
    aSetBuffer(ptr++, 0, inp, 0, nbytes + 8 - (nbytes & 0x7));
    aLoadBuffer(ptr++, dramLoc - dramAlign);
#else
#include "inc/n_load_add01.inc.c"
#endif
  } else
    dramAlign = 0;

  // At a loop restart, reload the saved ADPCM predictor state.
  if (flags & A_LOOP) {
    aSetLoop(ptr++, K0_TO_PHYS(f->dc_lstate));
  }

  // Decode tsam samples from the loaded input into the output buffer.
#ifndef N_MICRO
  aSetBuffer(ptr++, 0, inp + dramAlign, outp, tsam << 1);
  aADPCMdec(ptr++, flags, K0_TO_PHYS(f->dc_state));
#else
#include "inc/n_load_add02.inc.c"
#endif
  f->dc_first = 0;
  return ptr;
}
