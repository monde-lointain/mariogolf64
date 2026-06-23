/*
 * load.c
 *
 * Wavetable load/decode filters: the head of every voice's pull chain. They
 * DMA compressed (ADPCM) or raw 16-bit PCM sample data in from DRAM and emit
 * the RSP commands that decode it into the DMEM decoder output, honoring loop
 * points and zero-filling past the end of a one-shot sample.
 */
#include <libaudio.h>
#include "synthInternals.h"
#include <os.h>
#include <R4300.h>

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifdef AUD_PROFILE
extern u32 cnt_index, adpcm_num, adpcm_cnt, adpcm_max, adpcm_min, lastCnt[];
#endif

/* ADPCM frames pack 16 samples into 9 bytes; loop math works in 16-sample
 * units (1 << LFSAMPLES). */
#define ADPCMFBYTES 9
#define LFSAMPLES 4

static Acmd* _decodeChunk(Acmd* ptr, ALLoadFilter* f, s32 tsam, s32 nbytes,
                          s16 outp, s16 inp, u32 flags);

/*
 * ADPCM pull handler. Produces outCount decoded samples into *outp. The simple
 * case decodes one chunk and, if the sample is shorter than requested, clears
 * the tail. When the play position would cross the loop end, it decodes up to
 * the loop point, rewinds to the loop start, and keeps decoding chunk-by-chunk
 * until outCount is satisfied or the loop count runs out.
 */
Acmd* alAdpcmPull(void* filter, s16* outp, s32 outCount, s32 sampleOffset,
                  Acmd* p) {
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
  ALLoadFilter* f = (ALLoadFilter*)filter;
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif
  if (outCount == 0) {
    return ptr;
  }
  inp = AL_DECODER_IN;

  /* Load this wave's ADPCM codebook so the RSP can decode its frames. */
  aLoadADPCM(ptr++, f->bookSize,
             K0_TO_PHYS(f->table->waveInfo.adpcmWave.book->book));

  /* Looping is in play only when this frame would reach the loop end. */
  looped = (outCount + f->sample > f->loop.end) && (f->loop.count != 0);
  if (looped) {
    nSam = f->loop.end - f->sample;
  } else {
    nSam = outCount;
  }

  /* Samples left over in the partially-decoded frame from the last call. */
  if (f->lastsam) {
    nLeft = ADPCMFSIZE - f->lastsam;
  } else {
    nLeft = 0;
  }

  /* Whole new samples to decode, rounded up to frames -> bytes. */
  tsam = nSam - nLeft;
  if (tsam < 0) {
    tsam = 0;
  }
  nframes = (tsam + ADPCMFSIZE - 1) >> LFSAMPLES;
  nbytes = nframes * ADPCMFBYTES;

  if (looped) {
    /* Decode up to the loop end, then rewind state to the loop start. */
    ptr = _decodeChunk(ptr, f, tsam, nbytes, *outp, inp, f->first);
    if (f->lastsam) {
      *outp += (f->lastsam << 1);
    } else {
      *outp += (ADPCMFSIZE << 1);
    }
    f->lastsam = f->loop.start & 0xf;
    f->memin = (s32)f->table->base +
               (ADPCMFBYTES * ((s32)(f->loop.start >> LFSAMPLES) + 1));
    f->sample = f->loop.start;
    bEnd = *outp;

    /* Keep replaying the loop body until the whole request is filled. */
    while (outCount > nSam) {
      outCount -= nSam;
      op = (bEnd + ((nframes + 1) << (LFSAMPLES + 1))) & ~0x1f;
      bEnd += (nSam << 1);
      if ((f->loop.count != -1) && (f->loop.count != 0)) {
        f->loop.count--;
      }
      nSam = MIN(outCount, f->loop.end - f->loop.start);
      tsam = nSam - ADPCMFSIZE + f->lastsam;
      if (tsam < 0) {
        tsam = 0;
      }
      nframes = (tsam + ADPCMFSIZE - 1) >> LFSAMPLES;
      nbytes = nframes * ADPCMFBYTES;
      ptr = _decodeChunk(ptr, f, tsam, nbytes, op, inp, f->first | A_LOOP);
      aDMEMMove(ptr++, op + (f->lastsam << 1), bEnd, nSam << 1);
    }
    f->lastsam = (outCount + f->lastsam) & 0xf;
    f->sample += outCount;
    f->memin += ADPCMFBYTES * nframes;
#ifdef AUD_PROFILE
    PROFILE_AUD(adpcm_num, adpcm_cnt, adpcm_max, adpcm_min);
#endif
    return ptr;
  }

  /* Non-looping path: clamp the byte count to what remains in the sample, and
   * track how many samples fall past the end so they can be zeroed. */
  nSam = nframes << LFSAMPLES;
  overFlow = f->memin + nbytes - ((s32)f->table->base + f->table->len);
  if (overFlow < 0) {
    overFlow = 0;
  }
  nOver = (overFlow / ADPCMFBYTES) << LFSAMPLES;
  if (nOver > nSam + nLeft) {
    nOver = nSam + nLeft;
  }
  nbytes -= overFlow;

  /* Decode unless the request is entirely past the end of the sample. */
  if ((nOver - (nOver & 0xf)) < outCount) {
    decoded = 1;
    ptr = _decodeChunk(ptr, f, nSam - nOver, nbytes, *outp, inp, f->first);
    if (f->lastsam) {
      *outp += (f->lastsam << 1);
    } else {
      *outp += (ADPCMFSIZE << 1);
    }
    f->lastsam = (outCount + f->lastsam) & 0xf;
    f->sample += outCount;
    f->memin += ADPCMFBYTES * nframes;
  } else {
    f->lastsam = 0;
    f->memin += ADPCMFBYTES * nframes;
  }

  /* Silence the samples that ran past the end of the sample data. */
  if (nOver) {
    f->lastsam = 0;
    if (decoded) {
      startZero = (nLeft + nSam - nOver) << 1;
    } else {
      startZero = 0;
    }
    aClearBuffer(ptr++, startZero + *outp, nOver << 1);
  }
#ifdef AUD_PROFILE
  PROFILE_AUD(adpcm_num, adpcm_cnt, adpcm_max, adpcm_min);
#endif
  return ptr;
}

/*
 * Raw 16-bit PCM pull handler. Like the ADPCM path but with no decode step:
 * sample data is DMA'd straight into DMEM (8-byte aligned, with realignment
 * moves when the DRAM/DMEM addresses are unaligned), honoring loop points and
 * zero-filling any tail past the end of a one-shot sample.
 */
Acmd* alRaw16Pull(void* filter, s16* outp, s32 outCount, s32 sampleOffset,
                  Acmd* p) {
  Acmd* ptr = p;
  s32 nbytes;
  s32 dramLoc;
  s32 dramAlign;
  s32 dmemAlign;
  s32 overFlow;
  s32 startZero;
  s32 nSam;
  s32 op;
  ALLoadFilter* f = (ALLoadFilter*)filter;
  ALFilter* a = (ALFilter*)filter;
  if (outCount == 0) {
    return ptr;
  }

  if ((outCount + f->sample > f->loop.end) && (f->loop.count != 0)) {
    /* Load up to the loop end, compensating for the DRAM 8-byte alignment. */
    nSam = f->loop.end - f->sample;
    nbytes = nSam << 1;
    if (nSam > 0) {
      dramLoc = (f->dma)(f->memin, nbytes, f->dmaState);
      dramAlign = dramLoc & 0x7;
      nbytes += dramAlign;
      aSetBuffer(ptr++, 0, *outp, 0, nbytes + 8 - (nbytes & 0x7));
      aLoadBuffer(ptr++, dramLoc - dramAlign);
    } else {
      dramAlign = 0;
    }
    *outp += dramAlign;
    f->memin = (s32)f->table->base + (f->loop.start << 1);
    f->sample = f->loop.start;
    op = *outp;

    /* Replay the loop body, realigning DMEM each pass, until the request is
     * filled. */
    while (outCount > nSam) {
      op += (nSam << 1);
      outCount -= nSam;
      if ((f->loop.count != -1) && (f->loop.count != 0)) {
        f->loop.count--;
      }
      nSam = MIN(outCount, f->loop.end - f->loop.start);
      nbytes = nSam << 1;
      dramLoc = (f->dma)(f->memin, nbytes, f->dmaState);
      dramAlign = dramLoc & 0x7;
      nbytes += dramAlign;
      if (op & 0x7) {
        dmemAlign = 8 - (op & 0x7);
      } else {
        dmemAlign = 0;
      }
      aSetBuffer(ptr++, 0, op + dmemAlign, 0, nbytes + 8 - (nbytes & 0x7));
      aLoadBuffer(ptr++, dramLoc - dramAlign);
      if (dramAlign || dmemAlign)
        aDMEMMove(ptr++, op + dramAlign + dmemAlign, op, nSam << 1);
    }
    f->sample += outCount;
    f->memin += (outCount << 1);
    return ptr;
  }

  /* Non-looping path: load what fits before the end and zero any overflow. */
  nbytes = outCount << 1;
  overFlow = f->memin + nbytes - ((s32)f->table->base + f->table->len);
  if (overFlow < 0) {
    overFlow = 0;
  }
  if (overFlow > nbytes) {
    overFlow = nbytes;
  }
  if (overFlow < nbytes) {
    if (outCount > 0) {
      nbytes -= overFlow;
      dramLoc = (f->dma)(f->memin, nbytes, f->dmaState);
      dramAlign = dramLoc & 0x7;
      nbytes += dramAlign;
      aSetBuffer(ptr++, 0, *outp, 0, nbytes + 8 - (nbytes & 0x7));
      aLoadBuffer(ptr++, dramLoc - dramAlign);
    } else {
      dramAlign = 0;
    }
    *outp += dramAlign;
    f->sample += outCount;
    f->memin += outCount << 1;
  } else {
    f->memin += outCount << 1;
  }

  /* Silence the samples that ran past the end of the sample data. */
  if (overFlow) {
    startZero = (outCount << 1) - overFlow;
    if (startZero < 0) {
      startZero = 0;
    }
    aClearBuffer(ptr++, startZero + *outp, overFlow);
  }
  return ptr;
}

/*
 * Parameter setter for the load filters. SET_WAVETABLE binds a new wave,
 * selecting the ADPCM or raw16 handler, snapping the ADPCM length to whole
 * frames, computing the codebook size, and copying any loop info. RESET
 * rewinds the read position and restores the loop count. Other ops are no-ops.
 */
s32 alLoadParam(void* filter, s32 paramID, void* param) {
  ALLoadFilter* a = (ALLoadFilter*)filter;
  ALFilter* f = (ALFilter*)filter;
  switch (paramID) {
    case (AL_FILTER_SET_WAVETABLE):
      a->table = (ALWaveTable*)param;
      a->memin = (s32)a->table->base;
      a->sample = 0;
      switch (a->table->type) {
        case (AL_ADPCM_WAVE):
          f->handler = alAdpcmPull;
          a->table->len = ADPCMFBYTES * ((s32)(a->table->len / ADPCMFBYTES));
          a->bookSize = 2 * a->table->waveInfo.adpcmWave.book->order *
                        a->table->waveInfo.adpcmWave.book->npredictors *
                        ADPCMVSIZE;
          if (a->table->waveInfo.adpcmWave.loop) {
            a->loop.start = a->table->waveInfo.adpcmWave.loop->start;
            a->loop.end = a->table->waveInfo.adpcmWave.loop->end;
            a->loop.count = a->table->waveInfo.adpcmWave.loop->count;
            alCopy(a->table->waveInfo.adpcmWave.loop->state, a->lstate,
                   sizeof(ADPCM_STATE));
          } else {
            a->loop.start = a->loop.end = a->loop.count = 0;
          }
          break;
        case (AL_RAW16_WAVE):
          f->handler = alRaw16Pull;
          if (a->table->waveInfo.rawWave.loop) {
            a->loop.start = a->table->waveInfo.rawWave.loop->start;
            a->loop.end = a->table->waveInfo.rawWave.loop->end;
            a->loop.count = a->table->waveInfo.rawWave.loop->count;
          } else {
            a->loop.start = a->loop.end = a->loop.count = 0;
          }
          break;
        default:
          break;
      }
      break;
    case (AL_FILTER_RESET):
      a->lastsam = 0;
      a->first = 1;
      a->sample = 0;
      if (a->table) {
        a->memin = (s32)a->table->base;
        if (a->table->type == AL_ADPCM_WAVE) {
          if (a->table->waveInfo.adpcmWave.loop) {
            a->loop.count = a->table->waveInfo.adpcmWave.loop->count;
          }
        } else if (a->table->type == AL_RAW16_WAVE) {
          if (a->table->waveInfo.rawWave.loop) {
            a->loop.count = a->table->waveInfo.rawWave.loop->count;
          }
        }
      }
      break;
    default:
      break;
  }
}

/*
 * Emit the commands for one ADPCM chunk: DMA the compressed bytes into DMEM
 * (8-byte aligned), set the loop state if this chunk crosses a loop boundary,
 * and decode tsam samples to outp. Clears f->first so subsequent chunks
 * decode continuously rather than as the first frame of a sample.
 */
Acmd* _decodeChunk(Acmd* ptr, ALLoadFilter* f, s32 tsam, s32 nbytes, s16 outp,
                   s16 inp, u32 flags) {
  s32 dramAlign;
  s32 dramLoc;
  if (nbytes > 0) {
    dramLoc = (f->dma)(f->memin, nbytes, f->dmaState);
    dramAlign = dramLoc & 0x7;
    nbytes += dramAlign;
    aSetBuffer(ptr++, 0, inp, 0, nbytes + 8 - (nbytes & 0x7));
    aLoadBuffer(ptr++, dramLoc - dramAlign);
  } else {
    dramAlign = 0;
  }
  if (flags & A_LOOP) {
    aSetLoop(ptr++, K0_TO_PHYS(f->lstate));
  }
  aSetBuffer(ptr++, 0, inp + dramAlign, outp, tsam << 1);
  aADPCMdec(ptr++, flags, K0_TO_PHYS(f->state));
  f->first = 0;
  return ptr;
}
