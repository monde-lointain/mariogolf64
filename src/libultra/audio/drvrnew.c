/*
 * drvrnew.c
 *
 * Constructors for the synthesis filter objects (decoder, resampler, envelope
 * mixer, buses, save) and the effects unit. Also holds the built-in reverb/
 * chorus/flange presets and the low-pass-filter coefficient setup that the
 * effects constructor draws on.
 */
#include <libaudio.h>
#include "synthInternals.h"
#include <os.h>
#include <stdio.h>
#include "initfx.h"
#ident "$Revision: 1.49 $"

#define SCALE 16384

/* Convert a delay length in milliseconds to samples at ~44.1 kHz, floored to
 * a multiple of 8 (the RSP processes audio in 8-sample groups). */
#define ms *(((s32)((f32)44.1)) & ~0x7)

/*
 * Effects presets, read positionally by alFxNew. Layout: [0]=section count,
 * [1]=total delay-line length, then per section eight fields in order:
 * input tap, output tap, feedback coef, feedforward coef, gain, chorus rate,
 * chorus depth, low-pass cutoff. "ms" delays are in samples; coefs are s16
 * fixed-point; a zero rate/cutoff disables that stage.
 */
// clang-format off: positional preset rows, one delay section per group
static s32 SMALLROOM_PARAMS[26] = {
    3, 100 ms, 0,     54 ms, 9830,  -9830,  0, 0,     0,
    0, 19 ms,  38 ms, 3276,  -3276, 0x3fff, 0, 0,     0,
    0, 60 ms,  5000,  0,     0,     0,      0, 0x5000};
static s32 BIGROOM_PARAMS[34] = {4,      100 ms, 0, 66 ms, 9830,  -9830, 0,
                                 0,      0,      0, 22 ms, 54 ms, 3276,  -3276,
                                 0x3fff, 0,      0, 0,     66 ms, 91 ms, 3276,
                                 -3276,  0x3fff, 0, 0,     0,     0,     94 ms,
                                 8000,   0,      0, 0,     0,     0x5000};
static s32 ECHO_PARAMS[10] = {1, 200 ms, 0, 179 ms, 12000, 0, 0x7fff, 0, 0, 0};
static s32 CHORUS_PARAMS[10] = {1, 20 ms,  0,    5 ms, 0x4000,
                                0, 0x7fff, 7600, 700,  0};
static s32 FLANGE_PARAMS[10] = {1,      20 ms,  0,   5 ms, 0,
                                0x5fff, 0x7fff, 380, 500,  0};
static s32 NULL_PARAMS[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
// clang-format on

/*
 * Build the 16-tap low-pass coefficient vector for one reverb section from its
 * scalar cutoff (lp->fc). The taps are a geometric series of the normalized
 * cutoff: the first eight entries are zero, then the cutoff itself, then
 * successive powers, giving the single-pole filter its roll-off.
 */
void _init_lpfilter(ALLowPass* lp) {
  s32 i, temp;
  s16 fc;
  f64 ffc, fcoef;

  temp = lp->fc * SCALE;
  fc = temp >> 15;
  lp->fgain = SCALE - fc;
  lp->first = 1;

  for (i = 0; i < 8; i++) lp->fcvec.fccoef[i] = 0;
  lp->fcvec.fccoef[i++] = fc;

  fcoef = ffc = (f64)fc / SCALE;
  for (; i < 16; i++) {
    fcoef *= ffc;
    lp->fcvec.fccoef[i] = (s16)(fcoef * SCALE);
  }
}

/*
 * Construct the effects (reverb) unit. Selects the preset for c->fxType (or
 * the caller's custom table), allocates the delay-line sections and the shared
 * sample ring from the heap, and configures each section's taps, coefs, and
 * optional resampler (chorus/flange modulation) and low-pass stage.
 */
void alFxNew(ALFx* r, ALSynConfig* c, ALHeap* hp) {
  u16 i, j, k;
  s32* param = 0;
  ALFilter* f = (ALFilter*)r;
  ALDelay* d;

  alFilterNew(f, 0, alFxParam, AL_FX);
  f->handler = alFxPull;
  r->paramHdl = (ALSetFXParam)alFxParamHdl;

  /* Pick the preset table that drives this effect type. */
  switch (c->fxType) {
    case AL_FX_SMALLROOM:
      param = SMALLROOM_PARAMS;
      break;
    case AL_FX_BIGROOM:
      param = BIGROOM_PARAMS;
      break;
    case AL_FX_ECHO:
      param = ECHO_PARAMS;
      break;
    case AL_FX_CHORUS:
      param = CHORUS_PARAMS;
      break;
    case AL_FX_FLANGE:
      param = FLANGE_PARAMS;
      break;
    case AL_FX_CUSTOM:
      param = c->params;
      break;
    default:
      param = NULL_PARAMS;
      break;
  }

  /* Header fields, then the delay ring buffer, zeroed. */
  j = 0;
  r->section_count = param[j++];
  r->length = param[j++];
  r->delay = alHeapAlloc(hp, r->section_count, sizeof(ALDelay));
  r->base = alHeapAlloc(hp, r->length, sizeof(s16));
  r->input = r->base;
  for (k = 0; k < r->length; k++) r->base[k] = 0;

  /* Walk the preset, configuring one delay section per iteration. */
  for (i = 0; i < r->section_count; i++) {
    d = &r->delay[i];
    d->input = param[j++];
    d->output = param[j++];
    d->fbcoef = param[j++];
    d->ffcoef = param[j++];
    d->gain = param[j++];

    /* A non-zero chorus rate adds a resampler that modulates this tap. */
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

    /* A non-zero cutoff adds a low-pass filter on this tap's output. */
    if (param[j]) {
      d->lp = alHeapAlloc(hp, 1, sizeof(ALLowPass));
      d->lp->fstate = alHeapAlloc(hp, 1, sizeof(POLEF_STATE));
      d->lp->fc = param[j++];
      _init_lpfilter(d->lp);
    } else {
      d->lp = 0;
      j++;
    }
  }
}

/* Construct the envelope mixer: allocate its RSP state and reset volume/pan
 * tracking and the pending-update list to silence. */
void alEnvmixerNew(ALEnvMixer* e, ALHeap* hp) {
  alFilterNew((ALFilter*)e, alEnvmixerPull, alEnvmixerParam, AL_ENVMIX);
  e->state = alHeapAlloc(hp, 1, sizeof(ENVMIX_STATE));
  e->first = 1;
  e->motion = AL_STOPPED;
  e->volume = 1;
  e->ltgt = 1;
  e->rtgt = 1;
  e->cvolL = 1;
  e->cvolR = 1;
  e->dryamt = 0;
  e->wetamt = 0;
  e->lratm = 1;
  e->lratl = 0;
  e->lratm = 1;
  e->lratl = 0;
  e->delta = 0;
  e->segEnd = 0;
  e->pan = 0;
  e->ctrlList = 0;
  e->ctrlTail = 0;
  e->sources = 0;
}

/* Construct the ADPCM load/decode filter, allocating its decode state blocks
 * and the application-supplied DMA context. */
void alLoadNew(ALLoadFilter* f, ALDMANew dmaNew, ALHeap* hp) {
  s32 i;
  alFilterNew((ALFilter*)f, alAdpcmPull, alLoadParam, AL_ADPCM);
  f->state = alHeapAlloc(hp, 1, sizeof(ADPCM_STATE));
  f->lstate = alHeapAlloc(hp, 1, sizeof(ADPCM_STATE));
  f->dma = dmaNew(&f->dmaState);
  f->lastsam = 0;
  f->first = 1;
  f->memin = 0;
}

/* Construct a stand-alone resampler filter at unity ratio. */
void alResampleNew(ALResampler* r, ALHeap* hp) {
  alFilterNew((ALFilter*)r, alResamplePull, alResampleParam, AL_RESAMPLE);
  r->state = alHeapAlloc(hp, 1, sizeof(RESAMPLE_STATE));
  r->delta = 0.0;
  r->first = 1;
  r->motion = AL_STOPPED;
  r->ratio = 1.0;
  r->upitch = 0;
  r->ctrlList = 0;
  r->ctrlTail = 0;
}

/* Construct an aux-bus mixer over a caller-supplied source-pointer array. */
void alAuxBusNew(ALAuxBus* m, void* sources, s32 maxSources) {
  alFilterNew((ALFilter*)m, alAuxBusPull, alAuxBusParam, AL_AUXBUS);
  m->sourceCount = 0;
  m->maxSources = maxSources;
  m->sources = (ALFilter**)sources;
}

/* Construct the main-bus mixer over a caller-supplied source-pointer array. */
void alMainBusNew(ALMainBus* m, void* sources, s32 maxSources) {
  alFilterNew((ALFilter*)m, alMainBusPull, alMainBusParam, AL_MAINBUS);
  m->sourceCount = 0;
  m->maxSources = maxSources;
  m->sources = (ALFilter**)sources;
}

/* Construct the save (output) filter. */
void alSaveNew(ALSave* f) {
  alFilterNew((ALFilter*)f, alSavePull, alSaveParam, AL_SAVE);
  f->dramout = 0;
  f->first = 1;
}
