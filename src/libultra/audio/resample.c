/*
 * resample.c
 *
 * Resampler filter: shifts a voice's pitch by sample-rate-converting its
 * source. The pull handler decides how many source samples are needed for the
 * requested output, pulls them, and emits an aResample command; the param
 * handler carries pitch/start/reset state and forwards unknown ops upstream.
 */
#include <libaudio.h>
#include "synthInternals.h"
#include <os.h>
#include <stdio.h>
#ident "$Revision: 1.49 $"

#ifdef AUD_PROFILE
extern u32 cnt_index, resampler_num, resampler_cnt, resampler_max,
    resampler_min, lastCnt[];
#endif

/*
 * Command-list handler. At unity pitch the source is pulled straight through
 * and copied in DMEM (no rate conversion). Otherwise the fractional-sample
 * accumulator (delta) plus the pitch ratio give the integer source-sample
 * count to pull, and aResample interpolates outCnt output samples from them.
 */
Acmd* alResamplePull(void* filter, s16* outp, s32 outCnt, s32 sampleOffset,
                     Acmd* p) {
  ALResampler* f = (ALResampler*)filter;
  Acmd* ptr = p;
  s16 inp;
  s32 inCount;
  ALFilter* source = f->filter.source;
  s32 incr;
  f32 finCount;
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif
  inp = AL_DECODER_OUT;
  if (!outCnt) return ptr;

  if (f->upitch) {
    /* Unity pitch: no conversion, just relocate the source samples in DMEM. */
    ptr = (*source->handler)(source, &inp, outCnt, sampleOffset, p);
    aDMEMMove(ptr++, inp, *outp, outCnt << 1);
  } else {
    /* Clamp the ratio, then quantize it to the RSP's fixed-point pitch grid. */
    if (f->ratio > MAX_RATIO) f->ratio = MAX_RATIO;
    f->ratio = (s32)(f->ratio * UNITY_PITCH);
    f->ratio = f->ratio / UNITY_PITCH;

    /* Carry the fractional remainder so resampling stays phase-continuous. */
    finCount = f->delta + (f->ratio * (f32)outCnt);
    inCount = (s32)finCount;
    f->delta = finCount - (f32)inCount;

    ptr = (*source->handler)(source, &inp, inCount, sampleOffset, p);
    incr = (s32)(f->ratio * UNITY_PITCH);
    aSetBuffer(ptr++, 0, inp, *outp, outCnt << 1);
    aResample(ptr++, f->first, incr, osVirtualToPhysical(f->state));
    f->first = 0;
  }
#ifdef AUD_PROFILE
  PROFILE_AUD(resampler_num, resampler_cnt, resampler_max, resampler_min);
#endif
  return ptr;
}

/*
 * Parameter setter. RESET/START reinitialize the interpolation state and
 * cascade to the source; SET_PITCH stores the ratio (reinterpreting the void*
 * bits as a float); SET_UNITY_PITCH selects the copy fast path. Anything else
 * is forwarded to the source filter.
 */
s32 alResampleParam(void* filter, s32 paramID, void* param) {
  ALFilter* f = (ALFilter*)filter;
  ALResampler* r = (ALResampler*)filter;
  union {
    f32 f;
    s32 i;
  } data;

  switch (paramID) {
    case (AL_FILTER_SET_SOURCE):
      f->source = (ALFilter*)param;
      break;
    case (AL_FILTER_RESET):
      r->delta = 0.0;
      r->first = 1;
      r->motion = AL_STOPPED;
      r->upitch = 0;
      if (f->source) (*f->source->setParam)(f->source, AL_FILTER_RESET, 0);
      break;
    case (AL_FILTER_START):
      r->motion = AL_PLAYING;
      if (f->source) (*f->source->setParam)(f->source, AL_FILTER_START, 0);
      break;
    case (AL_FILTER_SET_PITCH):
      data.i = (s32)param;
      r->ratio = data.f;
      break;
    case (AL_FILTER_SET_UNITY_PITCH):
      r->upitch = 1;
      break;
    default:
      if (f->source) (*f->source->setParam)(f->source, paramID, param);
      break;
  }
  return 0;
}
