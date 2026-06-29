/*
 * n_env.c
 *
 * Envelope and mixer stage of the naudio ("Software Creations" n_audio_sc)
 * synthesis pipeline. Once per audio frame this stage pulls resampled
 * sub-frames from its source voice and applies the per-voice volume envelope
 * and stereo pan, emitting the RSP audio command list (Acmd) that the microcode
 * executes to scale and mix the samples. The naudio variant flattens an entire
 * voice into a single N_PVoice (rather than the base library's chain of
 * ALFilter stages) and drives the "micro" command stream (n_aSetVolume /
 * n_aEnvMixer) over a fixed 184-sample frame.
 */
#include "n_synthInternals.h"
#ifdef _DEBUG
#include <assert.h>
#endif

/* Optional per-stage cycle-count instrumentation (off unless AUD_PROFILE). */
#ifdef AUD_PROFILE
extern u32 cnt_index, env_num, env_cnt, env_max, env_min, lastCnt[];
extern u32 rate_num, rate_cnt, rate_max, rate_min;
extern u32 vol_num, vol_cnt, vol_max, vol_min;
#endif

/*
 * Equal-power pan / fx-mix gain curve: a quarter-cosine table of Q15 gains
 * (full scale 32767 down to 0) across 128 positions. For pan position p the
 * left gain is n_eqpower[p] and the right gain n_eqpower[127 - p]; because the
 * two sides are cosine/sine quadrature, the summed acoustic power stays roughly
 * constant as a voice pans across the stereo field. The same curve splits the
 * dry/wet (reverb-send) amount.
 */
#define N_EQPOWER_LENGTH 128
s16 n_eqpower[N_EQPOWER_LENGTH] = {
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

/* Soft-float helpers used only by the non-micro (geometric) envelope math. */
#ifndef N_MICRO
extern f64 __pow(f64, f64);
extern f64 _frexpf(f64 value, s32* eptr);
extern f64 _ldexpf(f64 in, s32 ex);
#endif

/* Internal helpers. _getVol's return type tracks the rate representation: a
 * multiplicative f32 factor for the non-micro path, an additive s16 step for
 * the micro path the build selects. */
static Acmd* _pullSubFrame(N_PVoice* pv, s16* inp, s16* outp, s32 outCount,
                           Acmd* p);
static s16 _getRate(f64 vol, f64 tgt, s32 count, u16* ratel);
#ifndef N_MICRO
static f32 _getVol(f32 ivol, s32 samples, s16 ratem, u16 ratel);
#else
static s16 _getVol(s16 ivol, s32 samples, s16 ratem, u16 ratel);
#endif

/*
 * Pull entry point of the envelope-mixer filter: emits one frame of RSP
 * commands for `filter`, walking the voice's pending parameter-event list and
 * cutting the frame into sub-frames at each event's sample delta. Each event
 * (start/stop/free voice, or a volume/pan/fx/pitch/wavetable change) is applied
 * between sub-frames; the remainder of the frame is emitted once the list
 * drains. `sampleOffset` is the frame's start time in samples. Returns the
 * advanced command-list pointer.
 */
Acmd* n_alEnvmixerPull(N_PVoice* filter, s32 sampleOffset, Acmd* p) {
  Acmd* ptr = p;
  N_PVoice* e = filter;
  s16 inp;
  s32 lastOffset;
  s32 thisOffset = sampleOffset;
  s32 samples;
  s16 loutp = 0;
  s32 fVol;
  ALParam* thisParam;
  s32 outCount = FIXED_SAMPLE; /* samples of this frame still to emit */
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif

  /* DMEM offset of the resampler's output, i.e. this stage's input buffer. */
#ifndef N_MICRO
  inp = AL_RESAMPLER_OUT;
#else
  inp = N_AL_RESAMPLER_OUT;
#endif

  /* Consume queued events in time order, emitting the audio between them. */
  while (e->em_ctrlList != 0) {
    /* Span (in samples) from the previous event to this one. FINAL_ROUND snaps
     * the span to a whole number of 184-sample frames; a zero span keeps the
     * clock at the previous event so its time is not lost. */
    lastOffset = thisOffset;
    thisOffset = e->em_ctrlList->delta;
#ifdef FINAL_ROUND
    samples = SAMPLE184(thisOffset - lastOffset);
    if (!samples) {
      thisOffset = lastOffset;
    }
#else
    samples = thisOffset - lastOffset;
#endif
    /* Event lands past the end of this frame: defer it to a later Pull. */
    if (samples > outCount) {
      break;
    }
#ifdef _DEBUG
    assert(samples >= 0);
#ifndef N_MICRO
    assert(samples <= AL_MAX_RSP_SAMPLES);
#else
    assert(samples <= FIXED_SAMPLE);
#endif
#endif

    /* Apply the event at the head of the control list. */
    switch (e->em_ctrlList->type) {
      /* Begin a note (full parameter set): load the wavetable and seed the
       * envelope, pan, and fx state from the start parameters. */
      case (AL_FILTER_START_VOICE_ALT): {
        ALStartParamAlt* param = (ALStartParamAlt*)e->em_ctrlList;
        ALFilter* f = (ALFilter*)e;
        s32 tmp;

        /* Flattened voice: start playing and load the wavetable via direct
         * field writes and a loader call, not chained setParam stages. */
        if (param->unity) {
          e->rs_upitch = 1;
        }
        n_alLoadParam(e, AL_FILTER_SET_WAVETABLE, param->wave);
        e->em_motion = AL_PLAYING;
        e->em_first = 1;
        e->em_delta = 0;
#ifdef FINAL_ROUND
        e->em_segEnd = SAMPLE184(param->samples);
#else
        e->em_segEnd = param->samples;
#endif

        /* Square the requested volume (Q15 * Q15 >> 15 stays Q15): a perceptual
         * taper so a linear control acts roughly logarithmically. */
        tmp = ((s32)param->volume * (s32)param->volume) >> 15;
        e->em_volume = (s16)tmp;

        e->em_pan = param->pan;
        /* Dry/wet reverb send taken off the equal-power curve. */
        e->em_dryamt = n_eqpower[param->fxMix];
        e->em_wetamt = n_eqpower[N_EQPOWER_LENGTH - param->fxMix - 1];

        /* With a ramp duration (samples) start near silence and let the
         * envelope ramp up; otherwise snap the L/R volumes to the panned
         * target now. */
        if (param->samples) {
          e->em_cvolL = 1;
          e->em_cvolR = 1;
        } else {
          e->em_cvolL = (e->em_volume * n_eqpower[e->em_pan]) >> 15;
          e->em_cvolR =
              (e->em_volume * n_eqpower[N_EQPOWER_LENGTH - e->em_pan - 1]) >>
              15;
        }

        /* Resampler playback ratio (pitch) for this note. */
        e->rs_ratio = param->pitch;
      } break;

      /* Volume / pan / fx change mid-note: flush audio up to this event, settle
       * the current volume to where the in-flight ramp has reached, then apply
       * the new parameter and re-arm a fresh ramp (em_first). */
      case (AL_FILTER_SET_FXAMT):
      case (AL_FILTER_SET_PAN):
      case (AL_FILTER_SET_VOLUME):
        ptr = _pullSubFrame(e, &inp, &loutp, samples, ptr);

        /* Bring em_cvolL/R up to date: snap to the target if the segment is
         * over, else project the partial ramp forward over the elapsed delta.
         */
        if (e->em_delta >= e->em_segEnd) {
          e->em_ltgt = (e->em_volume * n_eqpower[e->em_pan]) >> 15;
          e->em_rtgt =
              (e->em_volume * n_eqpower[N_EQPOWER_LENGTH - e->em_pan - 1]) >>
              15;
          e->em_delta = e->em_segEnd;
          e->em_cvolL = e->em_ltgt;
          e->em_cvolR = e->em_rtgt;
        } else {
          e->em_cvolL =
              _getVol(e->em_cvolL, e->em_delta, e->em_lratm, e->em_lratl);
          e->em_cvolR =
              _getVol(e->em_cvolR, e->em_delta, e->em_rratm, e->em_rratl);
        }

        /* Clamp to >=1: a zero current volume would stall the ramp solve. */
        if (e->em_cvolL == 0) {
          e->em_cvolL = 1;
        }
        if (e->em_cvolR == 0) {
          e->em_cvolR = 1;
        }

        /* Apply just the field this event changes (pan, squared volume target
         * plus a fresh ramp length, or dry/wet fx send). */
        if (e->em_ctrlList->type == AL_FILTER_SET_PAN) {
          e->em_pan = (s16)e->em_ctrlList->data.i;
        }
        if (e->em_ctrlList->type == AL_FILTER_SET_VOLUME) {
          /* New target volume (squared, Q15) over a new ramp length. */
          e->em_delta = 0;
          fVol = (e->em_ctrlList->data.i);
          fVol = (fVol * fVol) >> 15;
          e->em_volume = (s16)fVol;
#ifdef FINAL_ROUND
          e->em_segEnd = SAMPLE184(e->em_ctrlList->moredata.i);
#else
          e->em_segEnd = e->em_ctrlList->moredata.i;
#endif
        }
        if (e->em_ctrlList->type == AL_FILTER_SET_FXAMT) {
          e->em_dryamt = n_eqpower[e->em_ctrlList->data.i];
          e->em_wetamt =
              n_eqpower[N_EQPOWER_LENGTH - e->em_ctrlList->data.i - 1];
        }

        /* Re-arm: the next sub-frame re-solves the ramp from the settled
         * volume. */
        e->em_first = 1;
        break;

      /* Begin a note (simple parameter set): load the wavetable, mark playing.
       */
      case (AL_FILTER_START_VOICE): {
        ALStartParam* p = (ALStartParam*)e->em_ctrlList;
        if (p->unity) {
          e->rs_upitch = 1;
        }
        n_alLoadParam(e, AL_FILTER_SET_WAVETABLE, p->wave);
        e->em_motion = AL_PLAYING;
      } break;

      /* End the note: flush remaining audio, then reset to a silent idle. */
      case (AL_FILTER_STOP_VOICE): {
        ptr = _pullSubFrame(e, &inp, &loutp, samples, ptr);
        n_alEnvmixerParam(e, AL_FILTER_RESET, 0);
      } break;

      /* Release the physical voice back to the free pool. */
      case (AL_FILTER_FREE_VOICE): {
        N_ALFreeParam* param = (N_ALFreeParam*)e->em_ctrlList;
        param->pvoice->offset = 0;
        _n_freePVoice((N_PVoice*)param->pvoice);
      } break;

      /* Pitch / wavetable changes: flush audio, then update the resampler. */
      case (AL_FILTER_SET_PITCH):
        ptr = _pullSubFrame(e, &inp, &loutp, samples, ptr);
        e->rs_ratio = e->em_ctrlList->data.f;
        break;
      case (AL_FILTER_SET_UNITY_PITCH):
        ptr = _pullSubFrame(e, &inp, &loutp, samples, ptr);
        e->rs_upitch = 1;
        break;
      case (AL_FILTER_SET_WAVETABLE):
        ptr = _pullSubFrame(e, &inp, &loutp, samples, ptr);
        n_alLoadParam(e, AL_FILTER_SET_WAVETABLE,
                      (void*)e->em_ctrlList->data.i);
        break;

      /* Any other parameter: flush audio, forward to the loader. */
      default:
        ptr = _pullSubFrame(e, &inp, &loutp, samples, ptr);
        n_alEnvmixerParam(e, e->em_ctrlList->type,
                          (void*)e->em_ctrlList->data.i);
        break;
    }

    /* Advance the output cursor (<<1 = 2 bytes per s16 sample), shrink the
     * frame budget, then pop and free this event. */
    loutp += (samples << 1);
    outCount -= samples;
    thisParam = e->em_ctrlList;
    e->em_ctrlList = e->em_ctrlList->next;
    if (e->em_ctrlList == 0) {
      e->em_ctrlTail = 0;
    }
    _n_freeParam(thisParam);
  }

  /* Emit the rest of the frame past the last event, then clamp the envelope
   * clock to the segment end. */
  ptr = _pullSubFrame(e, &inp, &loutp, outCount, ptr);
  if (e->em_delta > e->em_segEnd) {
    e->em_delta = e->em_segEnd;
  }
#ifdef AUD_PROFILE
  PROFILE_AUD(env_num, env_cnt, env_max, env_min);
#endif
  return ptr;
}

/*
 * Parameter/command handler for the envelope mixer. AL_FILTER_ADD_UPDATE
 * appends a deferred event to the tail of the voice's control list (applied by
 * the next Pull); AL_FILTER_RESET clears the envelope to a stopped, silent
 * state and resets the resampler; AL_FILTER_START marks the voice playing;
 * anything else is forwarded to the loader. Always returns 0.
 */
s32 n_alEnvmixerParam(N_PVoice* filter, s32 paramID, void* param) {
  N_PVoice* e = filter;
  switch (paramID) {
    /* Enqueue a future update at the tail of the event list. */
    case (AL_FILTER_ADD_UPDATE):
      if (e->em_ctrlTail) {
        e->em_ctrlTail->next = (ALParam*)param;
      } else {
        e->em_ctrlList = (ALParam*)param;
      }
      e->em_ctrlTail = (ALParam*)param;
      break;

    /* Silence the voice and rewind the envelope/resampler to idle. */
    case (AL_FILTER_RESET):
      e->em_first = 1;
      e->em_motion = AL_STOPPED;
      e->em_volume = 1;
      e->em_segEnd = 0;
      e->rs_delta = 0.0;
      e->rs_first = 1;
      e->rs_upitch = 0;
      n_alLoadParam(e, AL_FILTER_RESET, param);
      break;

    /* Mark the voice playing; the next Pull emits its envelope. */
    case (AL_FILTER_START):
      e->em_motion = AL_PLAYING;
      break;

    /* Forward any other op to the loader (resampler) stage. */
    default:
      n_alLoadParam(e, paramID, param);
      break;
  }
  return 0;
}

/*
 * Emits the RSP commands for one fixed (184-sample) sub-frame: pull resampled
 * input from the source voice, then apply the volume envelope and pan. On the
 * first sub-frame of a segment (em_first) it solves the L/R ramp rates from the
 * current volume to the panned target (via _getRate) and emits the A_INIT
 * envelope setup; later sub-frames emit A_CONTINUE, resuming from saved
 * envelope state. Advances *inp and the envelope clock em_delta by one frame.
 * A no-op while the voice is not AL_PLAYING or when outCount is 0.
 */
static Acmd* _pullSubFrame(N_PVoice* filter, s16* inp, s16* outp, s32 outCount,
                           Acmd* p) {
  Acmd* ptr = p;
  N_PVoice* e = filter;
#ifndef N_MICRO
  s16* inpp = AL_RESAMPLER_OUT;
#else
  s16* inpp = N_AL_RESAMPLER_OUT;
#endif

  if (e->em_motion != AL_PLAYING || !outCount) {
    return ptr;
  }

  /* Pull the resampled sub-frame this stage will scale and mix. */
  ptr = n_alResamplePull(e, inp, p);
#ifndef N_MICRO
  aSetBuffer(ptr++, A_MAIN, *inp, AL_MAIN_L_OUT, FIXED_SAMPLE << 1);
  aSetBuffer(ptr++, A_AUX, AL_MAIN_R_OUT, AL_AUX_L_OUT, AL_AUX_R_OUT);
#endif

  if (e->em_first) {
    /* First sub-frame of a segment: solve the L/R per-step ramp rates from the
     * current volume (em_cvolL/R) to the panned segment target (em_ltgt/rtgt)
     * over em_segEnd samples, then emit the envelope-init commands. */
    e->em_first = 0;
    e->em_ltgt = (e->em_volume * n_eqpower[e->em_pan]) >> 15;
    e->em_lratm = _getRate((f64)e->em_cvolL, (f64)e->em_ltgt, e->em_segEnd,
                           &(e->em_lratl));
    e->em_rtgt =
        (e->em_volume * n_eqpower[N_EQPOWER_LENGTH - e->em_pan - 1]) >> 15;
    e->em_rratm = _getRate((f64)e->em_cvolR, (f64)e->em_rtgt, e->em_segEnd,
                           &(e->em_rratl));

#ifndef N_MICRO
    aSetVolume(ptr++, A_LEFT | A_VOL, e->em_cvolL, 0, 0);
    aSetVolume(ptr++, A_RIGHT | A_VOL, e->em_cvolR, 0, 0);
    aSetVolume(ptr++, A_LEFT | A_RATE, e->em_ltgt, e->em_lratm, e->em_lratl);
    aSetVolume(ptr++, A_RIGHT | A_RATE, e->em_rtgt, e->em_rratm, e->em_rratl);
    aSetVolume(ptr++, A_AUX, e->em_dryamt, 0, e->em_wetamt);
    aEnvMixer(ptr++, A_INIT | A_AUX, osVirtualToPhysical(e->em_state));
  } else
    aEnvMixer(ptr++, A_CONTINUE | A_AUX, osVirtualToPhysical(e->em_state));
#else
    /* Micro command stream: same init / continue split, packed into the naudio
     * n_aSetVolume / n_aEnvMixer commands (see inc/n_env_add01.inc.c). */
#include "inc/n_env_add01.inc"
#endif

  /* Advance one frame: *inp by 184 samples (<<1 = 2 bytes each), clock by 184.
   */
  *inp += (FIXED_SAMPLE << 1);
  e->em_delta += FIXED_SAMPLE;
  return ptr;
}

/*
 * Solves the per-step volume ramp from `vol` to `tgt` over `count` samples. The
 * envelope mixer advances the volume once every 8 samples; this returns the
 * signed integer step (the rate mantissa) and writes the fractional part as a
 * Q16 value to *ratel, so one step equals (return + *ratel / 65536). count == 0
 * returns the max (rising) or min (falling) slew. The two definitions select on
 * N_MICRO; the build uses the micro variant below.
 *
 * Non-micro variant: a geometric (multiplicative) factor, from a small log
 * lookup plus exp-by-squaring.
 */
#ifndef N_MICRO
#define EXP_MASK 0x7f800000
#define MANT_MASK 0x807fffff
static s16 _getRate(f64 vol, f64 tgt, s32 count, u16* ratel) {
  s16 s;
  f64 invn = 1.0 / count;
  f64 eps;
  f64 a;
  f64 fs;
  f64 mant;
  s32 i_invn;
  s32 ex;
  s32 indx;
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif

  if (count == 0) {
    if (tgt >= vol) {
      *ratel = 0xffff;
      return 0x7fff;
    } else {
      *ratel = 0;
      return 0;
    }
  }

  /* Clamp the ramp endpoints to >=1 so the divide and log stay well-defined. */
  if (tgt < 1.0) tgt = 1.0;
  if (vol <= 0) vol = 1;
/* NBITS: log-table index bits; NFRACBITS: Q30 fixed-point fraction; M_LN2:
 * ln(2), to convert the base-2 mantissa/exponent into a natural log. */
#define NBITS (3)
#define NPOS (1 << NBITS)
#define NFRACBITS (30)
#define M_LN2 0.69314718055994530942

  {
    /* Per-step factor fs ~= (tgt/vol)^(1/count): read ln(tgt/vol) from the log
     * table, scale by 1/count, then raise e^eps to the i_invn power by
     * repeated squaring. */
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
      if (i_invn & 1) a = a * fs;
      fs *= fs;
      i_invn >>= 1;
    }
  }

  /* Raise to the 8th power: the rate is applied once per 8-sample step. */
  a *= (a *= (a *= a));

  s = (s16)a;
  *ratel = (s16)(0xffff * (a - (f32)s));
#ifdef AUD_PROFILE
  PROFILE_AUD(rate_num, rate_cnt, rate_max, rate_min);
#endif
  return (s16)a;
}
/* Micro variant (built): a linear (additive) per-8-sample step. */
#else
  static s16 _getRate(f64 vol, f64 tgt, s32 count, u16 * ratel) {
    s16 s;
    s16 tmp;
    f64 invn;
    f64 a;
    f64 f;
#ifdef AUD_PROFILE
    lastCnt[++cnt_index] = osGetCount();
#endif

    if (count == 0) {
      if (tgt >= vol) {
        *ratel = 0xffff;
        return 0x7fff;
      }
      *ratel = 0;
      return 0x8000;
    }

    invn = 1.0 / count;
    if (tgt < 1.0) {
      tgt = 1.0;
    }
    if (vol <= 0.0) {
      vol = 1.0;
    }

    /* Volume change per 8-sample step. */
    a = (tgt - vol) * invn * 8.0;

    /* Split a into floor(a) and a fraction in [0,1): truncate, then shift down
     * one and re-floor so the fraction stays non-negative even when ramping
     * down (a < 0). s is the integer step; f becomes the Q16 fraction below. */
    s = (s16)a;
    f = a - (f64)s;
    s -= 1;
    f += 1.0;
    tmp = (s16)f;
    s += tmp;
    f -= (f64)tmp;
#ifdef AUD_PROFILE
    PROFILE_AUD(rate_num, rate_cnt, rate_max, rate_min);
#endif

    *ratel = (u16)(0xffff * f);
    return s;
  }
#endif

/*
 * Projects the volume `ivol` forward over `samples` samples at the ramp rate
 * (ratem + ratel/65536 per 8-sample step); used to catch the current volume up
 * to where an in-flight ramp has reached. Returns ivol unchanged if fewer than
 * 8 samples have elapsed. Like _getRate, the two definitions select on N_MICRO.
 *
 * Non-micro variant: applies the rate geometrically (ivol *= rate^steps), the
 * factor raised by exp-by-squaring.
 */
#ifndef N_MICRO
static f32 _getVol(f32 ivol, s32 samples, s16 ratem, u16 ratel) {
  f32 r;
  f32 a;
  s32 i;
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif

  /* samples -> number of 8-sample steps. */
  samples >>= 3;
  if (samples == 0) {
    return ivol;
  }

  r = ((f32)(ratem << 16) + (f32)ratel) / 65536;
  a = 1.0;

  for (i = 0; i < 32; i++) {
    if (samples & 1) a *= r;
    samples >>= 1;
    if (samples == 0) break;
    r *= r;
  }

  ivol *= a;
#ifdef AUD_PROFILE
  PROFILE_AUD(vol_num, vol_cnt, vol_max, vol_min);
#endif
  return ivol;
}
/* Micro variant (built): adds rate * steps linearly, accumulating the Q16
 * fraction separately and shifting it down. */
#else
  static s16 _getVol(s16 ivol, s32 samples, s16 ratem, u16 ratel) {
    s32 tmp1;
#ifdef AUD_PROFILE
    lastCnt[++cnt_index] = osGetCount();
#endif

    /* samples -> number of 8-sample steps. */
    samples >>= 3;
    if (samples == 0) {
      return ivol;
    }

    tmp1 = ratel * samples;
    tmp1 >>= 16;
    tmp1 += ratem * samples;
    ivol += tmp1;
#ifdef AUD_PROFILE
    PROFILE_AUD(vol_num, vol_cnt, vol_max, vol_min);
#endif
    return ivol;
  }
#endif
