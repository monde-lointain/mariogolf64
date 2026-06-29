/*
 * n_drvrNew.c
 *
 * naudio constructors for the effects unit and a physical voice. Holds the
 * built-in reverb/echo/chorus/flange presets and n_alFxNew, which builds an
 * ALFx delay-line network from the selected preset, plus alN_PVoiceNew, which
 * sets up one physical voice's decoder, resampler, and envelope-mixer state.
 * All storage comes from the audio heap; both routines run at synthesizer init.
 */
#include "n_synthInternals.h"
#include <os.h>

/* Fixed-point unit (1.0 == 16384) for the low-pass filter coefficients. Unused
   in this file: it is carried over from the drvrnew.c source this mirrors,
   where SCALE feeds the local _init_lpfilter; in this naudio split
   _init_lpfilter is defined elsewhere, so the macro is vestigial here. */
#define SCALE 16384

/* Convert a delay length in milliseconds to samples at ~44.1 kHz, floored to a
   multiple of 8 (the RSP processes audio in 8-sample groups): so the suffix
   "ms" multiplies by 40, e.g. 100 ms == 4000 samples. */
#define ms *(((s32)((f32)44.1)) & ~0x7)

/*
 * Built-in effect presets, read positionally by n_alFxNew. Layout: [0] = number
 * of delay sections, [1] = total delay-line length, then eight fields per
 * section in order: input tap, output tap, feedback coef, feedforward coef,
 * gain, chorus/flange rate, chorus/flange depth, low-pass cutoff. Delays use
 * the "ms" macro (samples); coefs/gains are s16 fixed-point; a zero rate or
 * cutoff disables that section's modulation or filter stage.
 */
// clang-format off: positional preset rows, one delay section per group
s32 SMALLROOM_PARAMS_N[26] = {
    3, 100 ms, 0,     54 ms, 9830,  -9830,  0, 0,     0,
    0, 19 ms,  38 ms, 3276,  -3276, 0x3fff, 0, 0,     0,
    0, 60 ms,  5000,  0,     0,     0,      0, 0x5000};
s32 BIGROOM_PARAMS_N[34] = {4,      100 ms, 0, 66 ms, 9830,  -9830, 0,
                            0,      0,      0, 22 ms, 54 ms, 3276,  -3276,
                            0x3fff, 0,      0, 0,     66 ms, 91 ms, 3276,
                            -3276,  0x3fff, 0, 0,     0,     0,     94 ms,
                            8000,   0,      0, 0,     0,     0x5000};
s32 ECHO_PARAMS_N[10] = {1, 200 ms, 0, 179 ms, 12000, 0, 0x7fff, 0, 0, 0};
s32 CHORUS_PARAMS_N[10] = {1, 20 ms, 0, 5 ms, 0x4000, 0, 0x7fff, 7600, 700, 0};
s32 FLANGE_PARAMS_N[10] = {1, 20 ms, 0, 5 ms, 0, 0x5fff, 0x7fff, 380, 500, 0};
s32 NULL_PARAMS_N[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
// clang-format on

/*
 * Construct an effects unit (reverb/echo/chorus/flange) from the configured
 * preset and return it through *fx_ar. Allocates the ALFx, its delay-line
 * array, and a zeroed delay buffer from the heap, then initializes each delay
 * section's taps, coefficients, and gain, plus an optional modulating resampler
 * (chorus/flange) and an optional one-pole low-pass on its output.
 */
void n_alFxNew(ALFx** fx_ar, ALSynConfig* c, ALHeap* hp) {
  u16 i;
  u16 j;
  u16 k;
  s32* param = 0;
  ALDelay* d;
  ALFx* r;
  *fx_ar = r = (ALFx*)alHeapAlloc(hp, 1, sizeof(ALFx));

  /* Select the preset parameter table for the requested effect (or the caller's
     custom table, or a do-nothing null preset). */
  switch (c->fxType) {
    case AL_FX_SMALLROOM:
      param = SMALLROOM_PARAMS_N;
      break;
    case AL_FX_BIGROOM:
      param = BIGROOM_PARAMS_N;
      break;
    case AL_FX_ECHO:
      param = ECHO_PARAMS_N;
      break;
    case AL_FX_CHORUS:
      param = CHORUS_PARAMS_N;
      break;
    case AL_FX_FLANGE:
      param = FLANGE_PARAMS_N;
      break;
    case AL_FX_CUSTOM:
      param = c->params;
      break;
    default:
      param = NULL_PARAMS_N;
      break;
  }

  /* Read the header (section count, buffer length), allocate the delay-line
     array and a zeroed delay buffer; the write pointer starts at the base. */
  j = 0;
  r->section_count = param[j++];
  r->length = param[j++];
  r->delay = alHeapAlloc(hp, r->section_count, sizeof(ALDelay));
  r->base = alHeapAlloc(hp, r->length, sizeof(s16));
  r->input = r->base;
  for (k = 0; k < r->length; k++) {
    r->base[k] = 0;
  }

  /* Initialize each delay section from the preset stream. */
  for (i = 0; i < r->section_count; i++) {
    d = &r->delay[i];
    d->input = param[j++];
    d->output = param[j++];
    d->fbcoef = param[j++];
    d->ffcoef = param[j++];
    d->gain = param[j++];

    /* Non-zero rate: a modulating (chorus/flange) section. rsinc is the LFO
       phase step per sample; rsgain swings the tap span (LENGTH) by the
       modulation depth. The depth is in hundredths of a cent; dividing by
       CONVERT (== 120000/ln(2)) converts it to a frequency ratio. */
    if (param[j]) {
#define RANGE 2.0
      d->rsinc = ((((f32)param[j++]) / 1000) * RANGE) / c->outputRate;
#define CONVERT 173123.404906676
#define LENGTH (d->output - d->input)
      d->rsgain = (((f32)param[j++]) / CONVERT) * LENGTH;
      d->rsval = 1.0;
      d->rsdelta = 0.0;
      d->rs = alHeapAlloc(hp, 1, sizeof(ALResampler));
      d->rs->state = alHeapAlloc(hp, 1, sizeof(RESAMPLE_STATE));
      d->rs->delta = 0.0;
      d->rs->first = 1;
    } else {
      /* No modulation: skip the rate and depth slots. */
      d->rs = 0;
      j++;
      j++;
    }

    /* Non-zero cutoff: attach a one-pole low-pass to this section's output. */
    if (param[j]) {
      d->lp = alHeapAlloc(hp, 1, sizeof(ALLowPass));
      d->lp->fstate = alHeapAlloc(hp, 1, sizeof(POLEF_STATE));
      d->lp->fc = param[j++];
#ifdef _OLD_AUDIO_LIBRARY
      init_lpfilter(d->lp);
#else
      _init_lpfilter(d->lp);
#endif
    } else {
      /* No filter: skip the cutoff slot. */
      d->lp = 0;
      j++;
    }
  }
}

/*
 * Set up one physical voice mv. Allocates its ADPCM decoder, resampler, and
 * envelope-mixer working state from the heap, builds its sample-DMA callback
 * via dmaNew, and initializes every field to an idle default: first-pass flags
 * set, voice stopped, and unity ratios/volumes. Called once per voice at synth
 * init.
 */
void alN_PVoiceNew(N_PVoice* mv, ALDMANew dmaNew, ALHeap* hp) {
  /* ADPCM decoder: current/last decompression state, sample-DMA callback, and
     first-block flag. */
  mv->dc_state = alHeapAlloc(hp, 1, sizeof(ADPCM_STATE));
  mv->dc_lstate = alHeapAlloc(hp, 1, sizeof(ADPCM_STATE));
  mv->dc_dma = dmaNew(&mv->dc_dmaState);
  mv->dc_lastsam = 0;
  mv->dc_first = 1;
  mv->dc_memin = 0;

  /* Resampler: unity ratio, no pitch shift, first-pass flag. */
  mv->rs_state = alHeapAlloc(hp, 1, sizeof(RESAMPLE_STATE));
  mv->rs_delta = 0.0;
  mv->rs_first = 1;
  mv->rs_ratio = 1.0;
  mv->rs_upitch = 0;

  /* Envelope mixer: stopped, unity volume/targets, zeroed pan and rate ramps.
     The left-rate ramp (em_lratm/em_lratl) is assigned twice; this duplication
     is verbatim from the upstream alEnvmixerNew, not a naudio bug. */
  mv->em_state = alHeapAlloc(hp, 1, sizeof(ENVMIX_STATE));
  mv->em_first = 1;
  mv->em_motion = AL_STOPPED;
  mv->em_volume = 1;
  mv->em_ltgt = 1;
  mv->em_rtgt = 1;
  mv->em_cvolL = 1;
  mv->em_cvolR = 1;
  mv->em_dryamt = 0;
  mv->em_wetamt = 0;
  mv->em_lratm = 1;
  mv->em_lratl = 0;
  mv->em_lratm = 1;
  mv->em_lratl = 0;
  mv->em_delta = 0;
  mv->em_segEnd = 0;
  mv->em_pan = 0;
  mv->em_ctrlList = 0;
  mv->em_ctrlTail = 0;
}
