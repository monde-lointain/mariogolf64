/*
 * env.c
 *
 * Envelope mixer filter: the per-voice node that applies volume, panning, and
 * dry/wet (effects) mix to a resampled voice. Its pull handler walks a
 * time-ordered list of control events (start/stop a voice, change volume, pan,
 * or fx amount), rendering the audio between events as sub-frames with
 * interpolated L/R volume ramps. The float helpers below compute those ramp
 * coefficients in the log domain.
 */
#include <libaudio.h>
#include "synthInternals.h"
#include <os.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#ident "$Revision: 1.49 $"
#ident "$Revision: 1.17 $"

#ifdef AUD_PROFILE
extern u32 cnt_index, env_num, env_cnt, env_max, env_min, lastCnt[];
extern u32 rate_num, rate_cnt, rate_max, rate_min;
extern u32 vol_num, vol_cnt, vol_max, vol_min;
#endif

/* Equal-power pan curve: a quarter cosine in s16 fixed point. Indexing it by
 * pan gives the left gain; indexing by (LENGTH-pan-1) gives the right, so the
 * two channels sum to constant power across the stereo field. */
#define EQPOWER_LENGTH 128
static s16 eqpower[EQPOWER_LENGTH] = {
    32767, 32764, 32757, 32744, 32727, 32704, 32677, 32644, 32607, 32564, 32517,
    32464, 32407, 32344, 32277, 32205, 32127, 32045, 31958, 31866, 31770, 31668,
    31561, 31450, 31334, 31213, 31087, 30957, 30822, 30682, 30537, 30388, 30234,
    30075, 29912, 29744, 29572, 29395, 29214, 29028, 28838, 28643, 28444, 28241,
    28033, 27821, 27605, 27385, 27160, 26931, 26698, 26461, 26220, 25975, 25726,
    25473, 25216, 24956, 24691, 24423, 24151, 23875, 23596, 23313, 23026, 22736,
    22442, 22145, 21845, 21541, 21234, 20924, 20610, 20294, 19974, 19651, 19325,
    18997, 18665, 18331, 17993, 17653, 17310, 16965, 16617, 16266, 15913, 15558,
    15200, 14840, 14477, 14113, 13746, 13377, 13006, 12633, 12258, 11881, 11503,
    11122, 10740, 10357, 9971,  9584,  9196,  8806,  8415,  8023,  7630,  7235,
    6839,  6442,  6044,  5646,  5246,  4845,  4444,  4042,  3640,  3237,  2833,
    2429,  2025,  1620,  1216,  810,   405,   0};

extern f64 __pow(f64, f64);

static Acmd* _pullSubFrame(void* filter, s16* inp, s16* outp, s32 outCount,
                           s32 sampleOffset, Acmd* p);
static s16 _getRate(f64 vol, f64 tgt, s32 count, u16* ratel);
static f32 _getVol(f32 ivol, s32 samples, s16 ratem, u16 ratel);

/*
 * Envelope-mixer pull handler. Consumes the pending control list in sample
 * order: for each event it renders the sub-frame up to that event's offset,
 * then applies the event (start/stop/free a voice, retarget volume/pan/fx),
 * and finally renders any remaining samples. Param events recompute the L/R
 * current volumes by interpolating along the active ramp.
 */
Acmd* alEnvmixerPull(void* filter, s16* outp, s32 outCount, s32 sampleOffset,
                     Acmd* p) {
  Acmd* ptr = p;
  ALEnvMixer* e = (ALEnvMixer*)filter;
  s16 inp;
  s32 lastOffset;
  s32 thisOffset = sampleOffset;
  s32 samples;
  s16 loutp = 0;
  s32 fVol;
  ALParam* thisParam;
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif
  inp = AL_RESAMPLER_OUT;
  while (e->ctrlList != 0) {
    /* Samples to render before the next control event fires. */
    lastOffset = thisOffset;
    thisOffset = e->ctrlList->delta;
    samples = thisOffset - lastOffset;
    if (samples > outCount) {
      break;
    }
#if BUILD_VERSION < VERSION_J
#line 103
#endif
#ifdef _DEBUG
    assert(samples >= 0);
    assert(samples <= AL_MAX_RSP_SAMPLES);
#endif
    switch (e->ctrlList->type) {
      case (AL_FILTER_START_VOICE_ALT): {
        /* Begin a voice with explicit volume/pan/fx/pitch from the event. */
        ALStartParamAlt* param = (ALStartParamAlt*)e->ctrlList;
        ALFilter* f = (ALFilter*)e;
        s32 tmp;
        if (param->unity) {
          (*e->filter.setParam)(&e->filter, AL_FILTER_SET_UNITY_PITCH, 0);
        }
        (*e->filter.setParam)(&e->filter, AL_FILTER_SET_WAVETABLE, param->wave);
        (*e->filter.setParam)(&e->filter, AL_FILTER_START, 0);
        e->first = 1;
        e->delta = 0;
        e->segEnd = param->samples;
        /* Square the requested volume to an s16 perceptual curve. */
        tmp = ((s32)param->volume * (s32)param->volume) >> 15;
        e->volume = (s16)tmp;
        e->pan = param->pan;
        e->dryamt = eqpower[param->fxMix];
        e->wetamt = eqpower[EQPOWER_LENGTH - param->fxMix - 1];
        if (param->samples) {
          e->cvolL = 1;
          e->cvolR = 1;
        } else {
          /* No ramp window: jump straight to the panned target volume. */
          e->cvolL = (e->volume * eqpower[e->pan]) >> 15;
          e->cvolR = (e->volume * eqpower[EQPOWER_LENGTH - e->pan - 1]) >> 15;
        }
        if (f->source) {
          union {
            f32 f;
            s32 i;
          } data;
          data.f = param->pitch;
          (*f->source->setParam)(f->source, AL_FILTER_SET_PITCH, (void*)data.i);
        }
      } break;
      case (AL_FILTER_SET_FXAMT):
      case (AL_FILTER_SET_PAN):
      case (AL_FILTER_SET_VOLUME):
        /* Render up to here, then settle or advance the volume ramp and apply
         * the new pan / volume / fx target. */
        ptr = _pullSubFrame(e, &inp, &loutp, samples, sampleOffset, ptr);
        if (e->delta >= e->segEnd) {
          e->ltgt = (e->volume * eqpower[e->pan]) >> 15;
          e->rtgt = (e->volume * eqpower[EQPOWER_LENGTH - e->pan - 1]) >> 15;
          e->delta = e->segEnd;
          e->cvolL = e->ltgt;
          e->cvolR = e->rtgt;
        } else {
          e->cvolL = _getVol(e->cvolL, e->delta, e->lratm, e->lratl);
          e->cvolR = _getVol(e->cvolR, e->delta, e->rratm, e->rratl);
        }
        if (e->cvolL == 0) {
          e->cvolL = 1;
        }
        if (e->cvolR == 0) {
          e->cvolR = 1;
        }
        if (e->ctrlList->type == AL_FILTER_SET_PAN) {
          e->pan = (s16)e->ctrlList->data.i;
        }
        if (e->ctrlList->type == AL_FILTER_SET_VOLUME) {
          e->delta = 0;
          fVol = (e->ctrlList->data.i);
          fVol = (fVol * fVol) >> 15;
          e->volume = (s16)fVol;
          e->segEnd = e->ctrlList->moredata.i;
        }
        if (e->ctrlList->type == AL_FILTER_SET_FXAMT) {
          e->dryamt = eqpower[e->ctrlList->data.i];
          e->wetamt = eqpower[EQPOWER_LENGTH - e->ctrlList->data.i - 1];
        }
        e->first = 1;
        break;
      case (AL_FILTER_START_VOICE): {
        /* Begin a voice; volume/pan stay at their current settings. */
        ALStartParam* p = (ALStartParam*)e->ctrlList;
        if (p->unity) {
          (*e->filter.setParam)(&e->filter, AL_FILTER_SET_UNITY_PITCH, 0);
        }
        (*e->filter.setParam)(&e->filter, AL_FILTER_SET_WAVETABLE, p->wave);
        (*e->filter.setParam)(&e->filter, AL_FILTER_START, 0);
      } break;
      case (AL_FILTER_STOP_VOICE): {
        /* Render up to here, then reset the source filter. */
        ptr = _pullSubFrame(e, &inp, &loutp, samples, sampleOffset, ptr);
        (*e->filter.setParam)(&e->filter, AL_FILTER_RESET, 0);
      } break;
      case (AL_FILTER_FREE_VOICE): {
        /* Return the physical voice to the driver's free pool. */
        ALSynth* drvr = &alGlobals->drvr;
        ALFreeParam* param = (ALFreeParam*)e->ctrlList;
        param->pvoice->offset = 0;
        _freePVoice(drvr, (PVoice*)param->pvoice);
      } break;
      default:
        /* Unknown event: render up to it and forward it to the source. */
        ptr = _pullSubFrame(e, &inp, &loutp, samples, sampleOffset, ptr);
        (*e->filter.setParam)(&e->filter, e->ctrlList->type,
                              (void*)e->ctrlList->data.i);
        break;
    }

    /* Advance past the rendered span and consume the event, freeing it. */
    loutp += (samples << 1);
    outCount -= samples;
    thisParam = e->ctrlList;
    e->ctrlList = e->ctrlList->next;
    if (e->ctrlList == 0) {
      e->ctrlTail = 0;
    }
    __freeParam(thisParam);
  }

  /* Render whatever is left after the last event in this frame. */
  ptr = _pullSubFrame(e, &inp, &loutp, outCount, sampleOffset, ptr);
  if (e->delta > e->segEnd) {
    e->delta = e->segEnd;
  }
#ifdef AUD_PROFILE
  PROFILE_AUD(env_num, env_cnt, env_max, env_min);
#endif
  return ptr;
}

/*
 * Envelope-mixer param setter. ADD_UPDATE appends a control event to the
 * pending list; RESET/START toggle motion state; SET_SOURCE wires the upstream
 * voice. Unknown ops cascade to the source filter.
 */
s32 alEnvmixerParam(void* filter, s32 paramID, void* param) {
  ALFilter* f = (ALFilter*)filter;
  ALEnvMixer* e = (ALEnvMixer*)filter;
  switch (paramID) {
    case (AL_FILTER_ADD_UPDATE):
      if (e->ctrlTail) {
        e->ctrlTail->next = (ALParam*)param;
      } else {
        e->ctrlList = (ALParam*)param;
      }
      e->ctrlTail = (ALParam*)param;
      break;
    case (AL_FILTER_RESET):
      e->first = 1;
      e->motion = AL_STOPPED;
      e->volume = 1;
      if (f->source) {
        (*f->source->setParam)(f->source, AL_FILTER_RESET, param);
      }
      break;
    case (AL_FILTER_START):
      e->motion = AL_PLAYING;
      if (f->source) {
        (*f->source->setParam)(f->source, AL_FILTER_START, param);
      }
      break;
    case (AL_FILTER_SET_SOURCE):
      f->source = (ALFilter*)param;
      break;
    default:
      if (f->source) {
        (*f->source->setParam)(f->source, paramID, param);
      }
  }
  return 0;
}

#if BUILD_VERSION < VERSION_J
#line 350
#endif

/*
 * Render outCount samples of the active voice with the current envelope. Pulls
 * the voice from its source, points the RSP at the L/R/aux output buffers, and
 * on the first sub-frame of a segment seeds the per-channel volume and ramp
 * rate (via aSetVolume) before running the env mixer; later sub-frames just
 * continue the ramp. A stopped or empty voice produces nothing.
 */
static Acmd* _pullSubFrame(void* filter, s16* inp, s16* outp, s32 outCount,
                           s32 sampleOffset, Acmd* p) {
  Acmd* ptr = p;
  ALEnvMixer* e = (ALEnvMixer*)filter;
  ALFilter* source = e->filter.source;
  if (e->motion != AL_PLAYING || !outCount) {
    return ptr;
  }
#ifdef _DEBUG
  assert(source);
#endif
  ptr = (*source->handler)(source, inp, outCount, sampleOffset, p);
  aSetBuffer(ptr++, A_MAIN, *inp, AL_MAIN_L_OUT + *outp, outCount << 1);
  aSetBuffer(ptr++, A_AUX, AL_MAIN_R_OUT + *outp, AL_AUX_L_OUT + *outp,
             AL_AUX_R_OUT + *outp);
  if (e->first) {
    /* First sub-frame of a segment: program the volume ramp endpoints. */
    e->first = 0;
    e->ltgt = (e->volume * eqpower[e->pan]) >> 15;
    e->lratm = _getRate((f64)e->cvolL, (f64)e->ltgt, e->segEnd, &(e->lratl));
    e->rtgt = (e->volume * eqpower[EQPOWER_LENGTH - e->pan - 1]) >> 15;
    e->rratm = _getRate((f64)e->cvolR, (f64)e->rtgt, e->segEnd, &(e->rratl));
    aSetVolume(ptr++, A_LEFT | A_VOL, e->cvolL, 0, 0);
    aSetVolume(ptr++, A_RIGHT | A_VOL, e->cvolR, 0, 0);
    aSetVolume(ptr++, A_LEFT | A_RATE, e->ltgt, e->lratm, e->lratl);
    aSetVolume(ptr++, A_RIGHT | A_RATE, e->rtgt, e->rratm, e->rratl);
    aSetVolume(ptr++, A_AUX, e->dryamt, 0, e->wetamt);
    aEnvMixer(ptr++, A_INIT | A_AUX, osVirtualToPhysical(e->state));
  } else
    aEnvMixer(ptr++, A_CONTINUE | A_AUX, osVirtualToPhysical(e->state));
  *inp += (outCount << 1);
  e->delta += outCount;
  return ptr;
}

/* Bit masks for the IEEE-754 single-precision exponent and mantissa fields,
 * used by the local frexp/ldexp pair below. */
#define EXP_MASK 0x7f800000
#define MANT_MASK 0x807fffff

/* Split value into a normalized fraction in [0.5, 1.0) and a power of two,
 * returned through eptr. A local frexp avoids pulling in the libc routine. */
f64 _frexpf(f64 value, s32* eptr) {
  f64 absvalue;
  *eptr = 0;
  if (value == 0.0) {
    return (value);
  }
  absvalue = (value > 0.0) ? value : -value;
  for (; absvalue >= 1.0; absvalue *= 0.5) {
    ++*eptr;
  }
  for (; absvalue < 0.5; absvalue += absvalue) {
    --*eptr;
  }
  return (value > 0.0 ? absvalue : -absvalue);
}

/* Scale in by 2^ex. A local ldexp counterpart to _frexpf. */
f64 _ldexpf(f64 in, s32 ex) {
  s32 exp;
  if (ex) {
    exp = 1 << ex;
    in *= (f64)exp;
  }
  return (in);
}

/*
 * Compute the per-sample volume-ramp multiplier that carries vol to tgt over
 * count samples, as the RSP's split rate (s16 integer return plus u16
 * fractional part through ratel). The nth root tgt/vol is evaluated in the log
 * domain: a small log table plus fixed-point exponentiation, avoiding a real
 * pow() per voice. count == 0 means snap immediately (full up or full down).
 */
static s16 _getRate(f64 vol, f64 tgt, s32 count, u16* ratel) {
  s16 s;
  f64 invn = 1.0 / count;
  f64 eps;
  f64 a;
  f64 fs;
  f64 mant;
  s32 i_invn, ex, indx;
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif
  if (count == 0) {
    if (tgt >= vol) {
      *ratel = 0xffff;
      return 0x7fff;
    }
    *ratel = 0;
    return 0;
  }
  if (tgt < 1.0) {
    tgt = 1.0;
  }
  if (vol <= 0) {
    vol = 1;
  }
#define NBITS (3)
#define NPOS (1 << NBITS)
#define NFRACBITS (30)
#define M_LN2 0.69314718055994530942
  {
    /* log of the mantissa sampled at NPOS points; combined with the binary
     * exponent it gives ln(tgt/vol), then exp() by repeated squaring. */
    f64 logtab[] = {-0.912537, -0.752072, -0.607683, -0.476438,
                    -0.356144, -0.245112, -0.142019, -0.045804};
    i_invn = (s32)_ldexpf(invn, NFRACBITS);
    mant = _frexpf(tgt / vol, &ex);
    indx = (s32)(_ldexpf(mant, NBITS + 1));
    eps = (logtab[indx - NPOS] + ex) * M_LN2;
    eps /= _ldexpf(1, NFRACBITS);
    fs = (1.0 + eps);
    a = 1.0;
    while (i_invn) {
      if (i_invn & 1) {
        a = a * fs;
      }
      fs *= fs;
      i_invn >>= 1;
    }
  }
  a *= (a *= (a *= a));
  s = (s16)a;
  *ratel = (s16)(0xffff * (a - (f32)s));
#ifdef AUD_PROFILE
  PROFILE_AUD(rate_num, rate_cnt, rate_max, rate_min);
#endif
  return (s16)a;
}

/*
 * Project a starting volume ivol forward by samples (in 8-sample steps) along
 * a ramp whose per-step multiplier is the split (ratem, ratel) fixed-point
 * rate. Uses exponentiation by squaring so a long ramp costs a few multiplies.
 */
static f32 _getVol(f32 ivol, s32 samples, s16 ratem, u16 ratel) {
  f32 r;
  f32 a;
  s32 i;
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif
  samples >>= 3;
  if (samples == 0) {
    return ivol;
  }
  r = ((f32)(ratem << 16) + (f32)ratel) / 65536;
  a = 1.0;
  for (i = 0; i < 32; i++) {
    if (samples & 1) {
      a *= r;
    }
    samples >>= 1;
    if (samples == 0) {
      break;
    }
    r *= r;
  }
  ivol *= a;
#ifdef AUD_PROFILE
  PROFILE_AUD(vol_num, vol_cnt, vol_max, vol_min);
#endif
  return ivol;
}
