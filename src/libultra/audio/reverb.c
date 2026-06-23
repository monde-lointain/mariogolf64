/*
 * reverb.c
 *
 * Effects (reverb) engine: the pull handler for the ALFx filter. Reverb is
 * built from a circular delay line read at several taps; each tap mixes in
 * feedforward and feedback paths with optional modulation (chorus/flange) and
 * low-pass filtering. The helpers below move data between the DMEM scratch
 * buffers and the DRAM delay ring, wrapping around its end.
 */
#include <libaudio.h>
#include <ultraerror.h>
#include "synthInternals.h"
#include <os.h>
#include <os_internal.h>
#include <stdio.h>
#include <assert.h>
#include "initfx.h"
#ident "$Revision: 1.49 $"
#ident "$Revision: 1.17 $"

#define RANGE 2.0

extern ALGlobals* alGlobals;

#ifdef AUD_PROFILE
extern u32 cnt_index, reverb_num, reverb_cnt, reverb_max, reverb_min, lastCnt[];
extern u32 load_num, load_cnt, load_max, load_min, save_num, save_cnt, save_max,
    save_min;
#endif

/* Exchange two DMEM buffer ids so a tap's output can become the next input
 * without recopying. */
#define SWAP(in, out) \
  {                   \
    s16 t = out;      \
    out = in;         \
    in = t;           \
  }

Acmd* _loadOutputBuffer(ALFx* r, ALDelay* d, s32 buff, s32 incount, Acmd* p);
Acmd* _loadBuffer(ALFx* r, s16* curr_ptr, s32 buff, s32 count, Acmd* p);
Acmd* _saveBuffer(ALFx* r, s16* curr_ptr, s32 buff, s32 count, Acmd* p);
Acmd* _filterBuffer(ALLowPass* lp, s32 buff, s32 count, Acmd* p);
f32 _doModFunc(ALDelay* d, s32 count);

static s32 L_INC[] = {L0_INC, L1_INC, L2_INC};

/*
 * Effects pull handler. Pulls the dry signal from the source, folds the stereo
 * AUX inputs down to a single reverb input, runs it through each delay-line
 * section (load tap, apply feedforward/feedback, optional low-pass, sum into
 * the output), then advances the circular delay ring and writes the wet result
 * back to the AUX bus.
 */
Acmd* alFxPull(void* filter, s16* outp, s32 outCount, s32 sampleOffset,
               Acmd* p) {
  Acmd* ptr = p;
  ALFx* r = (ALFx*)filter;
  ALFilter* source = r->filter.source;
  s16 i, buff1, buff2, input, output;
  s16 *in_ptr, *out_ptr, gain, *prev_out_ptr = 0;
  ALDelay *d, *pd;
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif
#if BUILD_VERSION < VERSION_J
#line 74
#endif
#ifdef _DEBUG
  assert(source);
#endif
  ptr = (*source->handler)(source, outp, outCount, sampleOffset, p);
  input = AL_AUX_L_OUT;
  output = AL_AUX_R_OUT;
  buff1 = AL_TEMP_0;
  buff2 = AL_TEMP_1;

  /* Collapse the stereo aux input into the mono reverb input buffer. */
  aSetBuffer(ptr++, 0, 0, 0, outCount << 1);
  aMix(ptr++, 0, 0xda83, AL_AUX_L_OUT, input);
  aMix(ptr++, 0, 0x5a82, AL_AUX_R_OUT, input);
  ptr = _saveBuffer(r, r->input, input, outCount, ptr);
  aClearBuffer(ptr++, output, outCount << 1);

  /* Process each delay tap, accumulating its contribution into output. */
  for (i = 0; i < r->section_count; i++) {
    d = &r->delay[i];
    in_ptr = &r->input[-d->input];
    out_ptr = &r->input[-d->output];

    /* Reuse the previous tap's output buffer when the taps line up. */
    if (in_ptr == prev_out_ptr) {
      SWAP(buff1, buff2);
    } else {
      ptr = _loadBuffer(r, in_ptr, buff1, outCount, ptr);
    }
    ptr = _loadOutputBuffer(r, d, buff2, outCount, ptr);

    /* Feedforward: blend the input tap forward into the output tap. */
    if (d->ffcoef) {
      aMix(ptr++, 0, (u16)d->ffcoef, buff1, buff2);
      if (!d->rs && !d->lp) {
        ptr = _saveBuffer(r, out_ptr, buff2, outCount, ptr);
      }
    }

    /* Feedback: fold the output tap back into the input tap. */
    if (d->fbcoef) {
      aMix(ptr++, 0, (u16)d->fbcoef, buff2, buff1);
      ptr = _saveBuffer(r, in_ptr, buff1, outCount, ptr);
    }
    if (d->lp) ptr = _filterBuffer(d->lp, buff2, outCount, ptr);
    if (!d->rs) ptr = _saveBuffer(r, out_ptr, buff2, outCount, ptr);
    if (d->gain) aMix(ptr++, 0, (u16)d->gain, buff2, output);
    prev_out_ptr = &r->input[d->output];
  }

  /* Advance the write head, wrapping it within the delay ring. */
  r->input += outCount;
  if (r->input > &r->base[r->length]) r->input -= r->length;
  aDMEMMove(ptr++, output, AL_AUX_L_OUT, outCount << 1);
#ifdef AUD_PROFILE
  PROFILE_AUD(reverb_num, reverb_cnt, reverb_max, reverb_min);
#endif
  return ptr;
}

/* Effects param setter: the base filter only accepts a source connection;
 * per-tap tuning goes through alFxParamHdl. */
s32 alFxParam(void* filter, s32 paramID, void* param) {
  if (paramID == AL_FILTER_SET_SOURCE) {
    ALFilter* f = (ALFilter*)filter;
    f->source = (ALFilter*)param;
  }
  return 0;
}

/*
 * Per-section effects-parameter handler. The param id encodes which delay
 * section (id-2)/8 and which field (id-2)%8 to set, so one call tunes one
 * reverb knob (tap positions, coefs, gain, chorus rate/depth, or low-pass
 * cutoff) on one section at runtime.
 */
s32 alFxParamHdl(void* filter, s32 paramID, void* param) {
  ALFx* f = (ALFx*)filter;
  s32 p = (paramID - 2) % 8;
  s32 s = (paramID - 2) / 8;
  s32 val = *(s32*)param;
#define INPUT_PARAM 0
#define OUTPUT_PARAM 1
#define FBCOEF_PARAM 2
#define FFCOEF_PARAM 3
#define GAIN_PARAM 4
#define CHORUSRATE_PARAM 5
#define CHORUSDEPTH_PARAM 6
#define LPFILT_PARAM 7
  switch (p) {
    case INPUT_PARAM:
      f->delay[s].input = (u32)val & 0xFFFFFFF8;
      break;
    case OUTPUT_PARAM:
      f->delay[s].output = (u32)val & 0xFFFFFFF8;
      break;
    case FFCOEF_PARAM:
      f->delay[s].ffcoef = (s16)val;
      break;
    case FBCOEF_PARAM:
      f->delay[s].fbcoef = (s16)val;
      break;
    case GAIN_PARAM:
      f->delay[s].gain = (s16)val;
      break;
    case CHORUSRATE_PARAM:
      f->delay[s].rsinc =
          ((((f32)val) / 1000) * RANGE) / alGlobals->drvr.outputRate;
      break;
#define CONVERT 173123.404906676
#define LENGTH (f->delay[s].output - f->delay[s].input)
    case CHORUSDEPTH_PARAM:
      f->delay[s].rsgain = (((f32)val) / CONVERT) * LENGTH;
      break;
    case LPFILT_PARAM:
      if (f->delay[s].lp) {
        f->delay[s].lp->fc = (s16)val;
        _init_lpfilter(f->delay[s].lp);
      }
      break;
  }
  return 0;
}

/*
 * Load a delay tap's output samples into DMEM. For a modulated tap (chorus/
 * flange) the modulation function sweeps the read position and the data is
 * resampled to the fixed output length; otherwise the tap is loaded straight.
 */
Acmd* _loadOutputBuffer(ALFx* r, ALDelay* d, s32 buff, s32 incount, Acmd* p) {
  Acmd* ptr = p;
  s32 ratio, count, rbuff = AL_TEMP_2;
  s16* out_ptr;
  f32 fincount, fratio, delta;
  s32 ramalign = 0, length;
  static f32 val = 0.0, lastval = -10.0;
  static f32 blob = 0;
  if (d->rs) {
    /* Modulated tap: derive the resample ratio from the LFO, pull the swept
     * window, and carry the fractional remainder for phase continuity. */
    length = d->output - d->input;
    delta = _doModFunc(d, incount);
    delta /= length;
    delta = (s32)(delta * UNITY_PITCH);
    delta = delta / UNITY_PITCH;
    fratio = 1.0 - delta;
    fincount = d->rs->delta + (fratio * (f32)incount);
    count = (s32)fincount;
    d->rs->delta = fincount - (f32)count;
    out_ptr = &r->input[-(d->output - d->rsdelta)];
    ramalign = ((s32)out_ptr & 0x7) >> 1;
#ifdef _DEBUG
#if 0
        if(length > 0) {
            if (length - d->rsdelta > (s32)r->length) {
                __osError(ERR_ALMODDELAYOVERFLOW, 1, length - d->rsdelta - r->length);
            }
        }
        else if(length < 0) {
            if ((-length) - d->rsdelta > (s32)r->length) {
                __osError(ERR_ALMODDELAYOVERFLOW, 1, (-length) - d->rsdelta - r->length);
            }
        }
#endif
#endif
    ptr = _loadBuffer(r, out_ptr - ramalign, rbuff, count + ramalign, ptr);
    ratio = (s32)(fratio * UNITY_PITCH);
    aSetBuffer(ptr++, 0, rbuff + (ramalign << 1), buff, incount << 1);
    aResample(ptr++, d->rs->first, ratio, osVirtualToPhysical(d->rs->state));
    d->rs->first = 0;
    d->rsdelta += count - incount;
  } else {
    out_ptr = &r->input[-d->output];
    ptr = _loadBuffer(r, out_ptr, buff, incount, ptr);
  }
  return ptr;
}

/*
 * Load count samples from the DRAM delay ring at curr_ptr into DMEM buffer
 * buff. Handles the read wrapping past the end of the ring by splitting it
 * into a pre-wrap and post-wrap DMA.
 */
Acmd* _loadBuffer(ALFx* r, s16* curr_ptr, s32 buff, s32 count, Acmd* p) {
  Acmd* ptr = p;
  s32 after_end, before_end;
  s16 *updated_ptr, *delay_end;
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif
  delay_end = &r->base[r->length];
#ifdef _DEBUG
  if (curr_ptr > delay_end)
    __osError(ERR_ALMODDELAYOVERFLOW, 1, delay_end - curr_ptr);
#endif
  if (curr_ptr < r->base) curr_ptr += r->length;
  updated_ptr = curr_ptr + count;
  if (updated_ptr > delay_end) {
    /* Split the load across the ring wrap. */
    after_end = updated_ptr - delay_end;
    before_end = delay_end - curr_ptr;
    aSetBuffer(ptr++, 0, buff, 0, before_end << 1);
    aLoadBuffer(ptr++, osVirtualToPhysical(curr_ptr));
    aSetBuffer(ptr++, 0, buff + (before_end << 1), 0, after_end << 1);
    aLoadBuffer(ptr++, osVirtualToPhysical(r->base));
  } else {
    aSetBuffer(ptr++, 0, buff, 0, count << 1);
    aLoadBuffer(ptr++, osVirtualToPhysical(curr_ptr));
  }
  aSetBuffer(ptr++, 0, 0, 0, count << 1);
#ifdef AUD_PROFILE
  PROFILE_AUD(load_num, load_cnt, load_max, load_min);
#endif
  return ptr;
}

/*
 * Store count samples from DMEM buffer buff back into the DRAM delay ring at
 * curr_ptr, splitting the write across the ring wrap when it runs past the end.
 */
Acmd* _saveBuffer(ALFx* r, s16* curr_ptr, s32 buff, s32 count, Acmd* p) {
  Acmd* ptr = p;
  s32 after_end, before_end;
  s16 *updated_ptr, *delay_end;
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif
  delay_end = &r->base[r->length];
  if (curr_ptr < r->base) curr_ptr += r->length;
  updated_ptr = curr_ptr + count;
  if (updated_ptr > delay_end) {
    /* Split the save across the ring wrap. */
    after_end = updated_ptr - delay_end;
    before_end = delay_end - curr_ptr;
    aSetBuffer(ptr++, 0, 0, buff, before_end << 1);
    aSaveBuffer(ptr++, osVirtualToPhysical(curr_ptr));
    aSetBuffer(ptr++, 0, 0, buff + (before_end << 1), after_end << 1);
    aSaveBuffer(ptr++, osVirtualToPhysical(r->base));
    aSetBuffer(ptr++, 0, 0, 0, count << 1);
  } else {
    aSetBuffer(ptr++, 0, 0, buff, count << 1);
    aSaveBuffer(ptr++, osVirtualToPhysical(curr_ptr));
  }
#ifdef AUD_PROFILE
  PROFILE_AUD(save_num, save_cnt, save_max, save_min);
#endif
  return ptr;
}

/* Apply a tap's single-pole low-pass filter in place over count samples,
 * loading its precomputed coefficient vector as the RSP codebook. */
Acmd* _filterBuffer(ALLowPass* lp, s32 buff, s32 count, Acmd* p) {
  Acmd* ptr = p;
  aSetBuffer(ptr++, 0, buff, buff, count << 1);
  aLoadADPCM(ptr++, 32, osVirtualToPhysical(lp->fcvec.fccoef));
  aPoleFilter(ptr++, lp->first, lp->fgain, osVirtualToPhysical(lp->fstate));
  lp->first = 0;
  return ptr;
}

/*
 * Chorus/flange modulation LFO. Advances a triangle-wave phase by the tap's
 * rate, folds it into the [-RANGE/2, +RANGE/2] range, and scales it by the
 * depth to produce the per-call delay offset that sweeps the read position.
 */
f32 _doModFunc(ALDelay* d, s32 count) {
  f32 val;
  d->rsval += d->rsinc * count;
  d->rsval = (d->rsval > RANGE) ? d->rsval - (RANGE * 2) : d->rsval;
  val = d->rsval;
  val = (val < 0) ? -val : val;
  val -= RANGE / 2;
  return (d->rsgain * val);
}
