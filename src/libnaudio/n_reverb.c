/*
 * n_reverb.c
 *
 * RSP audio command-list builder for the low-level ("n_") reverb / chorus
 * effect (ALFx). An ALFx is a configurable multi-section delay-line
 * reverberator: each section taps a DRAM ring buffer, applies feedforward and
 * feedback coefficients, an optional chorus resampler, and an optional damping
 * low-pass, then mixes its wet output into the effect accumulator. The routines
 * here only build the Acmd command list; the sample math itself runs on the
 * RSP. The N_MICRO build selects the compact n_a* commands (the ".inc"
 * fragments) in place of the full a* command sequences.
 */
#include "n_synthInternals.h"
#include <ultraerror.h>
#include <os.h>
#include <os_internal.h>

/* Chorus LFO full-scale range multiplier (see CHORUSRATE handling). */
#define RANGE 2.0

#ifdef AUD_PROFILE
extern u32 cnt_index, reverb_num, reverb_cnt, reverb_max, reverb_min, lastCnt[];
extern u32 load_num, load_cnt, load_max, load_min, save_num, save_cnt, save_max,
    save_min;
#endif

/* Swap two DMEM scratch-buffer ids (lets a section reuse a block in place). */
#define SWAP(in, out) \
  {                   \
    s16 t = out;      \
    (out) = in;       \
    (in) = t;         \
  }

Acmd* _n_loadOutputBuffer(ALFx* r, ALDelay* d, s32 buff, Acmd* p);
Acmd* _n_loadBuffer(ALFx* r, s16* curr_ptr, s32 buff, s32 count, Acmd* p);
Acmd* _n_saveBuffer(ALFx* r, s16* curr_ptr, s32 buff, Acmd* p);
Acmd* _n_filterBuffer(ALLowPass* lp, s32 buff, Acmd* p);
extern f32 _doModFunc(ALDelay* d, s32 count);
extern s32 L_INC[];

/*
 * Effects pull handler. Pulls the upstream aux-bus mix, folds the stereo aux
 * feed down to a single mono input, walks every delay section to build the wet
 * signal, advances the delay-line write pointer (wrapping at the ring end),
 * then copies the finished output back to the aux bus. Returns the advanced
 * command pointer.
 */
Acmd* n_alFxPull(s32 sampleOffset, Acmd* p) {
  Acmd* ptr = p;
  ALFx* r = n_syn->auxBus->fx;  // state of the single aux-bus FX slot
  s16 i;
  s16 buff1;
  s16 buff2;
  s16 input;
  s16 output;  // loop index + DMEM scratch-buffer ids
  s16* in_ptr;
  s16* out_ptr;
  s16 gain;
  s16* prev_out_ptr = 0;
  ALDelay *d, *pd;
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif

  /* Pull the dry aux-bus mix that feeds this effect. */
  ptr = n_alAuxBusPull(sampleOffset, p);

  /* Select DMEM buffer addresses for the full vs. micro command set. */
#ifndef N_MICRO
  input = AL_AUX_L_OUT;
  output = AL_AUX_R_OUT;
  buff1 = AL_TEMP_0;
  buff2 = AL_TEMP_1;
#else
  input = N_AL_AUX_L_OUT;
  output = N_AL_AUX_R_OUT;
  buff1 = N_AL_TEMP_0;
  buff2 = N_AL_TEMP_1;
#endif

  /* Collapse the stereo aux-bus output into the mono reverb input. */
#ifndef N_MICRO
  aSetBuffer(ptr++, 0, 0, 0, FIXED_SAMPLE << 1);
  aMix(ptr++, 0, 0xda83, AL_AUX_L_OUT, input);
  aMix(ptr++, 0, 0x5a82, AL_AUX_R_OUT, input);
#else
  aMix(ptr++, 0, 0xda83, N_AL_AUX_L_OUT, input);
  aMix(ptr++, 0, 0x5a82, N_AL_AUX_R_OUT, input);
#endif

  /* Store the new input block into the delay line and clear the wet output. */
  ptr = _n_saveBuffer(r, r->input, input, ptr);
  aClearBuffer(ptr++, output, FIXED_SAMPLE << 1);

  /* Accumulate each delay section's contribution into the output buffer. */
  for (i = 0; i < r->section_count; i++) {
    d = &r->delay[i];
    in_ptr =
        &r->input[-d->input];  // input tap (samples back from write pointer)
    out_ptr = &r->input[-d->output];  // delayed output tap

    /* Reuse the previous section's output buffer when this input tap matches
     * it. */
    if (in_ptr == prev_out_ptr) {
      SWAP(buff1, buff2);
    } else {
      ptr = _n_loadBuffer(r, in_ptr, buff1, FIXED_SAMPLE, ptr);
    }
    ptr = _n_loadOutputBuffer(r, d, buff2, ptr);

    /* Feedforward: blend the input tap forward into the output tap. */
    if (d->ffcoef) {
      aMix(ptr++, 0, (u16)d->ffcoef, buff1, buff2);
      if (!d->rs && !d->lp) {
        ptr = _n_saveBuffer(r, out_ptr, buff2, ptr);
      }
    }

    /* Feedback: fold the output tap back into the input tap. */
    if (d->fbcoef) {
      aMix(ptr++, 0, (u16)d->fbcoef, buff2, buff1);
      ptr = _n_saveBuffer(r, in_ptr, buff1, ptr);
    }

    if (d->lp) {
      ptr = _n_filterBuffer(d->lp, buff2, ptr);  // optional damping
    }
    if (!d->rs) {
      ptr = _n_saveBuffer(r, out_ptr, buff2, ptr);
    }
    if (d->gain)
      aMix(ptr++, 0, (u16)d->gain, buff2, output);  // wet output -> accumulator
    prev_out_ptr = &r->input[d->output];
  }

  /* Advance the delay-line write pointer, wrapping at the ring buffer end. */
  r->input += FIXED_SAMPLE;
  if (r->input > &r->base[r->length]) {
    r->input -= r->length;
  }

  /* Move the finished reverb output up to the aux bus. */
#ifndef N_MICRO
  aDMEMMove(ptr++, output, AL_AUX_L_OUT, FIXED_SAMPLE << 1);
#else
  aDMEMMove(ptr++, output, N_AL_AUX_L_OUT, FIXED_SAMPLE << 1);
#endif
#ifdef AUD_PROFILE
  PROFILE_AUD(reverb_num, reverb_cnt, reverb_max, reverb_min);
#endif
  return ptr;
}

/*
 * Per-section effects-parameter handler. Applies one runtime parameter change
 * to a reverb section: paramID encodes the section index and which field to set
 * (8 parameters per section, starting at id 2), and param points at the new s32
 * value. Coefficients are stored directly, tap offsets are forced to DMA
 * alignment, and chorus rate/depth are converted to the resampler's per-sample
 * increment and modulation gain. Returns 0.
 */
s32 n_alFxParamHdl(void* filter, s32 paramID, void* param) {
  ALFx* f = (ALFx*)filter;
  s32 p = (paramID - 2) % 8;  // field selector within the section
  s32 s = (paramID - 2) / 8;  // section index
  s32 val = *(s32*)param;

  /* Field selectors: offset within each section's 8-parameter block. */
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
      // round the tap offset down to a multiple of 8 for DMA alignment
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
      // chorus rate -> modulation-LFO increment per output sample
      f->delay[s].rsinc = ((((f32)val) / 1000) * RANGE) / n_syn->outputRate;
      break;
#define CONVERT 173123.404906676
#define LENGTH (f->delay[s].output - f->delay[s].input)
    case CHORUSDEPTH_PARAM:
      // chorus depth -> modulation gain, scaled by this section's delay length
      f->delay[s].rsgain = (((f32)val) / CONVERT) * LENGTH;
      break;
    case LPFILT_PARAM:
      // retune the damping low-pass and rebuild its filter coefficients
      if (f->delay[s].lp) {
        f->delay[s].lp->fc = (s16)val;
#ifdef _OLD_AUDIO_LIBRARY
        init_lpfilter(f->delay[s].lp);
#else
        _init_lpfilter(f->delay[s].lp);
#endif
      }
      break;
  }
  return 0;
}

/*
 * Load this section's delayed ("output" tap) samples into DMEM buffer `buff`.
 * With no resampler it is a straight load of one frame from the output tap.
 * With a resampler (chorus) it modulates the read position and source-sample
 * count via _doModFunc, loads count+alignment samples, and resamples them back
 * to a full frame so the delay tap appears to sweep. Returns the advanced
 * command pointer.
 */
Acmd* _n_loadOutputBuffer(ALFx* r, ALDelay* d, s32 buff, Acmd* p) {
  Acmd* ptr = p;
#ifndef N_MICRO
  s32 ratio, count, rbuff = AL_TEMP_2;
#else
  s32 ratio, count,
      rbuff = N_AL_TEMP_2;  // scratch DMEM for the pre-resample load
#endif
  s16* out_ptr;
  f32 fincount;
  f32 fratio;
  f32 delta;
  s32 ramalign = 0;
  s32 length;
  static f32 val = 0.0;
  static f32 lastval = -10.0;
  static f32 blob = 0;
  s32 incount =
      FIXED_SAMPLE;  // samples wanted out of the resampler (one frame)

  if (d->rs) {
    length = d->output - d->input;

    /* Modulate the read position, quantized to the resampler's pitch step. */
    delta = _doModFunc(d, incount);
    delta /= length;
    delta = (s32)(delta * UNITY_PITCH);
    delta = delta / UNITY_PITCH;
    fratio = 1.0 - delta;

    /* Accumulate the fractional source-sample count across frames. */
    fincount = d->rs->delta + (fratio * (f32)incount);
    count = (s32)fincount;
    d->rs->delta = fincount - (f32)count;

    /* Align the DRAM read to an 8-byte boundary, compensating in DMEM. */
    out_ptr = &r->input[-(d->output - d->rsdelta)];
    ramalign = ((s32)out_ptr & 0x7) >> 1;
    ptr = _n_loadBuffer(r, out_ptr - ramalign, rbuff, count + ramalign, ptr);
    ratio = (s32)(fratio * UNITY_PITCH);
#ifndef N_MICRO
    aSetBuffer(ptr++, 0, rbuff + (ramalign << 1), buff, incount << 1);
    aResample(ptr++, d->rs->first, ratio, osVirtualToPhysical(d->rs->state));
#else
#include "inc/n_reverb_add04.inc"
#endif
    d->rs->first = 0;
    d->rsdelta += count - incount;  // carry the source/dest length drift
  } else {
    /* No chorus: load one frame straight from the output tap. */
    out_ptr = &r->input[-d->output];
    ptr = _n_loadBuffer(r, out_ptr, buff, FIXED_SAMPLE, ptr);
  }
  return ptr;
}

/*
 * Load `count` samples from the DRAM delay line at curr_ptr into DMEM buffer
 * `buff`, handling the ring buffer: a pointer below base wraps forward, and a
 * read that runs past the end is split into a pre-wrap load plus a wrapped load
 * from base. Returns the advanced command pointer.
 */
Acmd* _n_loadBuffer(ALFx* r, s16* curr_ptr, s32 buff, s32 count, Acmd* p) {
  Acmd* ptr = p;
  s32 after_end;
  s32 before_end;  // wrapped / pre-wrap chunk sample counts
  s16 *updated_ptr, *delay_end;
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif
  delay_end = &r->base[r->length];
#ifdef _DEBUG
  if (curr_ptr > delay_end)
    __osError(ERR_ALMODDELAYOVERFLOW, 1, delay_end - curr_ptr);
#endif
  if (curr_ptr < r->base) {
    curr_ptr += r->length;  // wrap a pre-base pointer
  }
  updated_ptr = curr_ptr + count;

  if (updated_ptr > delay_end) {
    /* Read crosses the ring end: split into pre-wrap and wrapped chunks. */
    after_end = updated_ptr - delay_end;
    before_end = delay_end - curr_ptr;
#ifndef N_MICRO
    aSetBuffer(ptr++, 0, buff, 0, before_end << 1);
    aLoadBuffer(ptr++, osVirtualToPhysical(curr_ptr));
    aSetBuffer(ptr++, 0, buff + (before_end << 1), 0, after_end << 1);
    aLoadBuffer(ptr++, osVirtualToPhysical(r->base));
  } else {
    aSetBuffer(ptr++, 0, buff, 0, count << 1);
    aLoadBuffer(ptr++, osVirtualToPhysical(curr_ptr));
  }
  aSetBuffer(ptr++, 0, 0, 0, count << 1);
#else
#include "inc/n_reverb_add01.inc"
#endif
#ifdef AUD_PROFILE
  PROFILE_AUD(load_num, load_cnt, load_max, load_min);
#endif
  return ptr;
}

/*
 * Save one frame from DMEM buffer `buff` back into the DRAM delay line at
 * curr_ptr, with the same ring-buffer handling as _n_loadBuffer: a pre-base
 * pointer wraps forward and a write past the end splits into two saves. Returns
 * the advanced command pointer.
 */
Acmd* _n_saveBuffer(ALFx* r, s16* curr_ptr, s32 buff, Acmd* p) {
  Acmd* ptr = p;
  s32 after_end;
  s32 before_end;  // wrapped / pre-wrap chunk sample counts
  s16 *updated_ptr, *delay_end;
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif
  delay_end = &r->base[r->length];
  if (curr_ptr < r->base) {
    curr_ptr += r->length;  // wrap a pre-base pointer
  }
  updated_ptr = curr_ptr + FIXED_SAMPLE;

  if (updated_ptr > delay_end) {
    /* Write crosses the ring end: split into pre-wrap and wrapped chunks. */
    after_end = updated_ptr - delay_end;
    before_end = delay_end - curr_ptr;
#ifndef N_MICRO
    aSetBuffer(ptr++, 0, 0, buff, before_end << 1);
    aSaveBuffer(ptr++, osVirtualToPhysical(curr_ptr));
    aSetBuffer(ptr++, 0, 0, buff + (before_end << 1), after_end << 1);
    aSaveBuffer(ptr++, osVirtualToPhysical(r->base));
    aSetBuffer(ptr++, 0, 0, 0, FIXED_SAMPLE << 1);
  } else {
    aSetBuffer(ptr++, 0, 0, buff, FIXED_SAMPLE << 1);
    aSaveBuffer(ptr++, osVirtualToPhysical(curr_ptr));
  }
#else
#include "inc/n_reverb_add02.inc"
#endif
#ifdef AUD_PROFILE
  PROFILE_AUD(save_num, save_cnt, save_max, save_min);
#endif
  return ptr;
}

/*
 * Apply the section's one-pole damping low-pass to the frame in DMEM buffer
 * `buff`, in place, by loading the pole coefficients and running the pole
 * filter. Clears the filter's first-run flag. Returns the advanced command
 * pointer.
 */
Acmd* _n_filterBuffer(ALLowPass* lp, s32 buff, Acmd* p) {
  Acmd* ptr = p;
#ifndef N_MICRO
  aSetBuffer(ptr++, 0, buff, buff, FIXED_SAMPLE << 1);
  aLoadADPCM(ptr++, 32, osVirtualToPhysical(lp->fcvec.fccoef));
  aPoleFilter(ptr++, lp->first, lp->fgain, osVirtualToPhysical(lp->fstate));
#else
#include "inc/n_reverb_add03.inc"
#endif
  lp->first = 0;
  return ptr;
}
