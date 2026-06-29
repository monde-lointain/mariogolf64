/*
 * n_resample.c
 *
 * n_audio (microcode-variant) resampler / pitch-shift filter. Pulls decoded
 * samples from the ADPCM load filter and time-stretches them to the playback
 * rate using a fixed-point pitch ratio (UNITY_PITCH represents 1.0), carrying
 * the fractional sample position across command-list frames in rs_delta so
 * pitch does not drift. At unity pitch the resample step is skipped and the
 * decoded samples pass straight through. n_alResampleParam handles the pitch
 * and reset parameters and defers everything else to the load filter.
 */

#include "n_synthInternals.h"
#include <os.h>

/* Profiling counters; compiled in only for instrumented AUD_PROFILE builds. */
#ifdef AUD_PROFILE
extern u32 cnt_index, resampler_num, resampler_cnt, resampler_max,
    resampler_min, lastCnt[];
#endif

/*
 * Builds the resampler's portion of the command list: asks the ADPCM load
 * filter for enough input samples, then emits an aResample command that
 * stretches them into FIXED_SAMPLE output samples at *outp. The input-sample
 * count follows the pitch ratio, with the leftover fraction accumulated in
 * rs_delta so pitch stays drift-free across frames. Returns the advanced
 * command pointer.
 */
Acmd* n_alResamplePull(N_PVoice* e, s16* outp, Acmd* p) {
  Acmd* ptr = p;
  s16 inp;
  s32 inCount;
  s32 incr;
  f32 finCount;
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif
  // Decoded input arrives in the decoder-output DMEM buffer.
#ifndef N_MICRO
  inp = AL_DECODER_OUT;
#else
  inp = N_AL_DECODER_OUT;
#endif

  // Unity-pitch fast path: no rate conversion, just copy the decoded samples
  // straight to the output buffer.
  if (e->rs_upitch) {
    ptr = n_alAdpcmPull(e, &inp, FIXED_SAMPLE, p);
    aDMEMMove(ptr++, inp, *outp, FIXED_SAMPLE << 1);
  } else {
    // General case: convert the decoded samples to the output rate. Clamp the
    // pitch to at most +1 octave first.
    if (e->rs_ratio > MAX_RATIO) {
      e->rs_ratio = MAX_RATIO;
    }

    // Quantize the ratio onto the resampler's fixed-point pitch grid.
    e->rs_ratio = (s32)(e->rs_ratio * UNITY_PITCH);
    e->rs_ratio = e->rs_ratio / UNITY_PITCH;

    // Input samples needed = ratio * output count plus the fraction carried
    // from the previous frame; keep the new remainder in rs_delta.
    finCount = e->rs_delta + (e->rs_ratio * (f32)FIXED_SAMPLE);
    inCount = (s32)finCount;
    e->rs_delta = finCount - (f32)inCount;

    // Pull exactly that many decoded samples from the load filter.
    ptr = n_alAdpcmPull(e, &inp, inCount, p);

    // Emit the resample command. incr is the per-sample pitch step in
    // UNITY_PITCH fixed point.
    incr = (s32)(e->rs_ratio * UNITY_PITCH);
#ifndef N_MICRO
    aSetBuffer(ptr++, 0, inp, *outp, FIXED_SAMPLE << 1);
    aResample(ptr++, e->rs_first, incr, osVirtualToPhysical(e->rs_state));
#else
#include "inc/n_resample_add01.inc"
#endif
    e->rs_first = 0;
  }
#ifdef AUD_PROFILE
  PROFILE_AUD(resampler_num, resampler_cnt, resampler_max, resampler_min);
#endif
  return ptr;
}

/*
 * Resampler parameter handler. In this build every parameter is forwarded to
 * the load filter (n_alLoadParam). Always returns 0.
 */
s32 n_alResampleParam(N_PVoice* filter, s32 paramID, void* param) {
  switch (paramID) {
    default:
      n_alLoadParam(filter, paramID, param);
      break;
  }
  return 0;
}
